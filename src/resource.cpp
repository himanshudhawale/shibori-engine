#include <shibori/engine/resource.hpp>

#include <array>
#include <exception>
#include <mutex>
#include <new>
#include <string>
#include <utility>

namespace shibori::engine {
namespace detail {

class ResourceBudgetState {
 public:
  ResourceBudgetState(ResourceKind kind_value, std::uint64_t capacity_value)
      : kind(kind_value), capacity(capacity_value) {}

  ~ResourceBudgetState() {
    if (parent && parent_amount != 0) {
      std::scoped_lock lock(parent->mutex);
      if (parent_amount > parent->used) {
        std::terminate();
      }
      parent->used -= parent_amount;
    }
  }

  ResourceKind kind;
  std::uint64_t capacity;
  std::uint64_t used = 0;
  std::mutex mutex;
  std::shared_ptr<ResourceBudgetState> parent;
  std::uint64_t parent_amount = 0;
};

}  // namespace detail
namespace {

Status invalid_limit(std::string_view name) {
  return std::unexpected(Error(
      ErrorCode::invalid_resource_limit,
      Operation::configure,
      std::string(name) + " must be greater than zero"));
}

Result<std::shared_ptr<detail::ResourceBudgetState>> allocate_state(
    ResourceKind kind,
    std::uint64_t capacity,
    Operation operation) {
  try {
    return std::make_shared<detail::ResourceBudgetState>(kind, capacity);
  } catch (const std::bad_alloc&) {
    return std::unexpected(Error(
        ErrorCode::allocation_failed,
        operation,
        "unable to allocate resource budget state"));
  }
}

}  // namespace

std::string_view to_string(ResourceKind kind) noexcept {
  switch (kind) {
    case ResourceKind::resident_memory:
      return "resident_memory";
    case ResourceKind::block_rows:
      return "block_rows";
    case ResourceKind::decoded_block_bytes:
      return "decoded_block_bytes";
    case ResourceKind::record_bytes:
      return "record_bytes";
    case ResourceKind::metadata_bytes:
      return "metadata_bytes";
    case ResourceKind::fields:
      return "fields";
    case ResourceKind::encoding_depth:
      return "encoding_depth";
    case ResourceKind::workers:
      return "workers";
    case ResourceKind::queued_blocks:
      return "queued_blocks";
  }
  return "resident_memory";
}

ResourceLimits default_resource_limits() noexcept {
  constexpr std::uint64_t mebibyte = 1024 * 1024;
  return ResourceLimits{
      .maximum_resident_bytes = 512 * mebibyte,
      .maximum_block_rows = 1'000'000,
      .maximum_decoded_block_bytes = 64 * mebibyte,
      .maximum_record_bytes = 128 * mebibyte,
      .maximum_metadata_bytes = 16 * mebibyte,
      .maximum_fields = 4096,
      .maximum_encoding_depth = 8,
      .maximum_workers = 8,
      .maximum_queued_blocks = 4,
  };
}

Status validate_resource_limits(const ResourceLimits& limits) {
  const std::array<std::pair<std::string_view, std::uint64_t>, 9> values{{
      {"maximum_resident_bytes", limits.maximum_resident_bytes},
      {"maximum_block_rows", limits.maximum_block_rows},
      {"maximum_decoded_block_bytes", limits.maximum_decoded_block_bytes},
      {"maximum_record_bytes", limits.maximum_record_bytes},
      {"maximum_metadata_bytes", limits.maximum_metadata_bytes},
      {"maximum_fields", limits.maximum_fields},
      {"maximum_encoding_depth", limits.maximum_encoding_depth},
      {"maximum_workers", limits.maximum_workers},
      {"maximum_queued_blocks", limits.maximum_queued_blocks},
  }};

  for (const auto& [name, value] : values) {
    if (value == 0) {
      return invalid_limit(name);
    }
  }

  if (limits.maximum_decoded_block_bytes > limits.maximum_resident_bytes) {
    return std::unexpected(Error(
        ErrorCode::invalid_resource_limit,
        Operation::configure,
        "maximum_decoded_block_bytes exceeds maximum_resident_bytes"));
  }
  if (limits.maximum_record_bytes > limits.maximum_resident_bytes) {
    return std::unexpected(Error(
        ErrorCode::invalid_resource_limit,
        Operation::configure,
        "maximum_record_bytes exceeds maximum_resident_bytes"));
  }
  if (limits.maximum_metadata_bytes > limits.maximum_record_bytes) {
    return std::unexpected(Error(
        ErrorCode::invalid_resource_limit,
        Operation::configure,
        "maximum_metadata_bytes exceeds maximum_record_bytes"));
  }
  return {};
}

ResourceReservation::ResourceReservation(
    std::shared_ptr<detail::ResourceBudgetState> state,
    std::uint64_t amount) noexcept
    : state_(std::move(state)), amount_(amount) {}

ResourceReservation::~ResourceReservation() {
  release();
}

ResourceReservation::ResourceReservation(
    ResourceReservation&& other) noexcept
    : state_(std::move(other.state_)), amount_(std::exchange(other.amount_, 0)) {}

ResourceReservation& ResourceReservation::operator=(
    ResourceReservation&& other) noexcept {
  if (this != &other) {
    release();
    state_ = std::move(other.state_);
    amount_ = std::exchange(other.amount_, 0);
  }
  return *this;
}

bool ResourceReservation::active() const noexcept {
  return state_ != nullptr;
}

std::uint64_t ResourceReservation::amount() const noexcept {
  return amount_;
}

void ResourceReservation::release() noexcept {
  if (!state_) {
    return;
  }

  {
    std::scoped_lock lock(state_->mutex);
    if (amount_ > state_->used) {
      std::terminate();
    }
    state_->used -= amount_;
  }

  state_.reset();
  amount_ = 0;
}

ResourceBudget::ResourceBudget(
    std::shared_ptr<detail::ResourceBudgetState> state) noexcept
    : state_(std::move(state)) {}

Result<ResourceBudget> ResourceBudget::create_root(
    ResourceKind kind,
    std::uint64_t capacity) {
  if (capacity == 0) {
    return std::unexpected(Error(
        ErrorCode::invalid_resource_limit,
        Operation::configure,
        "root resource capacity must be greater than zero"));
  }

  auto state = allocate_state(kind, capacity, Operation::configure);
  if (!state) {
    return std::unexpected(std::move(state.error()));
  }
  return ResourceBudget(std::move(*state));
}

Result<ResourceBudget> ResourceBudget::create_child(
    std::uint64_t capacity,
    Operation operation) const {
  if (capacity == 0) {
    return std::unexpected(Error(
        ErrorCode::invalid_resource_limit,
        operation,
        "child resource capacity must be greater than zero"));
  }

  auto child = allocate_state(state_->kind, capacity, operation);
  if (!child) {
    return std::unexpected(std::move(child.error()));
  }

  std::scoped_lock lock(state_->mutex);
  if (capacity > state_->capacity - state_->used) {
    return std::unexpected(Error(
        ErrorCode::limit_exceeded,
        operation,
        std::string(to_string(state_->kind)) +
            " cannot fund the requested child budget"));
  }

  state_->used += capacity;
  (*child)->parent = state_;
  (*child)->parent_amount = capacity;
  return ResourceBudget(std::move(*child));
}

Result<ResourceReservation> ResourceBudget::reserve(
    std::uint64_t amount,
    Operation operation) const {
  if (amount == 0) {
    return std::unexpected(Error(
        ErrorCode::invalid_resource_limit,
        operation,
        "resource reservation must be greater than zero"));
  }

  std::scoped_lock lock(state_->mutex);
  if (amount > state_->capacity - state_->used) {
    return std::unexpected(Error(
        ErrorCode::limit_exceeded,
        operation,
        std::string(to_string(state_->kind)) + " budget is exhausted"));
  }

  state_->used += amount;
  return ResourceReservation(state_, amount);
}

ResourceKind ResourceBudget::kind() const noexcept {
  return state_->kind;
}

std::uint64_t ResourceBudget::capacity() const noexcept {
  return state_->capacity;
}

std::uint64_t ResourceBudget::used() const noexcept {
  std::scoped_lock lock(state_->mutex);
  return state_->used;
}

std::uint64_t ResourceBudget::available() const noexcept {
  std::scoped_lock lock(state_->mutex);
  return state_->capacity - state_->used;
}

}  // namespace shibori::engine
