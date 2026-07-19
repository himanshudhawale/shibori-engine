#include <shibori/engine/checked_arithmetic.hpp>
#include <shibori/engine/block.hpp>
#include <shibori/engine/column.hpp>
#include <shibori/engine/error.hpp>
#include <shibori/engine/logical_type.hpp>
#include <shibori/engine/resource.hpp>
#include <shibori/engine/result.hpp>
#include <shibori/engine/schema.hpp>
#include <shibori/engine/version.hpp>

#include <array>
#include <cstddef>
#include <utility>

int main() {
  if (shibori::engine::version_string() != "0.1.0") {
    return 1;
  }

  const auto error = shibori::engine::Error(
      shibori::engine::ErrorCode::invalid_configuration,
      shibori::engine::Operation::configure,
      "invalid consumer configuration");

  if (error.category() != shibori::engine::ErrorCategory::invalid_argument) {
    return 1;
  }

  const shibori::engine::Result<int> value = 42;
  if (value.value() != 42) {
    return 1;
  }

  const auto size = shibori::engine::checked_add(
      20,
      22,
      shibori::engine::Operation::configure,
      "consumer size");
  if (!size || *size != 42) {
    return 1;
  }

  auto budget = shibori::engine::ResourceBudget::create_root(
      shibori::engine::ResourceKind::resident_memory,
      64);
  if (!budget) {
    return 1;
  }

  auto reservation = budget->reserve(
      64,
      shibori::engine::Operation::configure);
  const auto decimal = shibori::engine::LogicalType::create(
      shibori::engine::LogicalTypeKind::decimal,
      shibori::engine::DecimalParameters{18, 4});
  auto field_type = shibori::engine::LogicalType::create(
      shibori::engine::LogicalTypeKind::int64);
  auto field = shibori::engine::FieldBuilder(
      1, "value", std::move(*field_type), false).finish();
  shibori::engine::SchemaBuilder schema_builder;
  const auto added = schema_builder.add_field(std::move(*field));
  const auto schema = std::move(schema_builder).finish();
  const std::array<std::byte, 8> column_bytes{std::byte{42}};
  auto column_buffer = shibori::engine::ByteBuffer::copy(column_bytes);
  auto column_type = shibori::engine::LogicalType::create(
      shibori::engine::LogicalTypeKind::int64);
  shibori::engine::FixedWidthColumnBuilder column_builder(
      std::move(*column_type), 1);
  const auto values_set = column_builder.set_values(std::move(*column_buffer));
  const auto column = std::move(column_builder).finish();
  shibori::engine::BlockBuilder block_builder(*schema, 1, 1);
  const auto column_set =
      block_builder.set_column(1, std::move(*column));
  const auto block = std::move(block_builder).finish();
  return reservation && budget->used() == 64 && decimal && added && schema &&
             schema->field_count() == 1 && decimal->fixed_width_bytes() == 16 &&
             values_set && column_set && block && block->row_count() == 1
         ? 0
         : 1;
}
