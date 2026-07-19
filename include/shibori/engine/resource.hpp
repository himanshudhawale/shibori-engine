#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include <shibori/engine/export.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine {

enum class ResourceKind : std::uint8_t {
  resident_memory,
  block_rows,
  decoded_block_bytes,
  record_bytes,
  metadata_bytes,
  fields,
  encoding_depth,
  workers,
  queued_blocks,
};

[[nodiscard]] SHIBORI_ENGINE_API std::string_view to_string(
    ResourceKind kind) noexcept;

struct ResourceLimits {
  std::uint64_t maximum_resident_bytes;
  std::uint64_t maximum_block_rows;
  std::uint64_t maximum_decoded_block_bytes;
  std::uint64_t maximum_record_bytes;
  std::uint64_t maximum_metadata_bytes;
  std::uint32_t maximum_fields;
  std::uint32_t maximum_encoding_depth;
  std::uint32_t maximum_workers;
  std::uint32_t maximum_queued_blocks;
};

[[nodiscard]] SHIBORI_ENGINE_API ResourceLimits default_resource_limits()
    noexcept;
[[nodiscard]] SHIBORI_ENGINE_API Status validate_resource_limits(
    const ResourceLimits& limits);

namespace detail {
class ResourceBudgetState;
}

class ResourceReservation {
 public:
  SHIBORI_ENGINE_API ~ResourceReservation();

  ResourceReservation(const ResourceReservation&) = delete;
  ResourceReservation& operator=(const ResourceReservation&) = delete;
  SHIBORI_ENGINE_API ResourceReservation(
      ResourceReservation&& other) noexcept;
  SHIBORI_ENGINE_API ResourceReservation& operator=(
      ResourceReservation&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API bool active() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t amount() const noexcept;
  SHIBORI_ENGINE_API void release() noexcept;

 private:
  friend class ResourceBudget;

  ResourceReservation(
      std::shared_ptr<detail::ResourceBudgetState> state,
      std::uint64_t amount) noexcept;

  std::shared_ptr<detail::ResourceBudgetState> state_;
  std::uint64_t amount_;
};

class ResourceBudget {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<ResourceBudget> create_root(
      ResourceKind kind,
      std::uint64_t capacity);

  [[nodiscard]] SHIBORI_ENGINE_API Result<ResourceBudget> create_child(
      std::uint64_t capacity,
      Operation operation) const;

  [[nodiscard]] SHIBORI_ENGINE_API Result<ResourceReservation> reserve(
      std::uint64_t amount,
      Operation operation) const;

  [[nodiscard]] SHIBORI_ENGINE_API ResourceKind kind() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t capacity() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t used() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t available() const noexcept;

 private:
  explicit ResourceBudget(
      std::shared_ptr<detail::ResourceBudgetState> state) noexcept;

  std::shared_ptr<detail::ResourceBudgetState> state_;
};

}  // namespace shibori::engine
