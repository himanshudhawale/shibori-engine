#include <shibori/engine/checked_arithmetic.hpp>
#include <shibori/engine/error.hpp>
#include <shibori/engine/logical_type.hpp>
#include <shibori/engine/resource.hpp>
#include <shibori/engine/result.hpp>
#include <shibori/engine/version.hpp>

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
  return reservation && budget->used() == 64 && decimal &&
             decimal->fixed_width_bytes() == 16
         ? 0
         : 1;
}
