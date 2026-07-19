#include <shibori/engine/block.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

shibori::engine::Result<shibori::engine::Schema> make_schema(
    shibori::engine::LogicalType type,
    bool nullable) {
  using namespace shibori::engine;
  auto field =
      FieldBuilder(1, "value", std::move(type), nullable).finish();
  if (!field) {
    return std::unexpected(std::move(field.error()));
  }
  SchemaBuilder builder;
  if (auto status = builder.add_field(std::move(*field)); !status) {
    return std::unexpected(std::move(status.error()));
  }
  return std::move(builder).finish();
}

shibori::engine::Result<shibori::engine::Column> fixed_column(
    shibori::engine::LogicalTypeKind kind,
    std::uint64_t rows,
    std::span<const std::byte> bytes) {
  using namespace shibori::engine;
  auto type = LogicalType::create(kind);
  auto values = ByteBuffer::copy(bytes);
  FixedWidthColumnBuilder builder(std::move(*type), rows);
  (void)builder.set_values(std::move(*values));
  return std::move(builder).finish();
}

bool fixed_and_limits() {
  using namespace shibori::engine;
  auto type = LogicalType::create(LogicalTypeKind::float64);
  auto schema = make_schema(*type, false);
  constexpr std::uint64_t bits = 0x8000000000000000ULL;
  const auto bytes = std::bit_cast<std::array<std::byte, 8>>(bits);
  auto column = fixed_column(LogicalTypeKind::float64, 1, bytes);
  BlockBuilder builder(*schema, 9, 1);
  (void)builder.set_column(1, std::move(*column));
  auto block = std::move(builder).finish();
  if (!block) {
    return false;
  }
  const auto stored = std::bit_cast<std::uint64_t>(
      std::array<std::byte, 8>{
          block->column(0).values()->bytes()[0],
          block->column(0).values()->bytes()[1],
          block->column(0).values()->bytes()[2],
          block->column(0).values()->bytes()[3],
          block->column(0).values()->bytes()[4],
          block->column(0).values()->bytes()[5],
          block->column(0).values()->bytes()[6],
          block->column(0).values()->bytes()[7]});

  auto short_column = fixed_column(
      LogicalTypeKind::float64,
      1,
      std::span(bytes).first(7));
  BlockBuilder short_builder(*schema, 10, 1);
  (void)short_builder.set_column(1, std::move(*short_column));
  auto rejected_short = std::move(short_builder).finish();

  auto limited_column = fixed_column(LogicalTypeKind::float64, 1, bytes);
  BlockBuilder limited_builder(*schema, 11, 1);
  (void)limited_builder.set_column(1, std::move(*limited_column));
  auto limits = default_resource_limits();
  limits.maximum_decoded_block_bytes = 7;
  auto rejected_limit = std::move(limited_builder).finish(limits);

  return expect(stored == bits, "Block changed floating-point bits") &&
         expect(!rejected_short, "Incorrect fixed-width size was accepted") &&
         expect(!rejected_limit, "Decoded-byte limit was ignored");
}

bool null_and_type_validation() {
  using namespace shibori::engine;
  auto schema_type = LogicalType::create(LogicalTypeKind::int32);
  auto schema = make_schema(*schema_type, false);

  ValidityBuilder validity_builder(1);
  (void)validity_builder.append(false);
  auto validity = std::move(validity_builder).finish();
  const std::array<std::byte, 0> no_values{};
  auto int_type = LogicalType::create(LogicalTypeKind::int32);
  auto buffer = ByteBuffer::copy(no_values);
  FixedWidthColumnBuilder nullable(std::move(*int_type), 1);
  (void)nullable.set_validity(std::move(*validity));
  (void)nullable.set_values(std::move(*buffer));
  auto null_column = std::move(nullable).finish();
  BlockBuilder null_builder(*schema, 1, 1);
  (void)null_builder.set_column(1, std::move(*null_column));
  auto null_result = std::move(null_builder).finish();

  const std::array wrong_bytes{std::byte{1}};
  auto wrong = fixed_column(LogicalTypeKind::int8, 1, wrong_bytes);
  BlockBuilder type_builder(*schema, 2, 1);
  (void)type_builder.set_column(1, std::move(*wrong));
  auto type_result = std::move(type_builder).finish();

  auto row_column = fixed_column(LogicalTypeKind::int32, 1, wrong_bytes);
  BlockBuilder row_builder(*schema, 3, 2);
  (void)row_builder.set_column(1, std::move(*row_column));
  auto row_result = std::move(row_builder).finish();

  return expect(!null_result, "Null entered a non-nullable field") &&
         expect(!type_result, "Schema type mismatch was accepted") &&
         expect(!row_result, "Column row mismatch was accepted");
}

shibori::engine::Result<shibori::engine::Column> variable_column(
    std::span<const std::uint64_t> offsets,
    std::span<const std::byte> payload) {
  using namespace shibori::engine;
  auto type = LogicalType::create(
      LogicalTypeKind::string,
      StringParameters{Utf8Validation::strict});
  auto offset_buffer = OffsetBuffer::copy(offsets);
  auto payload_buffer = ByteBuffer::copy(payload);
  VariableWidthColumnBuilder builder(std::move(*type), offsets.size() - 1);
  (void)builder.set_offsets(std::move(*offset_buffer));
  (void)builder.set_payload(std::move(*payload_buffer));
  return std::move(builder).finish();
}

bool offset_and_ownership_validation() {
  using namespace shibori::engine;
  auto type = LogicalType::create(
      LogicalTypeKind::string,
      StringParameters{Utf8Validation::strict});
  auto schema = make_schema(*type, false);
  const std::array payload{
      std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};

  const std::array valid_offsets{std::uint64_t{0}, std::uint64_t{2},
                                 std::uint64_t{3}};
  auto valid = variable_column(valid_offsets, payload);
  BlockBuilder valid_builder(*schema, 4, 2);
  (void)valid_builder.set_column(1, std::move(*valid));
  auto block = std::move(valid_builder).finish();

  const std::array descending{std::uint64_t{0}, std::uint64_t{3},
                              std::uint64_t{2}};
  auto descending_column = variable_column(descending, payload);
  BlockBuilder descending_builder(*schema, 5, 2);
  (void)descending_builder.set_column(1, std::move(*descending_column));
  auto descending_result = std::move(descending_builder).finish();

  const std::array beyond{std::uint64_t{0}, std::uint64_t{2},
                          std::uint64_t{9}};
  auto beyond_column = variable_column(beyond, payload);
  BlockBuilder beyond_builder(*schema, 6, 2);
  (void)beyond_builder.set_column(1, std::move(*beyond_column));
  auto beyond_result = std::move(beyond_builder).finish();

  if (!block) {
    return false;
  }
  const auto value = block->variable_value(1, 0);
  return expect(
             value && value->size() == 2 &&
                 std::to_integer<char>((*value)[0]) == 'a',
             "Validated variable value was not retained") &&
         expect(!descending_result, "Descending offsets were accepted") &&
         expect(!beyond_result, "Offset beyond payload was accepted");
}

}  // namespace

int main() {
  return fixed_and_limits() && null_and_type_validation() &&
                 offset_and_ownership_validation()
             ? 0
             : 1;
}
