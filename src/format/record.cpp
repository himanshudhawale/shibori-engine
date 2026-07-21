#include "record.hpp"

#include "endian.hpp"

#include <shibori/engine/checked_arithmetic.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <utility>

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

Result<RecordEnvelope> parse_record_envelope(
    std::span<const std::byte> bytes,
    const RecordEnvelopeLimits& limits) {
  if (bytes.size() < record_envelope_size) {
    return fail(
        ErrorCode::unexpected_end,
        Operation::parse,
        "record envelope is truncated");
  }

  if (!std::equal(record_sync.begin(), record_sync.end(), bytes.begin())) {
    return fail(
        ErrorCode::invalid_magic,
        Operation::parse,
        "record envelope sync does not match SHR1");
  }

  const auto flags = static_cast<std::uint8_t>(bytes[5]);
  if ((flags & record_reserved_flags) != 0) {
    return fail(
        ErrorCode::invalid_record,
        Operation::parse,
        "record envelope uses reserved format 1 flag bits");
  }

  auto extension_length = decode_u16_le(
      bytes.subspan(6, 2), Operation::parse, "record extension length");
  if (!extension_length) {
    return std::unexpected(std::move(extension_length.error()));
  }
  auto payload_length = decode_u64_le(
      bytes.subspan(8, 8), Operation::parse, "record payload length");
  if (!payload_length) {
    return std::unexpected(std::move(payload_length.error()));
  }
  auto sequence = decode_u64_le(
      bytes.subspan(16, 8), Operation::parse, "record sequence");
  if (!sequence) {
    return std::unexpected(std::move(sequence.error()));
  }

  if (*extension_length > limits.maximum_extension_length) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::parse,
        "record extension length exceeds the configured limit");
  }
  if (*payload_length > limits.maximum_payload_length) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::parse,
        "record payload length exceeds the configured limit");
  }

  auto content_length = checked_add(
      *extension_length,
      *payload_length,
      Operation::parse,
      "record content length");
  if (!content_length) {
    return std::unexpected(std::move(content_length.error()));
  }
  auto total_length = checked_add(
      record_envelope_size,
      *content_length,
      Operation::parse,
      "complete record length");
  if (!total_length) {
    return std::unexpected(std::move(total_length.error()));
  }

  const auto stored_checksum =
      Crc32c::from_little_endian(bytes.subspan<24, 4>());
  Crc32cHasher checksum(Crc32cMode::portable);
  checksum.update(bytes.first(24));
  if (stored_checksum != checksum.finalize()) {
    return fail(
        ErrorCode::checksum_mismatch,
        Operation::parse,
        "record envelope CRC32C does not match");
  }

  return RecordEnvelope{
      .type = static_cast<RecordType>(bytes[4]),
      .flags = flags,
      .extension_length = *extension_length,
      .payload_length = *payload_length,
      .sequence = *sequence,
      .payload_checksum =
          Crc32c::from_little_endian(bytes.subspan<28, 4>()),
  };
}

}  // namespace shibori::engine::detail
