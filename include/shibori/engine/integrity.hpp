#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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

enum class Crc32cMode : std::uint8_t {
  automatic,
  portable,
};

enum class Crc32cImplementation : std::uint8_t {
  portable,
  hardware,
};

class Crc32cHasher {
 public:
  SHIBORI_ENGINE_API explicit Crc32cHasher(
      Crc32cMode mode = Crc32cMode::automatic) noexcept;

  SHIBORI_ENGINE_API void update(std::span<const std::byte> bytes) noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API static bool hardware_available() noexcept;
  [[nodiscard]] constexpr Crc32cImplementation implementation() const noexcept {
    return implementation_;
  }

  [[nodiscard]] constexpr Crc32c finalize() const noexcept {
    return Crc32c(state_ ^ 0xffffffffU);
  }

  constexpr void reset() noexcept {
    state_ = 0xffffffffU;
  }

 private:
  std::uint32_t state_ = 0xffffffffU;
  Crc32cImplementation implementation_ = Crc32cImplementation::portable;
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

[[nodiscard]] SHIBORI_ENGINE_API std::string_view blake3_version() noexcept;

class Blake3Hasher {
 public:
  SHIBORI_ENGINE_API ~Blake3Hasher();

  Blake3Hasher(const Blake3Hasher&) = delete;
  Blake3Hasher& operator=(const Blake3Hasher&) = delete;
  SHIBORI_ENGINE_API Blake3Hasher(Blake3Hasher&& other) noexcept;
  SHIBORI_ENGINE_API Blake3Hasher& operator=(Blake3Hasher&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API static Result<Blake3Hasher> create();
  SHIBORI_ENGINE_API void update(std::span<const std::byte> bytes) noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API Blake3Digest finalize() const noexcept;
  SHIBORI_ENGINE_API void reset() noexcept;

 private:
  class Impl;
  explicit Blake3Hasher(std::unique_ptr<Impl> impl) noexcept;

  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
