#include "endian.hpp"

#include <string>
#include <utility>

namespace shibori::engine::detail {
namespace {

Status require_bytes(
    std::span<const std::byte> bytes,
    std::size_t required,
    Operation operation,
    std::string_view subject) {
  if (bytes.size() < required) {
    return fail(
        ErrorCode::unexpected_end,
        operation,
        std::string(subject) + " is truncated");
  }
  return {};
}

constexpr std::uint64_t byte_value(std::byte value) noexcept {
  return static_cast<std::uint64_t>(value);
}

}  // namespace

Result<std::uint16_t> decode_u16_le(
    std::span<const std::byte> bytes,
    Operation operation,
    std::string_view subject) {
  if (auto status = require_bytes(bytes, 2, operation, subject); !status) {
    return std::unexpected(std::move(status.error()));
  }
  return static_cast<std::uint16_t>(
      byte_value(bytes[0]) | (byte_value(bytes[1]) << 8U));
}

Result<std::uint32_t> decode_u32_le(
    std::span<const std::byte> bytes,
    Operation operation,
    std::string_view subject) {
  if (auto status = require_bytes(bytes, 4, operation, subject); !status) {
    return std::unexpected(std::move(status.error()));
  }
  return static_cast<std::uint32_t>(
      byte_value(bytes[0]) |
      (byte_value(bytes[1]) << 8U) |
      (byte_value(bytes[2]) << 16U) |
      (byte_value(bytes[3]) << 24U));
}

Result<std::uint64_t> decode_u64_le(
    std::span<const std::byte> bytes,
    Operation operation,
    std::string_view subject) {
  if (auto status = require_bytes(bytes, 8, operation, subject); !status) {
    return std::unexpected(std::move(status.error()));
  }
  return byte_value(bytes[0]) |
         (byte_value(bytes[1]) << 8U) |
         (byte_value(bytes[2]) << 16U) |
         (byte_value(bytes[3]) << 24U) |
         (byte_value(bytes[4]) << 32U) |
         (byte_value(bytes[5]) << 40U) |
         (byte_value(bytes[6]) << 48U) |
         (byte_value(bytes[7]) << 56U);
}

}  // namespace shibori::engine::detail
