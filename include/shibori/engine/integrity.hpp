#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <shibori/engine/export.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine {

class Crc32c {
 public:
  constexpr explicit Crc32c(std::uint32_t value = 0) noexcept : value_(value) {}

  [[nodiscard]] constexpr std::uint32_t value() const noexcept {
    return value_;
  }

  [[nodiscard]] constexpr std::array<std::byte, 4> little_endian_bytes()
      const noexcept {
    return {
        static_cast<std::byte>(value_ & 0xffU),
        static_cast<std::byte>((value_ >> 8U) & 0xffU),
        static_cast<std::byte>((value_ >> 16U) & 0xffU),
        static_cast<std::byte>((value_ >> 24U) & 0xffU),
    };
  }

  [[nodiscard]] static constexpr Crc32c from_little_endian(
      std::span<const std::byte, 4> bytes) noexcept {
    const auto byte = [](std::byte value) {
      return static_cast<std::uint32_t>(value);
    };
    return Crc32c(
        byte(bytes[0]) |
        (byte(bytes[1]) << 8U) |
        (byte(bytes[2]) << 16U) |
        (byte(bytes[3]) << 24U));
  }

  constexpr bool operator==(const Crc32c&) const noexcept = default;

 private:
  std::uint32_t value_;
};

class Blake3Digest {
 public:
  static constexpr std::size_t byte_size = 32;
  static constexpr std::size_t hex_size = byte_size * 2;

  constexpr Blake3Digest() noexcept = default;
  constexpr explicit Blake3Digest(
      std::array<std::byte, byte_size> bytes) noexcept
      : bytes_(bytes) {}

  [[nodiscard]] constexpr const std::array<std::byte, byte_size>& bytes()
      const noexcept {
    return bytes_;
  }

  [[nodiscard]] SHIBORI_ENGINE_API std::string to_hex() const;
  [[nodiscard]] SHIBORI_ENGINE_API static Result<Blake3Digest> from_hex(
      std::string_view text);

  constexpr bool operator==(const Blake3Digest&) const noexcept = default;

 private:
  std::array<std::byte, byte_size> bytes_{};
};

}  // namespace shibori::engine
