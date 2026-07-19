#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <variant>

#include <shibori/engine/export.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine {

enum class LogicalTypeKind : std::uint8_t {
  boolean,
  int8,
  int16,
  int32,
  int64,
  uint8,
  uint16,
  uint32,
  uint64,
  float32,
  float64,
  decimal,
  date,
  time,
  timestamp,
  duration,
  string,
  binary,
  fixed_binary,
  uuid,
};

enum class TimeUnit : std::uint8_t {
  second,
  millisecond,
  microsecond,
  nanosecond,
};

enum class TimezoneMode : std::uint8_t {
  instant,
  local,
};

enum class Utf8Validation : std::uint8_t {
  strict,
  trusted,
};

struct DecimalParameters {
  std::uint8_t precision;
  std::int16_t scale;

  constexpr bool operator==(const DecimalParameters&) const noexcept = default;
};

struct TimeParameters {
  TimeUnit unit;

  constexpr bool operator==(const TimeParameters&) const noexcept = default;
};

struct TimestampParameters {
  TimeUnit unit;
  TimezoneMode timezone;

  constexpr bool operator==(const TimestampParameters&) const noexcept = default;
};

struct StringParameters {
  Utf8Validation validation;

  constexpr bool operator==(const StringParameters&) const noexcept = default;
};

struct FixedBinaryParameters {
  std::uint32_t byte_width;

  constexpr bool operator==(const FixedBinaryParameters&) const noexcept =
      default;
};

using LogicalTypeParameters = std::variant<
    std::monostate,
    DecimalParameters,
    TimeParameters,
    TimestampParameters,
    StringParameters,
    FixedBinaryParameters>;

[[nodiscard]] SHIBORI_ENGINE_API std::string_view to_string(
    LogicalTypeKind kind) noexcept;

class LogicalType {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<LogicalType> create(
      LogicalTypeKind kind,
      LogicalTypeParameters parameters = {});

  SHIBORI_ENGINE_API ~LogicalType();
  SHIBORI_ENGINE_API LogicalType(const LogicalType& other);
  SHIBORI_ENGINE_API LogicalType& operator=(const LogicalType& other);
  SHIBORI_ENGINE_API LogicalType(LogicalType&& other) noexcept;
  SHIBORI_ENGINE_API LogicalType& operator=(LogicalType&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API LogicalTypeKind kind() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API LogicalTypeParameters parameters() const;
  [[nodiscard]] SHIBORI_ENGINE_API bool is_fixed_width() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API bool is_variable_width() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint32_t fixed_width_bytes()
      const noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API bool operator==(
      const LogicalType& other) const noexcept;

 private:
  class Impl;
  explicit LogicalType(std::unique_ptr<Impl> impl) noexcept;

  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
