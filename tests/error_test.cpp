#include <shibori/engine/error.hpp>
#include <shibori/engine/result.hpp>

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

shibori::engine::Result<std::unique_ptr<int>> make_move_only_result() {
  return std::make_unique<int>(42);
}

shibori::engine::Status make_failure() {
  return shibori::engine::fail(
      shibori::engine::ErrorCode::invalid_state,
      shibori::engine::Operation::finalize,
      std::string("writer is already finalized"));
}

}  // namespace

int main() {
  using shibori::engine::Error;
  using shibori::engine::ErrorCategory;
  using shibori::engine::ErrorCode;
  using shibori::engine::Operation;

  constexpr auto category_cases = std::to_array<std::pair<ErrorCode, ErrorCategory>>({
      {ErrorCode::invalid_configuration, ErrorCategory::invalid_argument},
      {ErrorCode::invalid_schema, ErrorCategory::invalid_data},
      {ErrorCode::io_read_failed, ErrorCategory::io},
      {ErrorCode::unexpected_end, ErrorCategory::truncated},
      {ErrorCode::checksum_mismatch, ErrorCategory::corrupt},
      {ErrorCode::unsupported_feature, ErrorCategory::unsupported},
      {ErrorCode::limit_exceeded, ErrorCategory::resource_limit},
      {ErrorCode::operation_cancelled, ErrorCategory::cancelled},
      {ErrorCode::plugin_failure, ErrorCategory::plugin},
      {ErrorCode::invariant_violation, ErrorCategory::internal},
  });

  for (const auto& [code, category] : category_cases) {
    if (!expect(
            shibori::engine::category_for(code) == category,
            "Error code mapped to the wrong category")) {
      return 1;
    }
  }

  auto cause = Error(
      ErrorCode::io_read_failed,
      Operation::read,
      "source read failed");
  auto error = Error(
                   ErrorCode::invalid_record,
                   Operation::parse,
                   "record checksum does not match")
                   .with_byte_offset(4096)
                   .with_block_id(7)
                   .with_field_id(3)
                   .with_component_id(12)
                   .with_cause(std::move(cause));

  const auto description = error.describe();
  const auto context_is_present =
      description.find("corrupt/invalid_record") != std::string::npos &&
      description.find("offset=4096") != std::string::npos &&
      description.find("block=7") != std::string::npos &&
      description.find("field=3") != std::string::npos &&
      description.find("component=12") != std::string::npos &&
      description.find("io/io_read_failed") != std::string::npos;

  if (!expect(context_is_present, "Formatted error omitted diagnostic context")) {
    return 1;
  }

  if (!expect(error.cause() != nullptr, "Nested cause was not retained")) {
    return 1;
  }

  auto move_only = make_move_only_result();
  if (!expect(
          move_only.has_value() && **move_only == 42,
          "Result did not preserve a move-only value")) {
    return 1;
  }

  auto failure = make_failure();
  if (!expect(
          !failure.has_value() &&
              failure.error().code() == ErrorCode::invalid_state,
          "Result did not preserve its typed error")) {
    return 1;
  }

  return 0;
}
