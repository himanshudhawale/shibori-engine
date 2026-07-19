#include <shibori/engine/error.hpp>

#include <sstream>
#include <utility>

namespace shibori::engine {
namespace {

void append_description(std::ostringstream& output, const Error& error) {
  output << to_string(error.category()) << '/' << to_string(error.code())
         << " during " << to_string(error.operation()) << ": "
         << error.message();

  if (error.byte_offset()) {
    output << " [offset=" << *error.byte_offset() << ']';
  }
  if (error.block_id()) {
    output << " [block=" << *error.block_id() << ']';
  }
  if (error.field_id()) {
    output << " [field=" << *error.field_id() << ']';
  }
  if (error.component_id()) {
    output << " [component=" << *error.component_id() << ']';
  }
  if (error.cause() != nullptr) {
    output << "; caused by ";
    append_description(output, *error.cause());
  }
}

}  // namespace

class Error::Impl {
 public:
  Impl(ErrorCode code_value, Operation operation_value, std::string message_value)
      : code(code_value),
        operation(operation_value),
        message(std::move(message_value)) {}

  ErrorCode code;
  Operation operation;
  std::string message;
  std::optional<std::uint64_t> byte_offset;
  std::optional<std::uint64_t> block_id;
  std::optional<std::uint32_t> field_id;
  std::optional<std::uint32_t> component_id;
  std::shared_ptr<const Error> cause;
};

std::string_view to_string(ErrorCategory category) noexcept {
  switch (category) {
    case ErrorCategory::invalid_argument:
      return "invalid_argument";
    case ErrorCategory::invalid_data:
      return "invalid_data";
    case ErrorCategory::io:
      return "io";
    case ErrorCategory::truncated:
      return "truncated";
    case ErrorCategory::corrupt:
      return "corrupt";
    case ErrorCategory::unsupported:
      return "unsupported";
    case ErrorCategory::resource_limit:
      return "resource_limit";
    case ErrorCategory::cancelled:
      return "cancelled";
    case ErrorCategory::plugin:
      return "plugin";
    case ErrorCategory::internal:
      return "internal";
  }
  return "internal";
}

std::string_view to_string(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::invalid_configuration:
      return "invalid_configuration";
    case ErrorCode::invalid_state:
      return "invalid_state";
    case ErrorCode::invalid_resource_limit:
      return "invalid_resource_limit";
    case ErrorCode::invalid_schema:
      return "invalid_schema";
    case ErrorCode::invalid_block:
      return "invalid_block";
    case ErrorCode::io_read_failed:
      return "io_read_failed";
    case ErrorCode::io_write_failed:
      return "io_write_failed";
    case ErrorCode::io_no_progress:
      return "io_no_progress";
    case ErrorCode::unexpected_end:
      return "unexpected_end";
    case ErrorCode::checksum_mismatch:
      return "checksum_mismatch";
    case ErrorCode::malformed_container:
      return "malformed_container";
    case ErrorCode::invalid_record:
      return "invalid_record";
    case ErrorCode::unsupported_format_version:
      return "unsupported_format_version";
    case ErrorCode::unsupported_feature:
      return "unsupported_feature";
    case ErrorCode::missing_component:
      return "missing_component";
    case ErrorCode::limit_exceeded:
      return "limit_exceeded";
    case ErrorCode::allocation_failed:
      return "allocation_failed";
    case ErrorCode::arithmetic_overflow:
      return "arithmetic_overflow";
    case ErrorCode::range_exceeded:
      return "range_exceeded";
    case ErrorCode::operation_cancelled:
      return "operation_cancelled";
    case ErrorCode::plugin_contract_violation:
      return "plugin_contract_violation";
    case ErrorCode::plugin_failure:
      return "plugin_failure";
    case ErrorCode::invariant_violation:
      return "invariant_violation";
  }
  return "invariant_violation";
}

std::string_view to_string(Operation operation) noexcept {
  switch (operation) {
    case Operation::unknown:
      return "unknown";
    case Operation::configure:
      return "configure";
    case Operation::validate_schema:
      return "validate_schema";
    case Operation::validate_block:
      return "validate_block";
    case Operation::read:
      return "read";
    case Operation::write:
      return "write";
    case Operation::parse:
      return "parse";
    case Operation::encode:
      return "encode";
    case Operation::decode:
      return "decode";
    case Operation::finalize:
      return "finalize";
    case Operation::inspect:
      return "inspect";
    case Operation::verify:
      return "verify";
    case Operation::load_plugin:
      return "load_plugin";
  }
  return "unknown";
}

Error::Error(ErrorCode code, Operation operation, std::string message)
    : impl_(std::make_unique<Impl>(code, operation, std::move(message))) {}

Error::~Error() = default;

Error::Error(const Error& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

Error& Error::operator=(const Error& other) {
  if (this != &other) {
    impl_ = std::make_unique<Impl>(*other.impl_);
  }
  return *this;
}

Error::Error(Error&& other) noexcept = default;

Error& Error::operator=(Error&& other) noexcept = default;

ErrorCategory Error::category() const noexcept {
  return category_for(impl_->code);
}

ErrorCode Error::code() const noexcept {
  return impl_->code;
}

Operation Error::operation() const noexcept {
  return impl_->operation;
}

const std::string& Error::message() const noexcept {
  return impl_->message;
}

std::optional<std::uint64_t> Error::byte_offset() const noexcept {
  return impl_->byte_offset;
}

std::optional<std::uint64_t> Error::block_id() const noexcept {
  return impl_->block_id;
}

std::optional<std::uint32_t> Error::field_id() const noexcept {
  return impl_->field_id;
}

std::optional<std::uint32_t> Error::component_id() const noexcept {
  return impl_->component_id;
}

const Error* Error::cause() const noexcept {
  return impl_->cause.get();
}

Error Error::with_byte_offset(std::uint64_t value) && {
  impl_->byte_offset = value;
  return std::move(*this);
}

Error Error::with_block_id(std::uint64_t value) && {
  impl_->block_id = value;
  return std::move(*this);
}

Error Error::with_field_id(std::uint32_t value) && {
  impl_->field_id = value;
  return std::move(*this);
}

Error Error::with_component_id(std::uint32_t value) && {
  impl_->component_id = value;
  return std::move(*this);
}

Error Error::with_cause(Error value) && {
  impl_->cause = std::make_shared<Error>(std::move(value));
  return std::move(*this);
}

std::string Error::describe() const {
  std::ostringstream output;
  append_description(output, *this);
  return output.str();
}

}  // namespace shibori::engine
