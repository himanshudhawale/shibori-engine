#include <shibori/engine/checked_arithmetic.hpp>

#include <limits>
#include <string>

namespace shibori::engine {

Result<std::uint64_t> checked_add(
    std::uint64_t left,
    std::uint64_t right,
    Operation operation,
    std::string_view subject) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return std::unexpected(Error(
        ErrorCode::arithmetic_overflow,
        operation,
        std::string(subject) + " exceeds the unsigned 64-bit range"));
  }
  return left + right;
}

Result<std::uint64_t> checked_multiply(
    std::uint64_t left,
    std::uint64_t right,
    Operation operation,
    std::string_view subject) {
  if (left != 0 &&
      right > std::numeric_limits<std::uint64_t>::max() / left) {
    return std::unexpected(Error(
        ErrorCode::arithmetic_overflow,
        operation,
        std::string(subject) + " exceeds the unsigned 64-bit range"));
  }
  return left * right;
}

Status checked_range(
    std::uint64_t offset,
    std::uint64_t length,
    std::uint64_t total_size,
    Operation operation,
    std::string_view subject) {
  if (offset > total_size || length > total_size - offset) {
    return std::unexpected(Error(
        ErrorCode::range_exceeded,
        operation,
        std::string(subject) + " falls outside the bounded region"));
  }
  return {};
}

}  // namespace shibori::engine
