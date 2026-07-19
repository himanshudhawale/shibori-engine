#include <shibori/engine/logical_type.hpp>

#include <array>
#include <cstdint>
#include <iostream>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool all_types() {
  using namespace shibori::engine;
  constexpr std::array parameterless{
      LogicalTypeKind::boolean, LogicalTypeKind::int8,
      LogicalTypeKind::int16, LogicalTypeKind::int32,
      LogicalTypeKind::int64, LogicalTypeKind::uint8,
      LogicalTypeKind::uint16, LogicalTypeKind::uint32,
      LogicalTypeKind::uint64, LogicalTypeKind::float32,
      LogicalTypeKind::float64, LogicalTypeKind::date,
      LogicalTypeKind::binary, LogicalTypeKind::uuid};
  for (const auto kind : parameterless) {
    const auto type = LogicalType::create(kind);
    if (!expect(type && type->kind() == kind, "A portable type was rejected")) {
      return false;
    }
  }

  return expect(
             LogicalType::create(
                 LogicalTypeKind::decimal,
                 DecimalParameters{38, 38})
                 .has_value(),
             "Decimal boundary was rejected") &&
         expect(
             LogicalType::create(
                 LogicalTypeKind::time,
                 TimeParameters{TimeUnit::nanosecond})
                 .has_value(),
             "Time type was rejected") &&
         expect(
             LogicalType::create(
                 LogicalTypeKind::timestamp,
                 TimestampParameters{
                     TimeUnit::microsecond,
                     TimezoneMode::instant})
                 .has_value(),
             "Timestamp type was rejected") &&
         expect(
             LogicalType::create(
                 LogicalTypeKind::duration,
                 TimeParameters{TimeUnit::second})
                 .has_value(),
             "Duration type was rejected") &&
         expect(
             LogicalType::create(
                 LogicalTypeKind::string,
                 StringParameters{Utf8Validation::strict})
                 .has_value(),
             "String type was rejected") &&
         expect(
             LogicalType::create(
                 LogicalTypeKind::fixed_binary,
                 FixedBinaryParameters{4096})
                 .has_value(),
             "Fixed binary type was rejected");
}

bool invalid_parameters() {
  using namespace shibori::engine;
  return expect(
             !LogicalType::create(
                 LogicalTypeKind::decimal,
                 DecimalParameters{0, 0}),
             "Zero decimal precision was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::decimal,
                 DecimalParameters{39, 0}),
             "Oversized decimal precision was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::decimal,
                 DecimalParameters{10, 11}),
             "Decimal scale above precision was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::decimal,
                 DecimalParameters{10, -11}),
             "Decimal scale below negative precision was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::time,
                 TimeParameters{static_cast<TimeUnit>(255)}),
             "Unknown time unit was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::timestamp,
                 TimestampParameters{
                     TimeUnit::second,
                     static_cast<TimezoneMode>(255)}),
             "Unknown timezone mode was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::string,
                 StringParameters{static_cast<Utf8Validation>(255)}),
             "Unknown UTF-8 mode was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::fixed_binary,
                 FixedBinaryParameters{0}),
             "Zero fixed-binary width was accepted") &&
         expect(
             !LogicalType::create(
                 LogicalTypeKind::int64,
                 TimeParameters{TimeUnit::second}),
             "Parameters on an integer were accepted") &&
         expect(
             !LogicalType::create(
                 static_cast<LogicalTypeKind>(255)),
             "Unknown logical type was accepted");
}

}  // namespace

int main() {
  return all_types() && invalid_parameters() ? 0 : 1;
}
