#include "preamble.hpp"

#include "endian.hpp"

#include <shibori/engine/integrity.hpp>

#include <array>
#include <string>
#include <span>
#include <utility>

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

Result<FilePreamble> parse_file_preamble(
    std::span<const std::byte> bytes) {
  if (bytes.size() < file_preamble_size) {
    return fail(
        ErrorCode::unexpected_end,
        Operation::parse,
        "file preamble is truncated");
  }

  for (std::size_t index = 0; index < file_magic.size(); ++index) {
    if (bytes[index] != file_magic[index]) {
      return fail(
          ErrorCode::invalid_magic,
          Operation::parse,
          "file preamble magic does not match SHIBORI");
    }
  }

  auto major = decode_u16_le(bytes.subspan(8, 2), Operation::parse, "major version");
  if (!major) {
    return std::unexpected(std::move(major.error()));
  }
  auto minor = decode_u16_le(bytes.subspan(10, 2), Operation::parse, "minor version");
  if (!minor) {
    return std::unexpected(std::move(minor.error()));
  }

  const auto stored_checksum =
      Crc32c::from_little_endian(bytes.subspan<12, 4>());
  Crc32cHasher checksum(Crc32cMode::portable);
  checksum.update(bytes.first(12));
  if (stored_checksum != checksum.finalize()) {
    return fail(
        ErrorCode::checksum_mismatch,
        Operation::parse,
        "file preamble CRC32C does not match");
  }

  if (*major != current_format_major) {
    return fail(
        ErrorCode::unsupported_format_version,
        Operation::parse,
        "file preamble uses an unsupported major version");
  }

  return FilePreamble{
      .major = *major,
      .minor = *minor,
      .checksum = stored_checksum,
  };
}

}  // namespace shibori::engine::detail
