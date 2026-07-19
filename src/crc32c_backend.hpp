#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace shibori::engine::detail {

std::uint32_t portable_crc32c_update(
    std::uint32_t state,
    std::span<const std::byte> bytes) noexcept;

#if defined(SHIBORI_HAS_X86_CRC32C)
bool x86_crc32c_available() noexcept;
std::uint32_t x86_crc32c_update(
    std::uint32_t state,
    std::span<const std::byte> bytes) noexcept;
#endif

}  // namespace shibori::engine::detail
