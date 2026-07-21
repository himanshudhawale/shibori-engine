#include "record.hpp"

#include <array>
#include <cstddef>
#include <iostream>

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
      .type = RecordType::file_header,
      .flags = record_mandatory_flag,
      .extension_length = 0x0203,
      .payload_length = 0x0102030405060708,
      .sequence = 0,
      .payload_checksum = Crc32c(0xe3069283),
  };
  constexpr std::array<std::byte, record_envelope_size> expected{
      std::byte{0x53}, std::byte{0x48}, std::byte{0x52}, std::byte{0x31},
      std::byte{0x01}, std::byte{0x01}, std::byte{0x03}, std::byte{0x02},
      std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
      std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x6d}, std::byte{0xa3}, std::byte{0xa0}, std::byte{0xba},
      std::byte{0x83}, std::byte{0x92}, std::byte{0x06}, std::byte{0xe3},
  };

  const auto first = encode_record_envelope(envelope);
  const auto second = encode_record_envelope(envelope);
  if (!expect(first.has_value(), "Valid record envelope was rejected") ||
      !expect(second.has_value(), "Repeated record envelope was rejected")) {
    return 1;
  }
  if (!expect(*first == expected, "Envelope bytes do not match golden bytes") ||
      !expect(*first == *second, "Envelope encoding is not deterministic") ||
      !expect(envelope.mandatory(), "Mandatory flag was not represented")) {
    return 1;
  }

  auto invalid = envelope;
  invalid.flags = 0x02;
  const auto rejected = encode_record_envelope(invalid);
  return expect(
             !rejected && rejected.error().code() == ErrorCode::invalid_record &&
                 rejected.error().operation() == Operation::encode,
             "Reserved record flags were not rejected")
             ? 0
             : 1;
}
