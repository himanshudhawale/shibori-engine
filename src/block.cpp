#include <shibori/engine/block.hpp>

#include <shibori/engine/checked_arithmetic.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace shibori::engine {
namespace {

std::unexpected<Error> invalid_column(
    std::uint32_t field_id,
    std::string message) {
  return std::unexpected(
      Error(
          ErrorCode::invalid_block,
          Operation::validate_block,
          std::move(message))
          .with_field_id(field_id));
}

bool valid_utf8(std::span<const std::byte> bytes) noexcept {
  std::size_t index = 0;
  while (index < bytes.size()) {
    const auto first = std::to_integer<std::uint8_t>(bytes[index]);
    std::size_t count = 0;
    std::uint32_t codepoint = 0;
    if (first <= 0x7fU) {
      ++index;
      continue;
    }
    if ((first & 0xe0U) == 0xc0U) {
      count = 2;
      codepoint = first & 0x1fU;
    } else if ((first & 0xf0U) == 0xe0U) {
      count = 3;
      codepoint = first & 0x0fU;
    } else if ((first & 0xf8U) == 0xf0U) {
      count = 4;
      codepoint = first & 0x07U;
    } else {
      return false;
    }
    if (index + count > bytes.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset < count; ++offset) {
      const auto byte =
          std::to_integer<std::uint8_t>(bytes[index + offset]);
      if ((byte & 0xc0U) != 0x80U) {
        return false;
      }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
    }
    const bool overlong =
        (count == 2 && codepoint < 0x80U) ||
        (count == 3 && codepoint < 0x800U) ||
        (count == 4 && codepoint < 0x10000U);
    if (overlong || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
      return false;
    }
    index += count;
  }
  return true;
}

Status add_decoded_bytes(
    std::uint64_t amount,
    std::uint64_t limit,
    std::uint64_t& total) {
  auto next = checked_add(
      total,
      amount,
      Operation::validate_block,
      "decoded block bytes");
  if (!next) {
    return std::unexpected(std::move(next.error()));
  }
  total = *next;
  if (total > limit) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::validate_block,
        "decoded block bytes exceed the configured limit");
  }
  return {};
}

Status validate_fixed(
    const Field& field,
    const Column& column,
    std::uint64_t dense_count,
    const ResourceLimits& limits,
    std::uint64_t& decoded_bytes) {
  const auto width = static_cast<std::uint64_t>(
      column.type().fixed_width_bytes());
  auto expected = checked_multiply(
      dense_count,
      width,
      Operation::validate_block,
      "fixed-width column bytes");
  if (!expected) {
    return std::unexpected(std::move(expected.error()));
  }
  if (column.values() == nullptr || column.values()->size() != *expected) {
    return invalid_column(
        field.id(),
        "fixed-width value bytes do not match the dense value count");
  }
  return add_decoded_bytes(
      *expected,
      limits.maximum_decoded_block_bytes,
      decoded_bytes);
}

Status validate_boolean(
    const Field& field,
    const Column& column,
    std::uint64_t dense_count,
    const ResourceLimits& limits,
    std::uint64_t& decoded_bytes) {
  if (dense_count > std::numeric_limits<std::uint64_t>::max() - 7) {
    return invalid_column(field.id(), "boolean bitmap size overflow");
  }
  const auto expected = (dense_count + 7) / 8;
  if (column.values() == nullptr || column.values()->size() != expected) {
    return invalid_column(
        field.id(),
        "boolean value bitmap does not match the dense value count");
  }
  if (dense_count % 8 != 0 && expected != 0) {
    const auto used = static_cast<unsigned>(dense_count % 8);
    const auto padding_mask =
        static_cast<std::uint8_t>(0xffU << used);
    if ((std::to_integer<std::uint8_t>(
             column.values()->bytes().back()) &
         padding_mask) != 0) {
      return invalid_column(
          field.id(),
          "boolean value padding bits must be zero");
    }
  }
  return add_decoded_bytes(
      expected,
      limits.maximum_decoded_block_bytes,
      decoded_bytes);
}

Status validate_variable(
    const Field& field,
    const Column& column,
    std::uint64_t dense_count,
    const ResourceLimits& limits,
    std::uint64_t& decoded_bytes) {
  if (dense_count == std::numeric_limits<std::uint64_t>::max()) {
    return invalid_column(field.id(), "variable offset count overflow");
  }
  const auto expected_count = dense_count + 1;
  if (column.offsets() == nullptr ||
      column.offsets()->size() != expected_count ||
      column.payload() == nullptr) {
    return invalid_column(
        field.id(),
        "variable-width physical components do not match the dense count");
  }

  const auto offsets = column.offsets()->values();
  if (offsets.empty() || offsets.front() != 0) {
    return invalid_column(field.id(), "variable offsets must begin at zero");
  }
  for (std::size_t index = 1; index < offsets.size(); ++index) {
    if (offsets[index] < offsets[index - 1]) {
      return invalid_column(
          field.id(),
          "variable offsets must be monotonically non-decreasing");
    }
    if (offsets[index] - offsets[index - 1] >
        limits.maximum_record_bytes) {
      return invalid_column(
          field.id(),
          "variable value exceeds the configured record limit");
    }
  }
  if (offsets.back() != column.payload()->size()) {
    return invalid_column(
        field.id(),
        "final variable offset must equal the payload size");
  }

  auto offset_bytes = checked_multiply(
      expected_count,
      sizeof(std::uint64_t),
      Operation::validate_block,
      "variable offset bytes");
  if (!offset_bytes) {
    return std::unexpected(std::move(offset_bytes.error()));
  }
  if (auto status = add_decoded_bytes(
          *offset_bytes,
          limits.maximum_decoded_block_bytes,
          decoded_bytes);
      !status) {
    return status;
  }
  if (auto status = add_decoded_bytes(
          column.payload()->size(),
          limits.maximum_decoded_block_bytes,
          decoded_bytes);
      !status) {
    return status;
  }

  if (column.type().kind() == LogicalTypeKind::string) {
    const auto parameters =
        std::get<StringParameters>(column.type().parameters());
    if (parameters.validation == Utf8Validation::strict) {
      const auto payload = column.payload()->bytes();
      for (std::size_t index = 1; index < offsets.size(); ++index) {
        const auto begin = static_cast<std::size_t>(offsets[index - 1]);
        const auto length =
            static_cast<std::size_t>(offsets[index] - offsets[index - 1]);
        if (!valid_utf8(payload.subspan(begin, length))) {
          return invalid_column(
              field.id(),
              "strict string column contains invalid UTF-8");
        }
      }
    }
  }
  return {};
}

Status validate_column(
    const Field& field,
    const Column& column,
    std::uint64_t row_count,
    const ResourceLimits& limits,
    std::uint64_t& decoded_bytes) {
  if (column.row_count() != row_count) {
    return invalid_column(
        field.id(),
        "column row count does not match the block");
  }
  if (!(column.type() == field.type())) {
    return invalid_column(
        field.id(),
        "column logical type does not match the schema field");
  }
  if (!field.nullable() && column.validity() != nullptr) {
    return invalid_column(
        field.id(),
        "non-nullable field must omit the validity bitmap");
  }
  if (column.null_count() > row_count) {
    return invalid_column(field.id(), "column null count exceeds row count");
  }
  if (column.validity() != nullptr) {
    if (auto status = add_decoded_bytes(
            column.validity()->bitmap().size(),
            limits.maximum_decoded_block_bytes,
            decoded_bytes);
        !status) {
      return status;
    }
  }

  const auto dense_count = row_count - column.null_count();
  switch (column.storage_kind()) {
    case ColumnStorageKind::fixed_width:
      return validate_fixed(
          field, column, dense_count, limits, decoded_bytes);
    case ColumnStorageKind::boolean:
      if (field.type().kind() != LogicalTypeKind::boolean) {
        return invalid_column(
            field.id(),
            "boolean storage requires the boolean logical type");
      }
      return validate_boolean(
          field, column, dense_count, limits, decoded_bytes);
    case ColumnStorageKind::variable_width:
      return validate_variable(
          field, column, dense_count, limits, decoded_bytes);
  }
  return invalid_column(field.id(), "unknown column storage kind");
}

}  // namespace

class Block::Impl {
 public:
  Impl(
      Schema schema_value,
      std::uint64_t id_value,
      std::uint64_t row_count_value,
      std::vector<Column> columns_value)
      : schema(std::move(schema_value)),
        id(id_value),
        row_count(row_count_value),
        columns(std::move(columns_value)) {}
  Schema schema;
  std::uint64_t id;
  std::uint64_t row_count;
  std::vector<Column> columns;
};

class BlockBuilder::Impl {
 public:
  Impl(Schema schema_value, std::uint64_t id_value, std::uint64_t rows_value)
      : schema(std::move(schema_value)), id(id_value), row_count(rows_value) {}
  Schema schema;
  std::uint64_t id;
  std::uint64_t row_count;
  std::map<std::uint32_t, Column> columns;
  bool finished = false;
};

Block::Block(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
Block::~Block() = default;
Block::Block(const Block&) = default;
Block& Block::operator=(const Block&) = default;
Block::Block(Block&&) noexcept = default;
Block& Block::operator=(Block&&) noexcept = default;
std::uint64_t Block::id() const noexcept { return impl_->id; }
std::uint64_t Block::row_count() const noexcept { return impl_->row_count; }
const Schema& Block::schema() const noexcept { return impl_->schema; }
std::size_t Block::column_count() const noexcept {
  return impl_->columns.size();
}
const Column& Block::column(std::size_t index) const {
  return impl_->columns.at(index);
}
const Column* Block::find_column(std::uint32_t field_id) const noexcept {
  for (std::size_t index = 0; index < impl_->schema.field_count(); ++index) {
    if (impl_->schema.field(index).id() == field_id) {
      return &impl_->columns[index];
    }
  }
  return nullptr;
}
Result<std::span<const std::byte>> Block::variable_value(
    std::uint32_t field_id,
    std::uint64_t dense_index) const {
  const auto* selected = find_column(field_id);
  if (selected == nullptr ||
      selected->storage_kind() != ColumnStorageKind::variable_width ||
      dense_index >= selected->offsets()->size() - 1) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::validate_block,
        "variable value selection is outside the validated column");
  }
  const auto offsets = selected->offsets()->values();
  const auto begin = offsets[static_cast<std::size_t>(dense_index)];
  const auto end = offsets[static_cast<std::size_t>(dense_index + 1)];
  return selected->payload()->bytes().subspan(
      static_cast<std::size_t>(begin),
      static_cast<std::size_t>(end - begin));
}

BlockBuilder::BlockBuilder(
    Schema schema,
    std::uint64_t id,
    std::uint64_t row_count)
    : impl_(std::make_unique<Impl>(
          std::move(schema),
          id,
          row_count)) {}
BlockBuilder::~BlockBuilder() = default;
BlockBuilder::BlockBuilder(BlockBuilder&&) noexcept = default;
BlockBuilder& BlockBuilder::operator=(BlockBuilder&&) noexcept = default;

Status BlockBuilder::set_column(std::uint32_t field_id, Column column) {
  if (impl_->finished || impl_->columns.contains(field_id)) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "block column is already set or builder is finished");
  }
  if (impl_->schema.find_field(field_id) == nullptr) {
    return invalid_column(field_id, "column field ID is absent from schema");
  }
  try {
    impl_->columns.emplace(field_id, std::move(column));
    return {};
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to retain block column");
  }
}

Result<Block> BlockBuilder::finish(const ResourceLimits& limits) && {
  if (impl_->finished) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_block,
        "block builder is already finished");
  }
  impl_->finished = true;
  if (auto status = validate_resource_limits(limits); !status) {
    return std::unexpected(std::move(status.error()));
  }
  if (impl_->row_count == 0) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "empty blocks are not accepted by the standard builder");
  }
  if (impl_->row_count > limits.maximum_block_rows) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::validate_block,
        "block row count exceeds the configured limit");
  }
  if (impl_->columns.size() != impl_->schema.field_count()) {
    return fail(
        ErrorCode::invalid_block,
        Operation::validate_block,
        "block must contain exactly one column for each schema field");
  }

  std::uint64_t decoded_bytes = 0;
  std::vector<Column> ordered;
  try {
    ordered.reserve(impl_->schema.field_count());
    for (std::size_t index = 0; index < impl_->schema.field_count(); ++index) {
      const auto& field = impl_->schema.field(index);
      const auto found = impl_->columns.find(field.id());
      if (found == impl_->columns.end()) {
        return invalid_column(field.id(), "schema field has no block column");
      }
      if (auto status = validate_column(
              field,
              found->second,
              impl_->row_count,
              limits,
              decoded_bytes);
          !status) {
        return std::unexpected(std::move(status.error()));
      }
      ordered.push_back(found->second);
    }
    return Block(std::make_shared<const Block::Impl>(
        std::move(impl_->schema),
        impl_->id,
        impl_->row_count,
        std::move(ordered)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_block,
        "unable to allocate immutable block");
  }
}

}  // namespace shibori::engine
