#include "preamble.hpp"

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
  using namespace shibori::engine::detail;

  constexpr std::array<std::byte, file_preamble_size> expected{
      std::byte{0x53},
      std::byte{0x48},
      std::byte{0x49},
      std::byte{0x42},
      std::byte{0x4f},
      std::byte{0x52},
      std::byte{0x49},
      std::byte{0x00},
      std::byte{0x01},
      std::byte{0x00},
      std::byte{0x00},
      std::byte{0x00},
      std::byte{0xeb},
      std::byte{0xbd},
      std::byte{0xc3},
      std::byte{0x9e},
  };

  const auto first = encode_current_file_preamble();
  const auto second = encode_current_file_preamble();

  if (!expect(first == expected, "Preamble bytes do not match format 1.0")) {
    return 1;
  }
  if (!expect(first == second, "Preamble encoding is not deterministic")) {
    return 1;
  }

  const auto alternate = encode_file_preamble(0x1234, 0xabcd);
  return expect(
             alternate[8] == std::byte{0x34} &&
                 alternate[9] == std::byte{0x12} &&
                 alternate[10] == std::byte{0xcd} &&
                 alternate[11] == std::byte{0xab} &&
                 alternate != first,
             "Preamble version fields are not explicit little-endian values")
             ? 0
             : 1;
}
