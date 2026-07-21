#include "record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

shibori::engine::Result<shibori::engine::detail::VerifiedRecord> make_record(
    shibori::engine::detail::RecordType type,
    std::uint8_t flags,
    std::uint64_t sequence,
    std::span<const std::byte> payload) {
  using namespace shibori::engine;
  using namespace shibori::engine::detail;

  Crc32cHasher payload_checksum(Crc32cMode::portable);
  payload_checksum.update(payload);
  const RecordEnvelope envelope{
      .type = type,
      .flags = flags,
      .extension_length = 0,
      .payload_length = payload.size(),
      .sequence = sequence,
      .payload_checksum = payload_checksum.finalize(),
  };
  const auto encoded = encode_record_envelope(envelope);
  constexpr RecordEnvelopeLimits limits{
      .maximum_extension_length = 0,
      .maximum_payload_length = 64,
  };
  return verify_record(*encoded, {}, payload, limits);
}

}  // namespace

int main() {
  using namespace shibori::engine;
  using namespace shibori::engine::detail;

  constexpr std::array payload{
      std::byte{'d'},
      std::byte{'a'},
      std::byte{'t'},
      std::byte{'a'},
  };
  auto known = make_record(
      RecordType::file_header, record_mandatory_flag, 0, payload);
  auto optional =
      make_record(static_cast<RecordType>(0x40), 0, 1, payload);
  auto mandatory = make_record(
      static_cast<RecordType>(0x41), record_mandatory_flag, 2, payload);
  if (!expect(known.has_value(), "Known record verification failed") ||
      !expect(optional.has_value(), "Optional record verification failed") ||
      !expect(mandatory.has_value(), "Mandatory record verification failed")) {
    return 1;
  }

  RecordSequenceTracker tracker;
  const auto process = tracker.accept(*known);
  const auto skip = tracker.accept(*optional);
  const auto unsupported = tracker.accept(*mandatory);

  auto out_of_order =
      make_record(RecordType::schema, record_mandatory_flag, 3, payload);
  RecordSequenceTracker sequence_tracker;
  const auto mismatch = sequence_tracker.accept(*out_of_order);

  auto corrupted_payload = payload;
  corrupted_payload[0] ^= std::byte{0x01};
  Crc32cHasher original_checksum(Crc32cMode::portable);
  original_checksum.update(payload);
  const RecordEnvelope unverified_optional{
      .type = static_cast<RecordType>(0x42),
      .flags = 0,
      .extension_length = 0,
      .payload_length = corrupted_payload.size(),
      .sequence = 0,
      .payload_checksum = original_checksum.finalize(),
  };
  const auto encoded_optional =
      encode_record_envelope(unverified_optional);
  constexpr RecordEnvelopeLimits limits{
      .maximum_extension_length = 0,
      .maximum_payload_length = 64,
  };
  const auto not_verified =
      verify_record(*encoded_optional, {}, corrupted_payload, limits);

  return expect(
             process && *process == RecordDisposition::process,
             "Known record was not selected for processing") &&
                 expect(
                     skip && *skip == RecordDisposition::skip,
                     "Verified unknown optional record was not skippable") &&
                 expect(
                     !unsupported &&
                         unsupported.error().code() ==
                             ErrorCode::unsupported_feature &&
                         unsupported.error().component_id() ==
                             std::optional<std::uint32_t>(0x41),
                     "Unknown mandatory record was accepted") &&
                 expect(
                     !mismatch &&
                         mismatch.error().code() == ErrorCode::invalid_record,
                     "Record sequence mismatch was accepted") &&
                 expect(
                     !not_verified &&
                         not_verified.error().code() ==
                             ErrorCode::checksum_mismatch,
                     "Corrupt unknown optional record became skippable")
             ? 0
             : 1;
}
