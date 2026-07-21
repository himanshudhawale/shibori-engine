#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <shibori/engine/integrity.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine::detail {

inline constexpr std::size_t file_preamble_size = 16;
inline constexpr std::uint16_t current_format_major = 1;
inline constexpr std::uint16_t current_format_minor = 0;

struct FilePreamble {
  std::uint16_t major;
  std::uint16_t minor;
  Crc32c checksum;

  constexpr bool operator==(const FilePreamble&) const noexcept = default;
};

[[nodiscard]] std::array<std::byte, file_preamble_size> encode_file_preamble(
    std::uint16_t major,
    std::uint16_t minor) noexcept;

[[nodiscard]] inline std::array<std::byte, file_preamble_size>
encode_current_file_preamble() noexcept {
  return encode_file_preamble(current_format_major, current_format_minor);
}

[[nodiscard]] Result<FilePreamble> parse_file_preamble(
    std::span<const std::byte> bytes);

}  // namespace shibori::engine::detail
