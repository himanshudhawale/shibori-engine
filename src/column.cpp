#include <shibori/engine/column.hpp>

#include <bit>
#include <limits>
#include <new>
#include <utility>

namespace shibori::engine {
namespace {

Status validate_validity(
    std::uint64_t row_count,
    const std::optional<Validity>& validity) {
  if (validity && validity->row_count() != row_count) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "validity row count does not match column row count");
  }
  return {};
}

bool is_fixed_storage_type(const LogicalType& type) noexcept {
  return type.is_fixed_width() &&
         type.kind() != LogicalTypeKind::boolean;
}

}  // namespace

class ByteBuffer::Impl {
 public:
  using value_type = std::byte;
  Impl(
      std::shared_ptr<const std::vector<std::byte>> storage_value,
      BufferOwnership ownership_value)
      : storage(std::move(storage_value)), ownership(ownership_value) {}
  std::shared_ptr<const std::vector<std::byte>> storage;
  BufferOwnership ownership;
};

Result<ByteBuffer> ByteBuffer::copy(std::span<const std::byte> bytes) {
  try {
    auto storage = std::make_shared<const std::vector<std::byte>>(
        bytes.begin(), bytes.end());
    return ByteBuffer(std::make_shared<const Impl>(
        std::move(storage),
        BufferOwnership::owned));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to copy byte buffer");
  }
}

Result<ByteBuffer> ByteBuffer::share(
    std::shared_ptr<const std::vector<std::byte>> bytes) {
  if (!bytes) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::validate_block,
        "shared byte buffer must not be null");
  }
  try {
    return ByteBuffer(std::make_shared<const Impl>(
        std::move(bytes),
        BufferOwnership::shared));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to retain shared byte buffer");
  }
}

ByteBuffer::ByteBuffer(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
ByteBuffer::~ByteBuffer() = default;
ByteBuffer::ByteBuffer(const ByteBuffer&) = default;
ByteBuffer& ByteBuffer::operator=(const ByteBuffer&) = default;
ByteBuffer::ByteBuffer(ByteBuffer&&) noexcept = default;
ByteBuffer& ByteBuffer::operator=(ByteBuffer&&) noexcept = default;
std::span<const std::byte> ByteBuffer::bytes() const noexcept {
  return *impl_->storage;
}
std::uint64_t ByteBuffer::size() const noexcept {
  return static_cast<std::uint64_t>(impl_->storage->size());
}
BufferOwnership ByteBuffer::ownership() const noexcept {
  return impl_->ownership;
}

class OffsetBuffer::Impl {
 public:
  using value_type = std::uint64_t;
  Impl(
      std::shared_ptr<const std::vector<std::uint64_t>> storage_value,
      BufferOwnership ownership_value)
      : storage(std::move(storage_value)), ownership(ownership_value) {}
  std::shared_ptr<const std::vector<std::uint64_t>> storage;
  BufferOwnership ownership;
};

Result<OffsetBuffer> OffsetBuffer::copy(
    std::span<const std::uint64_t> offsets) {
  try {
    auto storage = std::make_shared<const std::vector<std::uint64_t>>(
        offsets.begin(), offsets.end());
    return OffsetBuffer(std::make_shared<const Impl>(
        std::move(storage),
        BufferOwnership::owned));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to copy offset buffer");
  }
}

Result<OffsetBuffer> OffsetBuffer::share(
    std::shared_ptr<const std::vector<std::uint64_t>> offsets) {
  if (!offsets) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::validate_block,
        "shared offset buffer must not be null");
  }
  try {
    return OffsetBuffer(std::make_shared<const Impl>(
        std::move(offsets),
        BufferOwnership::shared));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to retain shared offset buffer");
  }
}

OffsetBuffer::OffsetBuffer(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
OffsetBuffer::~OffsetBuffer() = default;
OffsetBuffer::OffsetBuffer(const OffsetBuffer&) = default;
OffsetBuffer& OffsetBuffer::operator=(const OffsetBuffer&) = default;
OffsetBuffer::OffsetBuffer(OffsetBuffer&&) noexcept = default;
OffsetBuffer& OffsetBuffer::operator=(OffsetBuffer&&) noexcept = default;
std::span<const std::uint64_t> OffsetBuffer::values() const noexcept {
  return *impl_->storage;
}
std::uint64_t OffsetBuffer::size() const noexcept {
  return static_cast<std::uint64_t>(impl_->storage->size());
}
BufferOwnership OffsetBuffer::ownership() const noexcept {
  return impl_->ownership;
}

class Validity::Impl {
 public:
  Impl(
      std::uint64_t row_count_value,
      std::uint64_t null_count_value,
      ByteBuffer bitmap_value)
      : row_count(row_count_value),
        null_count(null_count_value),
        bitmap(std::move(bitmap_value)) {}
  std::uint64_t row_count;
  std::uint64_t null_count;
  ByteBuffer bitmap;
};

Result<Validity> Validity::from_bitmap(
    std::uint64_t row_count,
    ByteBuffer bitmap) {
  if (row_count > std::numeric_limits<std::uint64_t>::max() - 7) {
    return fail(
        ErrorCode::arithmetic_overflow,
        Operation::validate_block,
        "validity bitmap size overflow");
  }
  const auto required = (row_count + 7) / 8;
  if (bitmap.size() != required) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "validity bitmap has the wrong size");
  }
  const auto bytes = bitmap.bytes();
  if (row_count % 8 != 0 && !bytes.empty()) {
    const auto used = static_cast<unsigned>(row_count % 8);
    const auto padding_mask =
        static_cast<std::uint8_t>(0xffU << used);
    if ((std::to_integer<std::uint8_t>(bytes.back()) & padding_mask) != 0) {
      return fail(
          ErrorCode::invalid_block,
          Operation::validate_block,
          "validity padding bits must be zero");
    }
  }
  std::uint64_t valid_count = 0;
  for (const auto value : bytes) {
    valid_count += std::popcount(std::to_integer<std::uint8_t>(value));
  }
  try {
    return Validity(std::make_shared<const Impl>(
        row_count,
        row_count - valid_count,
        std::move(bitmap)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to retain validity bitmap");
  }
}

Validity::Validity(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
Validity::~Validity() = default;
Validity::Validity(const Validity&) = default;
Validity& Validity::operator=(const Validity&) = default;
Validity::Validity(Validity&&) noexcept = default;
Validity& Validity::operator=(Validity&&) noexcept = default;
std::uint64_t Validity::row_count() const noexcept { return impl_->row_count; }
std::uint64_t Validity::null_count() const noexcept {
  return impl_->null_count;
}
bool Validity::is_valid(std::uint64_t row) const noexcept {
  if (row >= impl_->row_count) {
    return false;
  }
  const auto value = std::to_integer<std::uint8_t>(
      impl_->bitmap.bytes()[static_cast<std::size_t>(row / 8)]);
  return (value & (1U << (row % 8))) != 0;
}
const ByteBuffer& Validity::bitmap() const noexcept { return impl_->bitmap; }

class ValidityBuilder::Impl {
 public:
  explicit Impl(std::uint64_t row_count_value) : row_count(row_count_value) {
    if (row_count <= std::numeric_limits<std::uint64_t>::max() - 7 &&
        (row_count + 7) / 8 <=
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
      bytes.resize(static_cast<std::size_t>((row_count + 7) / 8));
    } else {
      valid_size = false;
    }
  }
  std::uint64_t row_count;
  std::uint64_t appended = 0;
  std::vector<std::byte> bytes;
  bool valid_size = true;
  bool finished = false;
};

ValidityBuilder::ValidityBuilder(std::uint64_t row_count)
    : impl_(std::make_unique<Impl>(row_count)) {}
ValidityBuilder::~ValidityBuilder() = default;
ValidityBuilder::ValidityBuilder(ValidityBuilder&&) noexcept = default;
ValidityBuilder& ValidityBuilder::operator=(ValidityBuilder&&) noexcept =
    default;

Status ValidityBuilder::append(bool valid) {
  if (impl_->finished || impl_->appended >= impl_->row_count) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "validity builder cannot accept another row");
  }
  if (!impl_->valid_size) {
    return fail(
        ErrorCode::arithmetic_overflow,
        Operation::validate_block,
        "validity bitmap size overflow");
  }
  if (valid) {
    const auto index = static_cast<std::size_t>(impl_->appended / 8);
    const auto bit = static_cast<unsigned>(impl_->appended % 8);
    impl_->bytes[index] |= static_cast<std::byte>(1U << bit);
  }
  ++impl_->appended;
  return {};
}

Result<Validity> ValidityBuilder::finish() && {
  if (impl_->finished || impl_->appended != impl_->row_count) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "validity builder is incomplete or already finished");
  }
  impl_->finished = true;
  auto bitmap = ByteBuffer::copy(impl_->bytes);
  if (!bitmap) {
    return std::unexpected(std::move(bitmap.error()));
  }
  return Validity::from_bitmap(impl_->row_count, std::move(*bitmap));
}

class Column::Impl {
 public:
  Impl(
      LogicalType type_value,
      ColumnStorageKind kind_value,
      std::uint64_t row_count_value,
      std::optional<Validity> validity_value,
      std::optional<ByteBuffer> values_value,
      std::optional<OffsetBuffer> offsets_value,
      std::optional<ByteBuffer> payload_value)
      : type(std::move(type_value)),
        kind(kind_value),
        row_count(row_count_value),
        validity(std::move(validity_value)),
        values(std::move(values_value)),
        offsets(std::move(offsets_value)),
        payload(std::move(payload_value)) {}
  LogicalType type;
  ColumnStorageKind kind;
  std::uint64_t row_count;
  std::optional<Validity> validity;
  std::optional<ByteBuffer> values;
  std::optional<OffsetBuffer> offsets;
  std::optional<ByteBuffer> payload;
};

Column::Column(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
Column::~Column() = default;
Column::Column(const Column&) = default;
Column& Column::operator=(const Column&) = default;
Column::Column(Column&&) noexcept = default;
Column& Column::operator=(Column&&) noexcept = default;
const LogicalType& Column::type() const noexcept { return impl_->type; }
ColumnStorageKind Column::storage_kind() const noexcept { return impl_->kind; }
std::uint64_t Column::row_count() const noexcept { return impl_->row_count; }
std::uint64_t Column::null_count() const noexcept {
  return impl_->validity ? impl_->validity->null_count() : 0;
}
const Validity* Column::validity() const noexcept {
  return impl_->validity ? &*impl_->validity : nullptr;
}
const ByteBuffer* Column::values() const noexcept {
  return impl_->values ? &*impl_->values : nullptr;
}
const OffsetBuffer* Column::offsets() const noexcept {
  return impl_->offsets ? &*impl_->offsets : nullptr;
}
const ByteBuffer* Column::payload() const noexcept {
  return impl_->payload ? &*impl_->payload : nullptr;
}

class FixedWidthColumnBuilder::Impl {
 public:
  Impl(LogicalType type_value, std::uint64_t row_count_value)
      : type(std::move(type_value)), row_count(row_count_value) {}
  LogicalType type;
  std::uint64_t row_count;
  std::optional<Validity> validity;
  std::optional<ByteBuffer> values;
  bool finished = false;
};

FixedWidthColumnBuilder::FixedWidthColumnBuilder(
    LogicalType type,
    std::uint64_t row_count)
    : impl_(std::make_unique<Impl>(std::move(type), row_count)) {}
FixedWidthColumnBuilder::~FixedWidthColumnBuilder() = default;
FixedWidthColumnBuilder::FixedWidthColumnBuilder(
    FixedWidthColumnBuilder&&) noexcept = default;
FixedWidthColumnBuilder& FixedWidthColumnBuilder::operator=(
    FixedWidthColumnBuilder&&) noexcept = default;

Status FixedWidthColumnBuilder::set_validity(Validity validity) {
  if (impl_->finished || impl_->validity) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "fixed-width validity is already set");
  }
  impl_->validity = std::move(validity);
  return {};
}
Status FixedWidthColumnBuilder::set_values(ByteBuffer values) {
  if (impl_->finished || impl_->values) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "fixed-width values are already set");
  }
  impl_->values = std::move(values);
  return {};
}
Result<Column> FixedWidthColumnBuilder::finish() && {
  if (impl_->finished || !impl_->values ||
      !is_fixed_storage_type(impl_->type)) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "invalid or incomplete fixed-width column");
  }
  impl_->finished = true;
  if (auto status = validate_validity(impl_->row_count, impl_->validity);
      !status) {
    return std::unexpected(std::move(status.error()));
  }
  try {
    return Column(std::make_shared<const Column::Impl>(
        std::move(impl_->type),
        ColumnStorageKind::fixed_width,
        impl_->row_count,
        std::move(impl_->validity),
        std::move(impl_->values),
        std::nullopt,
        std::nullopt));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to allocate fixed-width column");
  }
}

class BooleanColumnBuilder::Impl {
 public:
  explicit Impl(std::uint64_t row_count_value) : row_count(row_count_value) {}
  std::uint64_t row_count;
  std::optional<Validity> validity;
  std::optional<ByteBuffer> values;
  bool finished = false;
};

BooleanColumnBuilder::BooleanColumnBuilder(std::uint64_t row_count)
    : impl_(std::make_unique<Impl>(row_count)) {}
BooleanColumnBuilder::~BooleanColumnBuilder() = default;
BooleanColumnBuilder::BooleanColumnBuilder(BooleanColumnBuilder&&) noexcept =
    default;
BooleanColumnBuilder& BooleanColumnBuilder::operator=(
    BooleanColumnBuilder&&) noexcept = default;
Status BooleanColumnBuilder::set_validity(Validity validity) {
  if (impl_->finished || impl_->validity) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "boolean validity is already set");
  }
  impl_->validity = std::move(validity);
  return {};
}
Status BooleanColumnBuilder::set_values(ByteBuffer values) {
  if (impl_->finished || impl_->values) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "boolean values are already set");
  }
  impl_->values = std::move(values);
  return {};
}
Result<Column> BooleanColumnBuilder::finish() && {
  if (impl_->finished || !impl_->values) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "invalid or incomplete boolean column");
  }
  impl_->finished = true;
  if (auto status = validate_validity(impl_->row_count, impl_->validity);
      !status) {
    return std::unexpected(std::move(status.error()));
  }
  auto type = LogicalType::create(LogicalTypeKind::boolean);
  if (!type) {
    return std::unexpected(std::move(type.error()));
  }
  try {
    return Column(std::make_shared<const Column::Impl>(
        std::move(*type),
        ColumnStorageKind::boolean,
        impl_->row_count,
        std::move(impl_->validity),
        std::move(impl_->values),
        std::nullopt,
        std::nullopt));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to allocate boolean column");
  }
}

class VariableWidthColumnBuilder::Impl {
 public:
  Impl(LogicalType type_value, std::uint64_t row_count_value)
      : type(std::move(type_value)), row_count(row_count_value) {}
  LogicalType type;
  std::uint64_t row_count;
  std::optional<Validity> validity;
  std::optional<OffsetBuffer> offsets;
  std::optional<ByteBuffer> payload;
  bool finished = false;
};

VariableWidthColumnBuilder::VariableWidthColumnBuilder(
    LogicalType type,
    std::uint64_t row_count)
    : impl_(std::make_unique<Impl>(std::move(type), row_count)) {}
VariableWidthColumnBuilder::~VariableWidthColumnBuilder() = default;
VariableWidthColumnBuilder::VariableWidthColumnBuilder(
    VariableWidthColumnBuilder&&) noexcept = default;
VariableWidthColumnBuilder& VariableWidthColumnBuilder::operator=(
    VariableWidthColumnBuilder&&) noexcept = default;
Status VariableWidthColumnBuilder::set_validity(Validity validity) {
  if (impl_->finished || impl_->validity) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "variable-width validity is already set");
  }
  impl_->validity = std::move(validity);
  return {};
}
Status VariableWidthColumnBuilder::set_offsets(OffsetBuffer offsets) {
  if (impl_->finished || impl_->offsets) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "variable-width offsets are already set");
  }
  impl_->offsets = std::move(offsets);
  return {};
}
Status VariableWidthColumnBuilder::set_payload(ByteBuffer payload) {
  if (impl_->finished || impl_->payload) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "variable-width payload is already set");
  }
  impl_->payload = std::move(payload);
  return {};
}
Result<Column> VariableWidthColumnBuilder::finish() && {
  if (impl_->finished || !impl_->offsets || !impl_->payload ||
      !impl_->type.is_variable_width()) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "invalid or incomplete variable-width column");
  }
  impl_->finished = true;
  if (auto status = validate_validity(impl_->row_count, impl_->validity);
      !status) {
    return std::unexpected(std::move(status.error()));
  }
  try {
    return Column(std::make_shared<const Column::Impl>(
        std::move(impl_->type),
        ColumnStorageKind::variable_width,
        impl_->row_count,
        std::move(impl_->validity),
        std::nullopt,
        std::move(impl_->offsets),
        std::move(impl_->payload)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to allocate variable-width column");
  }
}

}  // namespace shibori::engine
