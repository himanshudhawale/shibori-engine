#include <shibori/engine/integrity.hpp>

#include <array>
#include <string>

namespace shibori::engine {
namespace {

constexpr char hex_digits[] = "0123456789abcdef";

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
