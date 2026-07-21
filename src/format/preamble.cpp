#include "preamble.hpp"

#include "endian.hpp"

#include <shibori/engine/integrity.hpp>

#include <array>
#include <span>

namespace shibori::engine::detail {
namespace {

constexpr std::array<std::byte, 8> file_magic{
    std::byte{'S'},
    std::byte{'H'},
    std::byte{'I'},
    std::byte{'B'},
    std::byte{'O'},
    std::byte{'R'},
    std::byte{'I'},
    std::byte{0},
};

}  // namespace

std::array<std::byte, file_preamble_size> encode_file_preamble(
    std::uint16_t major,
    std::uint16_t minor) noexcept {
  std::array<std::byte, file_preamble_size> output{};

  for (std::size_t index = 0; index < file_magic.size(); ++index) {
    output[index] = file_magic[index];
  }

  const auto major_bytes = encode_u16_le(major);
  output[8] = major_bytes[0];
  output[9] = major_bytes[1];

  const auto minor_bytes = encode_u16_le(minor);
  output[10] = minor_bytes[0];
  output[11] = minor_bytes[1];

  Crc32cHasher checksum(Crc32cMode::portable);
  checksum.update(std::span<const std::byte>(output).first(12));
  const auto checksum_bytes = checksum.finalize().little_endian_bytes();
  for (std::size_t index = 0; index < checksum_bytes.size(); ++index) {
    output[12 + index] = checksum_bytes[index];
  }

  return output;
}

}  // namespace shibori::engine::detail
