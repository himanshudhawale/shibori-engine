#include "record.hpp"

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

}  // namespace

int main() {
  using namespace shibori::engine;
  using namespace shibori::engine::detail;

  const RecordEnvelope envelope{
      .type = RecordType::data_block,
      .flags = record_mandatory_flag,
      .extension_length = 12,
      .payload_length = 4096,
      .sequence = 42,
      .payload_checksum = Crc32c(0x12345678),
  };
  const auto encoded = encode_record_envelope(envelope);
  if (!expect(encoded.has_value(), "Test envelope could not be encoded")) {
    return 1;
  }

  constexpr RecordEnvelopeLimits limits{
      .maximum_extension_length = 12,
      .maximum_payload_length = 4096,
  };
  const auto parsed = parse_record_envelope(*encoded, limits);
  if (!expect(
          parsed && *parsed == envelope,
          "Valid record envelope did not round-trip")) {
    return 1;
  }

  for (std::size_t size = 0; size < record_envelope_size; ++size) {
    const auto truncated = parse_record_envelope(
        std::span<const std::byte>(*encoded).first(size), limits);
    if (!expect(
            !truncated &&
                truncated.error().code() == ErrorCode::unexpected_end,
            "Truncated record envelope was accepted")) {
      return 1;
    }
  }

  auto bad_sync = *encoded;
  bad_sync[0] = std::byte{'X'};
  const auto invalid_sync = parse_record_envelope(bad_sync, limits);

  auto bad_checksum = *encoded;
  bad_checksum[16] ^= std::byte{0x01};
  const auto invalid_checksum = parse_record_envelope(bad_checksum, limits);

  auto reserved_flags = *encoded;
  reserved_flags[5] = std::byte{0x02};
  const auto invalid_flags = parse_record_envelope(reserved_flags, limits);

  const RecordEnvelopeLimits short_extension{
      .maximum_extension_length = 11,
      .maximum_payload_length = 4096,
  };
  const RecordEnvelopeLimits short_payload{
      .maximum_extension_length = 12,
      .maximum_payload_length = 4095,
  };
  const auto extension_limited =
      parse_record_envelope(*encoded, short_extension);
  const auto payload_limited = parse_record_envelope(*encoded, short_payload);

  const RecordEnvelope overflowing{
      .type = RecordType::data_block,
      .flags = record_mandatory_flag,
      .extension_length = 1,
      .payload_length = std::numeric_limits<std::uint64_t>::max(),
      .sequence = 43,
      .payload_checksum = Crc32c(),
  };
  const auto overflowing_bytes = encode_record_envelope(overflowing);
  const RecordEnvelopeLimits unlimited{
      .maximum_extension_length =
          std::numeric_limits<std::uint16_t>::max(),
      .maximum_payload_length =
          std::numeric_limits<std::uint64_t>::max(),
  };
  const auto overflow =
      parse_record_envelope(*overflowing_bytes, unlimited);

  return expect(
             !invalid_sync &&
                 invalid_sync.error().code() == ErrorCode::invalid_magic,
             "Invalid record sync was not identified") &&
                 expect(
                     !invalid_checksum &&
                         invalid_checksum.error().code() ==
                             ErrorCode::checksum_mismatch,
                     "Invalid envelope checksum was not identified") &&
                 expect(
                     !invalid_flags &&
                         invalid_flags.error().code() ==
                             ErrorCode::invalid_record,
                     "Reserved record flags were accepted") &&
                 expect(
                     !extension_limited &&
                         extension_limited.error().code() ==
                             ErrorCode::limit_exceeded,
                     "Extension limit was not enforced") &&
                 expect(
                     !payload_limited &&
                         payload_limited.error().code() ==
                             ErrorCode::limit_exceeded,
                     "Payload limit was not enforced") &&
                 expect(
                     !overflow &&
                         overflow.error().code() ==
                             ErrorCode::arithmetic_overflow,
                     "Complete record length overflow was not identified")
             ? 0
             : 1;
}
