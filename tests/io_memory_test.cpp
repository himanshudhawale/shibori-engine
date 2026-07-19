#include <shibori/engine/io.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool short_operation_tests() {
  using namespace shibori::engine;
  const std::array input{
      std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
      std::byte{5}};
  auto source = MemoryByteSource::copy(input, 2);
  std::array<std::byte, 5> output{};
  const auto read = read_exact(**source, output);

  auto sink = MemoryByteSink::create(5, 2);
  const auto written = write_all(**sink, input);
  return expect(read && output == input, "Short reads did not complete") &&
         expect(
             written && (*sink)->bytes().size() == input.size() &&
                 (*sink)->bytes()[4] == std::byte{5},
             "Short writes did not complete");
}

bool capability_and_seek_tests() {
  using namespace shibori::engine;
  const std::array input{std::byte{4}, std::byte{5}};
  auto source = MemoryByteSource::copy(input);
  const auto source_capabilities = require_capabilities(
      **source,
      ByteSourceCapabilities{true, true, true});
  auto seek = (*source)->seek(1, {});
  std::array<std::byte, 1> value{};
  auto read = read_exact(**source, value);

  auto sink = MemoryByteSink::create(4);
  const auto sink_capabilities = require_capabilities(
      **sink,
      ByteSinkCapabilities{true, true, true});
  (void)write_all(**sink, input);
  auto sink_seek = (*sink)->seek(0, {});
  const std::array replacement{std::byte{9}};
  auto overwrite = write_all(**sink, replacement);

  return expect(source_capabilities && sink_capabilities,
                "Memory capabilities were not reported") &&
         expect(seek && read && value[0] == std::byte{5},
                "Memory source seek failed") &&
         expect(sink_seek && overwrite && (*sink)->bytes()[0] == std::byte{9},
                "Memory sink seek failed");
}

bool lifetime_and_error_tests() {
  using namespace shibori::engine;
  auto storage =
      std::make_shared<const std::vector<std::byte>>(3, std::byte{7});
  auto source = MemoryByteSource::share(storage, 1);
  storage.reset();
  std::array<std::byte, 3> output{};
  const auto read = read_exact(**source, output);

  std::array<std::byte, 4> too_long{};
  const auto truncated = read_exact(**source, too_long);

  auto sink = MemoryByteSink::create(2);
  const auto limited = write_all(**sink, too_long);

  return expect(read && output[2] == std::byte{7},
                "Shared memory source lost its storage") &&
         expect(
             !truncated &&
                 truncated.error().code() == ErrorCode::unexpected_end,
             "Truncated source did not return a typed error") &&
         expect(
             !limited &&
                 limited.error().code() == ErrorCode::limit_exceeded,
             "Bounded sink did not return a resource error");
}

bool cancellation_tests() {
  using namespace shibori::engine;
  CancellationSource cancellation;
  auto token = cancellation.token();
  cancellation.request_cancel();
  const std::array input{std::byte{1}};
  auto source = MemoryByteSource::copy(input);
  std::array<std::byte, 1> output{};
  const auto read = read_exact(**source, output, token);
  auto sink = MemoryByteSink::create(4);
  const auto write = write_all(**sink, input, token);
  return expect(
             !read && read.error().code() == ErrorCode::operation_cancelled,
             "Cancelled read was not reported") &&
         expect(
             !write && write.error().code() == ErrorCode::operation_cancelled &&
                 (*sink)->bytes().empty(),
             "Cancelled write mutated the sink");
}

}  // namespace

int main() {
  return short_operation_tests() && capability_and_seek_tests() &&
                 lifetime_and_error_tests() && cancellation_tests()
             ? 0
             : 1;
}
