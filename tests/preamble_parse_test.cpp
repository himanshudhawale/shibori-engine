#include "preamble.hpp"

#include <array>
#include <cstddef>
#include <iostream>
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

  const auto encoded = encode_current_file_preamble();
  const auto parsed = parse_file_preamble(encoded);
  if (!expect(
          parsed && parsed->major == current_format_major &&
              parsed->minor == current_format_minor &&
              parsed->checksum.value() == 0x9ec3bdebU,
          "Valid format 1.0 preamble did not parse")) {
    return 1;
  }

  for (std::size_t length = 0; length < file_preamble_size; ++length) {
    const auto truncated =
        parse_file_preamble(std::span<const std::byte>(encoded).first(length));
    if (!expect(
            !truncated &&
                truncated.error().code() == ErrorCode::unexpected_end,
            "Truncated preamble did not return unexpected_end")) {
      return 1;
    }
  }

  auto bad_magic = encoded;
  bad_magic[0] = std::byte{'X'};
  const auto invalid_magic = parse_file_preamble(bad_magic);
  if (!expect(
          !invalid_magic &&
              invalid_magic.error().code() == ErrorCode::invalid_magic,
          "Bad preamble magic did not return invalid_magic")) {
    return 1;
  }

  auto bad_checksum = encoded;
  bad_checksum[15] ^= std::byte{0x01};
  const auto checksum_mismatch = parse_file_preamble(bad_checksum);
  if (!expect(
          !checksum_mismatch &&
              checksum_mismatch.error().code() ==
                  ErrorCode::checksum_mismatch,
          "Bad preamble checksum did not return checksum_mismatch")) {
    return 1;
  }

  const auto future_major = encode_file_preamble(2, 0);
  const auto unsupported = parse_file_preamble(future_major);
  if (!expect(
          !unsupported &&
              unsupported.error().code() ==
                  ErrorCode::unsupported_format_version,
          "Unsupported major version was accepted")) {
    return 1;
  }

  const auto future_minor = encode_file_preamble(current_format_major, 7);
  const auto accepted_minor = parse_file_preamble(future_minor);
  return expect(
             accepted_minor && accepted_minor->minor == 7,
             "Preamble parser rejected a higher minor before feature checks")
             ? 0
             : 1;
}
