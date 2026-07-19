#include <shibori/engine/integrity.hpp>

#include <algorithm>
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
  using shibori::engine::Blake3Hasher;

  if (!expect(
          shibori::engine::blake3_version() == "1.8.5",
          "Unexpected pinned BLAKE3 version")) {
    return 1;
  }

  auto empty = Blake3Hasher::create();
  if (!expect(empty.has_value(), "Unable to create empty BLAKE3 hasher")) {
    return 1;
  }
  if (!expect(
          empty->finalize().to_hex() ==
              "af1349b9f5f9a1a6a0404dea36dcc949"
              "9bcb25c9adc112b7cc9a93cae41f3262",
          "Official empty BLAKE3 vector failed")) {
    return 1;
  }

  auto contiguous = Blake3Hasher::create();
  if (!expect(contiguous.has_value(), "Unable to create BLAKE3 hasher")) {
    return 1;
  }
  contiguous->update(bytes("abc"));
  const auto abc_digest = contiguous->finalize();
  if (!expect(
          abc_digest.to_hex() ==
              "6437b3ac38465133ffb63b75273a8db5"
              "48c558465d79db03fd359c6cd5bd9d85",
          "Official abc BLAKE3 vector failed")) {
    return 1;
  }
  if (!expect(
          contiguous->finalize() == abc_digest,
          "Repeated BLAKE3 finalize changed the digest")) {
    return 1;
  }

  auto fragmented = Blake3Hasher::create();
  if (!expect(fragmented.has_value(), "Unable to create fragmented hasher")) {
    return 1;
  }
  fragmented->update(bytes("a"));
  fragmented->update(bytes("b"));
  fragmented->update(bytes("c"));
  if (!expect(
          fragmented->finalize() == abc_digest,
          "Fragmented BLAKE3 differs from contiguous BLAKE3")) {
    return 1;
  }

  fragmented->reset();
  fragmented->update(bytes("abc"));
  if (!expect(
          fragmented->finalize() == abc_digest,
          "Reset BLAKE3 did not reproduce the digest")) {
    return 1;
  }

  std::vector<std::byte> large(1024 * 1024, std::byte{0xa5});
  auto large_contiguous = Blake3Hasher::create();
  if (!expect(
          large_contiguous.has_value(),
          "Unable to create large-input hasher")) {
    return 1;
  }
  large_contiguous->update(large);
  auto large_fragmented = Blake3Hasher::create();
  if (!expect(
          large_fragmented.has_value(),
          "Unable to create fragmented large-input hasher")) {
    return 1;
  }
  constexpr std::size_t fragment_size = 997;
  for (std::size_t offset = 0; offset < large.size();
       offset += fragment_size) {
    const auto length =
        std::min(fragment_size, large.size() - offset);
    large_fragmented->update(
        std::span<const std::byte>(large).subspan(offset, length));
  }

  return expect(
             large_fragmented->finalize() ==
                 large_contiguous->finalize(),
             "Large fragmented BLAKE3 differs from contiguous BLAKE3")
             ? 0
             : 1;
}
