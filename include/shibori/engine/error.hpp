#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <shibori/engine/export.hpp>

namespace shibori::engine {

enum class ErrorCategory : std::uint8_t {
  invalid_argument,
  invalid_data,
  io,
  truncated,
  corrupt,
  unsupported,
  resource_limit,
  cancelled,
  plugin,
  internal,
};

enum class ErrorCode : std::uint16_t {
  invalid_configuration = 1000,
  invalid_state = 1001,
  invalid_resource_limit = 1002,
  invalid_schema = 2000,
  invalid_block = 2001,
  io_read_failed = 3000,
  io_write_failed = 3001,
  io_no_progress = 3002,
  unexpected_end = 4000,
  checksum_mismatch = 5000,
  malformed_container = 5001,
  invalid_record = 5002,
  unsupported_format_version = 6000,
  unsupported_feature = 6001,
  missing_component = 6002,
  limit_exceeded = 7000,
  allocation_failed = 7001,
  arithmetic_overflow = 7002,
  range_exceeded = 7003,
  operation_cancelled = 8000,
  plugin_contract_violation = 9000,
  plugin_failure = 9001,
  invariant_violation = 10000,
};

enum class Operation : std::uint8_t {
  unknown,
  configure,
  validate_schema,
  validate_block,
  read,
  write,
  parse,
  encode,
  decode,
  finalize,
  inspect,
  verify,
  load_plugin,
};

[[nodiscard]] constexpr ErrorCategory category_for(ErrorCode code) noexcept {
  const auto value = static_cast<std::uint16_t>(code);

  if (value < 2000) {
    return ErrorCategory::invalid_argument;
  }
  if (value < 3000) {
    return ErrorCategory::invalid_data;
  }
  if (value < 4000) {
    return ErrorCategory::io;
  }
  if (value < 5000) {
    return ErrorCategory::truncated;
  }
  if (value < 6000) {
    return ErrorCategory::corrupt;
  }
  if (value < 7000) {
    return ErrorCategory::unsupported;
  }
  if (value < 8000) {
    return ErrorCategory::resource_limit;
  }
  if (value < 9000) {
    return ErrorCategory::cancelled;
  }
  if (value < 10000) {
    return ErrorCategory::plugin;
  }
  return ErrorCategory::internal;
}

[[nodiscard]] SHIBORI_ENGINE_API std::string_view to_string(
    ErrorCategory category) noexcept;
[[nodiscard]] SHIBORI_ENGINE_API std::string_view to_string(
    ErrorCode code) noexcept;
[[nodiscard]] SHIBORI_ENGINE_API std::string_view to_string(
    Operation operation) noexcept;

class Error {
 public:
  SHIBORI_ENGINE_API Error(
      ErrorCode code,
      Operation operation,
      std::string message);
  SHIBORI_ENGINE_API ~Error();

  SHIBORI_ENGINE_API Error(const Error& other);
  SHIBORI_ENGINE_API Error& operator=(const Error& other);
  SHIBORI_ENGINE_API Error(Error&& other) noexcept;
  SHIBORI_ENGINE_API Error& operator=(Error&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API ErrorCategory category() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API ErrorCode code() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API Operation operation() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const std::string& message() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint64_t> byte_offset()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint64_t> block_id()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint32_t> field_id()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint32_t> component_id()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const Error* cause() const noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Error with_byte_offset(
      std::uint64_t value) &&;
  [[nodiscard]] SHIBORI_ENGINE_API Error with_block_id(
      std::uint64_t value) &&;
  [[nodiscard]] SHIBORI_ENGINE_API Error with_field_id(
      std::uint32_t value) &&;
  [[nodiscard]] SHIBORI_ENGINE_API Error with_component_id(
      std::uint32_t value) &&;
  [[nodiscard]] SHIBORI_ENGINE_API Error with_cause(Error value) &&;

  [[nodiscard]] SHIBORI_ENGINE_API std::string describe() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
