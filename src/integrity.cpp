#include <shibori/engine/integrity.hpp>

#include "crc32c_backend.hpp"

#include <array>
#include <string>

namespace shibori::engine {
namespace {

constexpr char hex_digits[] = "0123456789abcdef";
constexpr std::uint32_t crc32c_polynomial = 0x82f63b78U;

constexpr std::array<std::uint32_t, 256> make_crc32c_table() noexcept {
  std::array<std::uint32_t, 256> table{};
  for (std::size_t index = 0; index < table.size(); ++index) {
    auto remainder = static_cast<std::uint32_t>(index);
    for (int bit = 0; bit < 8; ++bit) {
      const auto mask = 0U - (remainder & 1U);
      remainder = (remainder >> 1U) ^ (crc32c_polynomial & mask);
    }
    table[index] = remainder;
  }
  return table;
}

constexpr auto crc32c_table = make_crc32c_table();

int decode_hex(char value) noexcept {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

}  // namespace

namespace detail {

std::uint32_t portable_crc32c_update(
    std::uint32_t state,
    std::span<const std::byte> bytes) noexcept {
  for (const auto byte : bytes) {
    const auto index =
        (state ^ static_cast<std::uint32_t>(byte)) & 0xffU;
    state = crc32c_table[index] ^ (state >> 8U);
  }
  return state;
}

}  // namespace detail

Crc32cHasher::Crc32cHasher(Crc32cMode mode) noexcept {
#if defined(SHIBORI_HAS_X86_CRC32C)
  if (mode == Crc32cMode::automatic && detail::x86_crc32c_available()) {
    implementation_ = Crc32cImplementation::hardware;
  }
#else
  static_cast<void>(mode);
#endif
}

bool Crc32cHasher::hardware_available() noexcept {
#if defined(SHIBORI_HAS_X86_CRC32C)
  return detail::x86_crc32c_available();
#else
  return false;
#endif
}

void Crc32cHasher::update(std::span<const std::byte> bytes) noexcept {
#if defined(SHIBORI_HAS_X86_CRC32C)
  if (implementation_ == Crc32cImplementation::hardware) {
    state_ = detail::x86_crc32c_update(state_, bytes);
    return;
  }
#endif
  state_ = detail::portable_crc32c_update(state_, bytes);
}

std::string Blake3Digest::to_hex() const {
  std::string output(hex_size, '0');
  for (std::size_t index = 0; index < bytes_.size(); ++index) {
    const auto value = static_cast<unsigned int>(bytes_[index]);
    output[index * 2] = hex_digits[value >> 4U];
    output[(index * 2) + 1] = hex_digits[value & 0x0fU];
  }
  return output;
}

Result<Blake3Digest> Blake3Digest::from_hex(std::string_view text) {
  if (text.size() != hex_size) {
    return fail(
        ErrorCode::invalid_digest_text,
        Operation::parse,
        "BLAKE3-256 digest text must contain exactly 64 hexadecimal characters");
  }

  std::array<std::byte, byte_size> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto high = decode_hex(text[index * 2]);
    const auto low = decode_hex(text[(index * 2) + 1]);
    if (high < 0 || low < 0) {
      return fail(
          ErrorCode::invalid_digest_text,
          Operation::parse,
          "BLAKE3-256 digest text contains a non-hexadecimal character");
    }
    bytes[index] = static_cast<std::byte>((high << 4) | low);
  }
  return Blake3Digest(bytes);
}

}  // namespace shibori::engine
