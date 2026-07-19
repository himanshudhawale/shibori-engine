#include <shibori/engine/logical_type.hpp>

#include <new>
#include <string>
#include <utility>

namespace shibori::engine {
namespace {

bool valid_time_unit(TimeUnit unit) noexcept {
  return unit >= TimeUnit::second && unit <= TimeUnit::nanosecond;
}

bool valid_timezone(TimezoneMode mode) noexcept {
  return mode == TimezoneMode::instant || mode == TimezoneMode::local;
}

bool valid_utf8_mode(Utf8Validation mode) noexcept {
  return mode == Utf8Validation::strict || mode == Utf8Validation::trusted;
}

Status validate_parameters(
    LogicalTypeKind kind,
    const LogicalTypeParameters& parameters) {
  const auto no_parameters = std::holds_alternative<std::monostate>(parameters);
  switch (kind) {
    case LogicalTypeKind::boolean:
    case LogicalTypeKind::int8:
    case LogicalTypeKind::int16:
    case LogicalTypeKind::int32:
    case LogicalTypeKind::int64:
    case LogicalTypeKind::uint8:
    case LogicalTypeKind::uint16:
    case LogicalTypeKind::uint32:
    case LogicalTypeKind::uint64:
    case LogicalTypeKind::float32:
    case LogicalTypeKind::float64:
    case LogicalTypeKind::date:
    case LogicalTypeKind::binary:
    case LogicalTypeKind::uuid:
      if (no_parameters) {
        return {};
      }
      break;
    case LogicalTypeKind::decimal:
      if (const auto* decimal = std::get_if<DecimalParameters>(&parameters)) {
        const auto precision = static_cast<std::int16_t>(decimal->precision);
        if (precision >= 1 && precision <= 38 &&
            decimal->scale >= -precision && decimal->scale <= precision) {
          return {};
        }
      }
      break;
    case LogicalTypeKind::time:
    case LogicalTypeKind::duration:
      if (const auto* time = std::get_if<TimeParameters>(&parameters);
          time != nullptr && valid_time_unit(time->unit)) {
        return {};
      }
      break;
    case LogicalTypeKind::timestamp:
      if (const auto* timestamp =
              std::get_if<TimestampParameters>(&parameters);
          timestamp != nullptr && valid_time_unit(timestamp->unit) &&
          valid_timezone(timestamp->timezone)) {
        return {};
      }
      break;
    case LogicalTypeKind::string:
      if (const auto* string = std::get_if<StringParameters>(&parameters);
          string != nullptr && valid_utf8_mode(string->validation)) {
        return {};
      }
      break;
    case LogicalTypeKind::fixed_binary:
      if (const auto* fixed =
              std::get_if<FixedBinaryParameters>(&parameters);
          fixed != nullptr && fixed->byte_width != 0) {
        return {};
      }
      break;
  }

  return fail(
      ErrorCode::invalid_schema,
      Operation::validate_schema,
      std::string("invalid parameters for logical type ") +
          std::string(to_string(kind)));
}

}  // namespace

class LogicalType::Impl {
 public:
  Impl(LogicalTypeKind kind_value, LogicalTypeParameters parameters_value)
      : kind(kind_value), parameters(std::move(parameters_value)) {}

  LogicalTypeKind kind;
  LogicalTypeParameters parameters;
};

std::string_view to_string(LogicalTypeKind kind) noexcept {
  switch (kind) {
    case LogicalTypeKind::boolean: return "bool";
    case LogicalTypeKind::int8: return "int8";
    case LogicalTypeKind::int16: return "int16";
    case LogicalTypeKind::int32: return "int32";
    case LogicalTypeKind::int64: return "int64";
    case LogicalTypeKind::uint8: return "uint8";
    case LogicalTypeKind::uint16: return "uint16";
    case LogicalTypeKind::uint32: return "uint32";
    case LogicalTypeKind::uint64: return "uint64";
    case LogicalTypeKind::float32: return "float32";
    case LogicalTypeKind::float64: return "float64";
    case LogicalTypeKind::decimal: return "decimal";
    case LogicalTypeKind::date: return "date";
    case LogicalTypeKind::time: return "time";
    case LogicalTypeKind::timestamp: return "timestamp";
    case LogicalTypeKind::duration: return "duration";
    case LogicalTypeKind::string: return "string";
    case LogicalTypeKind::binary: return "binary";
    case LogicalTypeKind::fixed_binary: return "fixed_binary";
    case LogicalTypeKind::uuid: return "uuid";
  }
  return "unknown";
}

Result<LogicalType> LogicalType::create(
    LogicalTypeKind kind,
    LogicalTypeParameters parameters) {
  if (kind > LogicalTypeKind::uuid) {
    return fail(
        ErrorCode::invalid_schema,
        Operation::validate_schema,
        "unknown logical type");
  }
  if (auto status = validate_parameters(kind, parameters); !status) {
    return std::unexpected(std::move(status.error()));
  }
  try {
    return LogicalType(
        std::make_unique<Impl>(kind, std::move(parameters)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_schema,
        "unable to allocate logical type");
  }
}

LogicalType::LogicalType(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

LogicalType::~LogicalType() = default;
LogicalType::LogicalType(const LogicalType& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {}
LogicalType& LogicalType::operator=(const LogicalType& other) {
  if (this != &other) {
    auto replacement = std::make_unique<Impl>(*other.impl_);
    impl_ = std::move(replacement);
  }
  return *this;
}
LogicalType::LogicalType(LogicalType&& other) noexcept = default;
LogicalType& LogicalType::operator=(LogicalType&& other) noexcept = default;

LogicalTypeKind LogicalType::kind() const noexcept { return impl_->kind; }
LogicalTypeParameters LogicalType::parameters() const {
  return impl_->parameters;
}

bool LogicalType::is_variable_width() const noexcept {
  return impl_->kind == LogicalTypeKind::string ||
         impl_->kind == LogicalTypeKind::binary;
}

bool LogicalType::is_fixed_width() const noexcept {
  return !is_variable_width();
}

std::uint32_t LogicalType::fixed_width_bytes() const noexcept {
  switch (impl_->kind) {
    case LogicalTypeKind::boolean: return 0;
    case LogicalTypeKind::int8:
    case LogicalTypeKind::uint8: return 1;
    case LogicalTypeKind::int16:
    case LogicalTypeKind::uint16: return 2;
    case LogicalTypeKind::int32:
    case LogicalTypeKind::uint32:
    case LogicalTypeKind::float32:
    case LogicalTypeKind::date: return 4;
    case LogicalTypeKind::int64:
    case LogicalTypeKind::uint64:
    case LogicalTypeKind::float64:
    case LogicalTypeKind::time:
    case LogicalTypeKind::timestamp:
    case LogicalTypeKind::duration: return 8;
    case LogicalTypeKind::decimal: return 16;
    case LogicalTypeKind::fixed_binary:
      return std::get<FixedBinaryParameters>(impl_->parameters).byte_width;
    case LogicalTypeKind::uuid: return 16;
    case LogicalTypeKind::string:
    case LogicalTypeKind::binary: return 0;
  }
  return 0;
}

bool LogicalType::operator==(const LogicalType& other) const noexcept {
  return impl_->kind == other.impl_->kind &&
         impl_->parameters == other.impl_->parameters;
}

}  // namespace shibori::engine
