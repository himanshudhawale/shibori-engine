#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <shibori/engine/result.hpp>

namespace shibori::engine::detail {

[[nodiscard]] constexpr std::array<std::byte, 2> encode_u16_le(
    std::uint16_t value) noexcept {
  return {
      static_cast<std::byte>(value & 0xffU),
      static_cast<std::byte>((value >> 8U) & 0xffU),
  };
}

[[nodiscard]] constexpr std::array<std::byte, 4> encode_u32_le(
    std::uint32_t value) noexcept {
  return {
      static_cast<std::byte>(value & 0xffU),
      static_cast<std::byte>((value >> 8U) & 0xffU),
      static_cast<std::byte>((value >> 16U) & 0xffU),
      static_cast<std::byte>((value >> 24U) & 0xffU),
  };
}

[[nodiscard]] constexpr std::array<std::byte, 8> encode_u64_le(
    std::uint64_t value) noexcept {
  return {
      static_cast<std::byte>(value & 0xffU),
      static_cast<std::byte>((value >> 8U) & 0xffU),
      static_cast<std::byte>((value >> 16U) & 0xffU),
      static_cast<std::byte>((value >> 24U) & 0xffU),
      static_cast<std::byte>((value >> 32U) & 0xffU),
      static_cast<std::byte>((value >> 40U) & 0xffU),
      static_cast<std::byte>((value >> 48U) & 0xffU),
      static_cast<std::byte>((value >> 56U) & 0xffU),
  };
}

[[nodiscard]] Result<std::uint16_t> decode_u16_le(
    std::span<const std::byte> bytes,
    Operation operation,
    std::string_view subject);

[[nodiscard]] Result<std::uint32_t> decode_u32_le(
    std::span<const std::byte> bytes,
    Operation operation,
    std::string_view subject);

[[nodiscard]] Result<std::uint64_t> decode_u64_le(
    std::span<const std::byte> bytes,
    Operation operation,
    std::string_view subject);

}  // namespace shibori::engine::detail
