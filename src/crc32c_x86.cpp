#include "crc32c_backend.hpp"

#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#include <nmmintrin.h>
#else
#include <cpuid.h>
#include <nmmintrin.h>
#endif

namespace shibori::engine::detail {

bool x86_crc32c_available() noexcept {
#if defined(_MSC_VER)
  int registers[4]{};
  __cpuid(registers, 1);
  return (registers[2] & (1 << 20)) != 0;
#else
  return __builtin_cpu_supports("sse4.2");
#endif
}

std::uint32_t x86_crc32c_update(
    std::uint32_t state,
    std::span<const std::byte> bytes) noexcept {
  auto offset = std::size_t{0};

#if defined(_M_X64) || defined(__x86_64__)
  auto wide_state = static_cast<std::uint64_t>(state);
  while (bytes.size() - offset >= sizeof(std::uint64_t)) {
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    wide_state = _mm_crc32_u64(wide_state, value);
    offset += sizeof(value);
  }
  state = static_cast<std::uint32_t>(wide_state);
#endif

  while (offset < bytes.size()) {
    state = _mm_crc32_u8(
        state,
        static_cast<unsigned char>(bytes[offset]));
    ++offset;
  }
  return state;
}

}  // namespace shibori::engine::detail
