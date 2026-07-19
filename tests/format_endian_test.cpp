#include "format/endian.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

template <typename Value, std::size_t Size, typename Encode, typename Decode>
bool round_trip(Value value, Encode encode, Decode decode) {
  const std::array<std::byte, Size> bytes = encode(value);
  const auto decoded = decode(
      std::span<const std::byte>(bytes),
      shibori::engine::Operation::parse,
      "test integer");
  return decoded && *decoded == value;
}

}  // namespace

int main() {
  using namespace shibori::engine;
  using namespace shibori::engine::detail;

  constexpr auto u32_bytes = encode_u32_le(0x12345678U);
  static_assert(u32_bytes[0] == std::byte{0x78});
  static_assert(u32_bytes[1] == std::byte{0x56});
  static_assert(u32_bytes[2] == std::byte{0x34});
  static_assert(u32_bytes[3] == std::byte{0x12});

  const auto valid =
      round_trip<std::uint16_t, 2>(
          0, encode_u16_le, decode_u16_le) &&
      round_trip<std::uint16_t, 2>(
          std::numeric_limits<std::uint16_t>::max(),
          encode_u16_le,
          decode_u16_le) &&
      round_trip<std::uint32_t, 4>(
          0x12345678U, encode_u32_le, decode_u32_le) &&
      round_trip<std::uint64_t, 8>(
          0x0123456789abcdefULL, encode_u64_le, decode_u64_le) &&
      round_trip<std::uint64_t, 8>(
          std::numeric_limits<std::uint64_t>::max(),
          encode_u64_le,
          decode_u64_le);

  if (!expect(valid, "Little-endian integer round-trip failed")) {
    return 1;
  }

  const std::array<std::byte, 7> short_bytes{};
  const auto short_u16 = decode_u16_le(
      std::span<const std::byte>(short_bytes).first(1),
      Operation::parse,
      "u16");
  const auto short_u32 = decode_u32_le(
      std::span<const std::byte>(short_bytes).first(3),
      Operation::parse,
      "u32");
  const auto short_u64 = decode_u64_le(
      short_bytes,
      Operation::parse,
      "u64");

  return expect(
             !short_u16 &&
                 short_u16.error().code() == ErrorCode::unexpected_end,
             "Short u16 input was accepted") &&
                 expect(!short_u32, "Short u32 input was accepted") &&
                 expect(!short_u64, "Short u64 input was accepted")
             ? 0
             : 1;
}
