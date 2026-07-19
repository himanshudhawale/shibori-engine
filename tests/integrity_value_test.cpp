#include <shibori/engine/integrity.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

}  // namespace

int main() {
  using shibori::engine::Blake3Digest;
  using shibori::engine::Crc32c;
  using shibori::engine::ErrorCode;

  constexpr auto crc = Crc32c(0x12345678U);
  constexpr auto crc_bytes = crc.little_endian_bytes();
  static_assert(crc_bytes[0] == std::byte{0x78});
  static_assert(crc_bytes[1] == std::byte{0x56});
  static_assert(crc_bytes[2] == std::byte{0x34});
  static_assert(crc_bytes[3] == std::byte{0x12});
  static_assert(Crc32c::from_little_endian(crc_bytes) == crc);

  std::array<std::byte, Blake3Digest::byte_size> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::byte>(index);
  }

  const Blake3Digest digest(bytes);
  const std::string expected =
      "000102030405060708090a0b0c0d0e0f"
      "101112131415161718191a1b1c1d1e1f";
  if (!expect(digest.to_hex() == expected, "Digest hex output is not canonical")) {
    return 1;
  }

  const auto lowercase = Blake3Digest::from_hex(expected);
  const auto uppercase = Blake3Digest::from_hex(
      "000102030405060708090A0B0C0D0E0F"
      "101112131415161718191A1B1C1D1E1F");
  const auto short_text = Blake3Digest::from_hex("00");
  const auto invalid_text = Blake3Digest::from_hex(
      "g00102030405060708090a0b0c0d0e0f"
      "101112131415161718191a1b1c1d1e1f");

  return expect(lowercase && *lowercase == digest, "Lowercase hex did not round-trip") &&
                 expect(uppercase && *uppercase == digest, "Uppercase hex did not parse") &&
                 expect(
                     !short_text &&
                         short_text.error().code() ==
                             ErrorCode::invalid_digest_text,
                     "Short digest text was accepted") &&
                 expect(
                     !invalid_text &&
                         invalid_text.error().code() ==
                             ErrorCode::invalid_digest_text,
                     "Invalid digest text was accepted")
             ? 0
             : 1;
}
