#include <shibori/engine/column.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool validity_tests() {
  using namespace shibori::engine;
  ValidityBuilder builder(10);
  for (std::uint64_t row = 0; row < 10; ++row) {
    if (!builder.append(row != 3 && row != 9)) {
      return false;
    }
  }
  auto validity = std::move(builder).finish();
  const std::array bad_padding{std::byte{0xff}, std::byte{0xff}};
  auto bad_buffer = ByteBuffer::copy(bad_padding);
  auto invalid = Validity::from_bitmap(10, std::move(*bad_buffer));
  ValidityBuilder incomplete(2);
  (void)incomplete.append(true);
  auto incomplete_result = std::move(incomplete).finish();
  return expect(validity && validity->null_count() == 2,
                "Validity null count is incorrect") &&
         expect(validity->is_valid(0) && !validity->is_valid(3),
                "Validity bits are incorrect") &&
         expect(!invalid, "Nonzero validity padding was accepted") &&
         expect(!incomplete_result, "Incomplete validity was accepted");
}

bool ownership_and_fixed_tests() {
  using namespace shibori::engine;
  Result<Column> column = fail(
      ErrorCode::invariant_violation,
      Operation::validate_block,
      "not initialized");
  constexpr std::uint64_t float_bits = 0x7ff8000000001234ULL;
  {
    const auto bytes = std::bit_cast<std::array<std::byte, 8>>(float_bits);
    auto buffer = ByteBuffer::copy(bytes);
    auto type = LogicalType::create(LogicalTypeKind::float64);
    FixedWidthColumnBuilder builder(std::move(*type), 1);
    (void)builder.set_values(std::move(*buffer));
    column = std::move(builder).finish();
  }
  if (!expect(column.has_value(), "Fixed-width column construction failed")) {
    return false;
  }
  const auto round_trip =
      std::bit_cast<std::uint64_t>(std::array<std::byte, 8>{
          column->values()->bytes()[0], column->values()->bytes()[1],
          column->values()->bytes()[2], column->values()->bytes()[3],
          column->values()->bytes()[4], column->values()->bytes()[5],
          column->values()->bytes()[6], column->values()->bytes()[7]});

  auto shared_storage =
      std::make_shared<const std::vector<std::byte>>(4, std::byte{7});
  const auto* address = shared_storage->data();
  auto shared = ByteBuffer::share(shared_storage);
  shared_storage.reset();
  return expect(round_trip == float_bits,
                "Floating-point bits were changed") &&
         expect(shared && shared->ownership() == BufferOwnership::shared &&
                    shared->bytes().data() == address,
                "Shared buffer was copied or lost");
}

bool typed_builder_tests() {
  using namespace shibori::engine;
  const std::array bool_bits{std::byte{0x05}};
  auto bool_buffer = ByteBuffer::copy(bool_bits);
  BooleanColumnBuilder boolean(3);
  (void)boolean.set_values(std::move(*bool_buffer));
  auto bool_column = std::move(boolean).finish();

  auto string_type = LogicalType::create(
      LogicalTypeKind::string,
      StringParameters{Utf8Validation::strict});
  const std::array<std::uint64_t, 3> offsets{0, 2, 2};
  const std::array payload{std::byte{'o'}, std::byte{'k'}};
  auto offset_buffer = OffsetBuffer::copy(offsets);
  auto payload_buffer = ByteBuffer::copy(payload);
  VariableWidthColumnBuilder variable(std::move(*string_type), 2);
  (void)variable.set_offsets(std::move(*offset_buffer));
  (void)variable.set_payload(std::move(*payload_buffer));
  auto variable_column = std::move(variable).finish();

  auto wrong_type = LogicalType::create(LogicalTypeKind::binary);
  auto empty = ByteBuffer::copy({});
  FixedWidthColumnBuilder wrong(std::move(*wrong_type), 0);
  (void)wrong.set_values(std::move(*empty));
  const auto rejected = std::move(wrong).finish();

  return expect(
             bool_column &&
                 bool_column->storage_kind() == ColumnStorageKind::boolean,
             "Boolean column construction failed") &&
         expect(
             variable_column &&
                 variable_column->offsets()->values()[1] == 2 &&
                 variable_column->payload()->size() == 2,
             "Variable-width buffers were not retained") &&
         expect(!rejected, "Variable type entered fixed-width storage");
}

}  // namespace

int main() {
  return validity_tests() && ownership_and_fixed_tests() &&
                 typed_builder_tests()
             ? 0
             : 1;
}
