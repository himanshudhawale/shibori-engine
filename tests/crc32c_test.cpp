#include <shibori/engine/integrity.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

std::span<const std::byte> bytes(std::string_view text) {
  return std::as_bytes(std::span(text.data(), text.size()));
}

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

}  // namespace

int main() {
  using shibori::engine::Crc32cHasher;
  using shibori::engine::Crc32cImplementation;
  using shibori::engine::Crc32cMode;

  Crc32cHasher empty;
  if (!expect(empty.finalize().value() == 0U, "Empty CRC32C vector failed")) {
    return 1;
  }

  Crc32cHasher contiguous;
  contiguous.update(bytes("123456789"));
  if (!expect(
          contiguous.finalize().value() == 0xe3069283U,
          "Official CRC32C check vector failed")) {
    return 1;
  }

  Crc32cHasher fragmented;
  fragmented.update(bytes("123"));
  fragmented.update(bytes("456"));
  fragmented.update(bytes("789"));
  if (!expect(
          fragmented.finalize() == contiguous.finalize(),
          "Fragmented CRC32C differs from contiguous CRC32C")) {
    return 1;
  }

  fragmented.reset();
  fragmented.update(bytes("123456789"));
  if (!expect(
          fragmented.finalize() == contiguous.finalize(),
          "Reset CRC32C did not reproduce the vector")) {
    return 1;
  }

  std::vector<std::byte> large(1024 * 1024, std::byte{0xa5});
  Crc32cHasher large_contiguous;
  large_contiguous.update(large);

  Crc32cHasher large_fragmented;
  constexpr std::size_t fragment_size = 997;
  for (std::size_t offset = 0; offset < large.size();
       offset += fragment_size) {
    const auto length =
        std::min(fragment_size, large.size() - offset);
    large_fragmented.update(
        std::span<const std::byte>(large).subspan(offset, length));
  }

  if (!expect(
          large_fragmented.finalize() == large_contiguous.finalize(),
          "Large fragmented CRC32C differs from contiguous CRC32C")) {
    return 1;
  }

  Crc32cHasher portable(Crc32cMode::portable);
  portable.update(large);
  if (!expect(
          portable.implementation() == Crc32cImplementation::portable,
          "Portable mode selected another implementation")) {
    return 1;
  }
  if (!expect(
          portable.finalize() == large_contiguous.finalize(),
          "Runtime and portable CRC32C implementations differ")) {
    return 1;
  }

  if (Crc32cHasher::hardware_available()) {
    return expect(
               large_contiguous.implementation() ==
                   Crc32cImplementation::hardware,
               "Automatic mode did not select available hardware CRC32C")
               ? 0
               : 1;
  }

  return expect(
             large_contiguous.implementation() ==
                 Crc32cImplementation::portable,
             "Automatic mode selected unavailable hardware CRC32C")
             ? 0
             : 1;
}
