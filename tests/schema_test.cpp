#include <shibori/engine/schema.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
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

shibori::engine::Result<shibori::engine::Field> make_field(
    std::uint32_t id,
    std::string name) {
  using namespace shibori::engine;
  auto type = LogicalType::create(LogicalTypeKind::int64);
  if (!type) {
    return std::unexpected(std::move(type.error()));
  }
  return FieldBuilder(id, std::move(name), std::move(*type), false).finish();
}

bool validation_tests() {
  using namespace shibori::engine;
  auto type = LogicalType::create(LogicalTypeKind::string, StringParameters{
      Utf8Validation::strict});
  if (!type) {
    return false;
  }
  const std::array value{std::byte{1}, std::byte{2}};
  FieldBuilder field_builder(1, "name", std::move(*type), true);
  if (!field_builder.add_metadata("org.example/source", value)) {
    return false;
  }
  auto field = std::move(field_builder).finish();
  if (!expect(field.has_value(), "Valid field was rejected")) {
    return false;
  }

  auto invalid_type = LogicalType::create(LogicalTypeKind::int8);
  auto invalid_name = FieldBuilder(
      2,
      std::string("\xc0\x80", 2),
      std::move(*invalid_type),
      false).finish();
  auto zero_type = LogicalType::create(LogicalTypeKind::int8);
  auto zero_id =
      FieldBuilder(0, "zero", std::move(*zero_type), false).finish();

  auto metadata_type = LogicalType::create(LogicalTypeKind::int8);
  FieldBuilder metadata_builder(
      3, "metadata", std::move(*metadata_type), false);
  const auto bad_key = metadata_builder.add_metadata("unqualified", value);
  const auto reserved =
      metadata_builder.add_metadata("shibori.other/key", value);

  auto limits = default_resource_limits();
  limits.maximum_metadata_bytes = 4;
  auto limited_type = LogicalType::create(LogicalTypeKind::int8);
  FieldBuilder limited_builder(4, "wide", std::move(*limited_type), false);
  (void)limited_builder.add_metadata("org.x/k", value);
  const auto limited = std::move(limited_builder).finish(limits);

  return expect(!invalid_name, "Invalid UTF-8 field name was accepted") &&
         expect(!zero_id, "Zero field ID was accepted") &&
         expect(!bad_key, "Unqualified metadata key was accepted") &&
         expect(!reserved, "Reserved metadata namespace was accepted") &&
         expect(!limited, "Metadata limit was ignored");
}

bool schema_tests() {
  using namespace shibori::engine;
  auto first = make_field(1, "first");
  auto second = make_field(2, "second");
  if (!first || !second) {
    return false;
  }

  const std::array a{std::byte{1}};
  const std::array b{std::byte{2}};
  SchemaBuilder left;
  (void)left.add_field(*first);
  (void)left.add_field(*second);
  (void)left.add_metadata("org.example/z", b);
  (void)left.add_metadata("org.example/a", a);
  auto left_schema = std::move(left).finish();

  SchemaBuilder right;
  (void)right.add_field(*first);
  (void)right.add_field(*second);
  (void)right.add_metadata("org.example/a", a);
  (void)right.add_metadata("org.example/z", b);
  auto right_schema = std::move(right).finish();
  if (!left_schema || !right_schema) {
    return false;
  }

  auto duplicate_id = make_field(1, "third");
  SchemaBuilder duplicate;
  (void)duplicate.add_field(*first);
  (void)duplicate.add_field(std::move(*duplicate_id));
  const auto duplicate_schema = std::move(duplicate).finish();

  auto same_name = make_field(3, "first");
  SchemaBuilder names;
  (void)names.add_field(*first);
  (void)names.add_field(std::move(*same_name));
  const auto name_schema = std::move(names).finish();

  auto limits = default_resource_limits();
  limits.maximum_fields = 1;
  SchemaBuilder too_many;
  (void)too_many.add_field(*first);
  (void)too_many.add_field(*second);
  const auto limited = std::move(too_many).finish(limits);

  const auto shared = *left_schema;
  return expect(
             left_schema->canonical_bytes().size() != 0,
             "Canonical schema is empty") &&
         expect(
             left_schema->canonical_bytes().size() ==
                     right_schema->canonical_bytes().size() &&
                 std::ranges::equal(
                     left_schema->canonical_bytes(),
                     right_schema->canonical_bytes()),
             "Metadata insertion order changed canonical bytes") &&
         expect(
             left_schema->fingerprint() == right_schema->fingerprint(),
             "Equivalent schemas have different fingerprints") &&
         expect(shared.find_field(2) != nullptr, "Shared schema lost a field") &&
         expect(!duplicate_schema, "Duplicate field ID was accepted") &&
         expect(!name_schema, "Duplicate field name was accepted") &&
         expect(!limited, "Field-count limit was ignored");
}

bool ownership_test() {
  using namespace shibori::engine;
  Result<Schema> result = fail(
      ErrorCode::invariant_violation,
      Operation::validate_schema,
      "not initialized");
  {
    std::string name = "temporary";
    auto field = make_field(7, name);
    SchemaBuilder builder;
    (void)builder.add_field(std::move(*field));
    result = std::move(builder).finish();
  }
  return expect(
      result && result->field(0).name() == "temporary",
      "Finished schema retained temporary caller input");
}

}  // namespace

int main() {
  return validation_tests() && schema_tests() && ownership_test() ? 0 : 1;
}
