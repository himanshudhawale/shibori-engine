#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <shibori/engine/integrity.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine::detail {

inline constexpr std::size_t record_envelope_size = 32;
inline constexpr std::uint8_t record_mandatory_flag = 0x01;
inline constexpr std::uint8_t record_reserved_flags = 0xfe;

enum class RecordType : std::uint8_t {
  file_header = 0x01,
  schema = 0x02,
  data_block = 0x03,
  block_index = 0x04,
  file_footer = 0x05,
  user_metadata = 0x06,
};

struct RecordEnvelope {
  RecordType type;
  std::uint8_t flags;
  std::uint16_t extension_length;
  std::uint64_t payload_length;
  std::uint64_t sequence;
  Crc32c payload_checksum;

  [[nodiscard]] constexpr bool mandatory() const noexcept {
    return (flags & record_mandatory_flag) != 0;
  }

  constexpr bool operator==(const RecordEnvelope&) const noexcept = default;
};

struct RecordEnvelopeLimits {
  std::uint16_t maximum_extension_length;
  std::uint64_t maximum_payload_length;
};

enum class RecordDisposition : std::uint8_t {
  process,
  skip,
};

class VerifiedRecord {
 public:
  [[nodiscard]] constexpr const RecordEnvelope& envelope() const noexcept {
    return envelope_;
  }

 private:
  explicit constexpr VerifiedRecord(RecordEnvelope envelope) noexcept
      : envelope_(envelope) {}

  friend Result<VerifiedRecord> verify_record(
      std::span<const std::byte> envelope_bytes,
      std::span<const std::byte> extension,
      std::span<const std::byte> payload,
      const RecordEnvelopeLimits& limits);

  RecordEnvelope envelope_;
};

class RecordSequenceTracker {
 public:
  [[nodiscard]] Result<RecordDisposition> accept(
      const VerifiedRecord& record);
  [[nodiscard]] constexpr std::uint64_t expected_sequence() const noexcept {
    return expected_sequence_;
  }

 private:
  std::uint64_t expected_sequence_ = 0;
};

[[nodiscard]] Result<std::array<std::byte, record_envelope_size>>
encode_record_envelope(const RecordEnvelope& envelope);

[[nodiscard]] Result<RecordEnvelope> parse_record_envelope(
    std::span<const std::byte> bytes,
    const RecordEnvelopeLimits& limits);

[[nodiscard]] Result<VerifiedRecord> verify_record(
    std::span<const std::byte> envelope_bytes,
    std::span<const std::byte> extension,
    std::span<const std::byte> payload,
    const RecordEnvelopeLimits& limits);

}  // namespace shibori::engine::detail
