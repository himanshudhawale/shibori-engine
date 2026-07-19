#include <shibori/engine/error.hpp>
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
  return value.value() == 42 ? 0 : 1;
}
