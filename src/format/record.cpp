#include "record.hpp"

#include "endian.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace shibori::engine::detail {
namespace {

constexpr std::array<std::byte, 4> record_sync{
    std::byte{'S'},
    std::byte{'H'},
    std::byte{'R'},
    std::byte{'1'},
};

template <std::size_t Size>
void write_bytes(
    std::array<std::byte, record_envelope_size>& output,
    std::size_t offset,
    const std::array<std::byte, Size>& bytes) {
  std::copy(bytes.begin(), bytes.end(), output.begin() + offset);
}

}  // namespace

Result<std::array<std::byte, record_envelope_size>> encode_record_envelope(
    const RecordEnvelope& envelope) {
  if ((envelope.flags & record_reserved_flags) != 0) {
    return fail(
        ErrorCode::invalid_record,
        Operation::encode,
        "record envelope uses reserved format 1 flag bits");
  }

  std::array<std::byte, record_envelope_size> output{};
  write_bytes(output, 0, record_sync);
  output[4] = static_cast<std::byte>(envelope.type);
  output[5] = static_cast<std::byte>(envelope.flags);
  write_bytes(output, 6, encode_u16_le(envelope.extension_length));
  write_bytes(output, 8, encode_u64_le(envelope.payload_length));
  write_bytes(output, 16, encode_u64_le(envelope.sequence));

  Crc32cHasher checksum(Crc32cMode::portable);
  checksum.update(std::span<const std::byte>(output).first(24));
  write_bytes(output, 24, checksum.finalize().little_endian_bytes());
  write_bytes(output, 28, envelope.payload_checksum.little_endian_bytes());
  return output;
}

}  // namespace shibori::engine::detail
