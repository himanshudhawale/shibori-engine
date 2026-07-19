#include "io_test_adapters.hpp"

#include <shibori/engine/io.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

class FileCleanup {
 public:
  explicit FileCleanup(std::string path) : path_(std::move(path)) {
    std::remove(path_.c_str());
  }
  ~FileCleanup() { std::remove(path_.c_str()); }

 private:
  std::string path_;
};

bool file_round_trip() {
  using namespace shibori::engine;
  const std::string path = "shibori-engine-io-file-test.bin";
  FileCleanup cleanup(path);
  const std::array input{
      std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
      std::byte{5}};
  {
    auto sink = FileByteSink::open(path, input.size(), true, 2);
    if (!sink || !write_all(**sink, input)) {
      return false;
    }
    const std::array replacement{std::byte{9}};
    if (!write_all_at(**sink, 2, replacement) ||
        !(*sink)->flush({})) {
      return false;
    }
  }
  auto source = FileByteSource::open(path, 2);
  if (!source || (*source)->size() != input.size()) {
    return false;
  }
  std::array<std::byte, 5> output{};
  const auto read = read_exact(**source, output);
  std::array<std::byte, 2> middle{};
  const auto positioned = read_exact_at(**source, 1, middle);
  return expect(
             read && output[2] == std::byte{9},
             "File short-operation round trip failed") &&
         expect(
             positioned && middle[0] == std::byte{2} &&
                 middle[1] == std::byte{9},
             "File positioned read failed");
}

bool deterministic_adapter_tests() {
  using namespace shibori::engine;
  using namespace shibori::engine::test;
  const std::vector input{
      std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  DeterministicSource short_source(input, 1);
  std::array<std::byte, 4> output{};
  const auto short_read = read_exact(short_source, output);
  DeterministicSink short_sink(1);
  const auto short_write = write_all(short_sink, input);

  DeterministicSource zero_source(input, 1, true, true);
  std::array<std::byte, 1> one{};
  const auto zero_read = read_exact(zero_source, one);
  DeterministicSink zero_sink(1, true, true);
  const auto zero_write = write_all(zero_sink, input);

  return expect(
             short_read && short_source.read_calls == 4,
             "Deterministic short reads did not complete") &&
         expect(
             short_write && short_sink.write_calls == 4,
             "Deterministic short writes did not complete") &&
         expect(
             !zero_read &&
                 zero_read.error().code() == ErrorCode::io_no_progress,
             "Zero-progress read was accepted") &&
         expect(
             !zero_write &&
                 zero_write.error().code() == ErrorCode::io_no_progress,
             "Zero-progress write was accepted");
}

bool failure_and_seek_tests() {
  using namespace shibori::engine;
  using namespace shibori::engine::test;
  const std::vector input{
      std::byte{1}, std::byte{2}, std::byte{3}};
  DeterministicSource failed_source(input, 1, true, false, 2);
  std::array<std::byte, 3> output{};
  const auto failed_read = read_exact(failed_source, output);
  DeterministicSink failed_sink(1, true, false, 2);
  const auto failed_write = write_all(failed_sink, input);

  DeterministicSource unseekable_source(input, 1, false);
  const auto positioned_read =
      read_exact_at(unseekable_source, 0, output);
  DeterministicSink unseekable_sink(1, false);
  const auto positioned_write =
      write_all_at(unseekable_sink, 0, input);

  return expect(
             !failed_read &&
                 failed_read.error().code() == ErrorCode::io_read_failed,
             "Injected read failure was not propagated") &&
         expect(
             !failed_write &&
                 failed_write.error().code() == ErrorCode::io_write_failed,
             "Injected write failure was not propagated") &&
         expect(
             !positioned_read && unseekable_source.seek_calls == 0 &&
                 unseekable_source.read_calls == 0,
             "Seek-required read mutated an unsupported source") &&
         expect(
             !positioned_write && unseekable_sink.seek_calls == 0 &&
                 unseekable_sink.write_calls == 0 &&
                 unseekable_sink.bytes.empty(),
             "Seek-required write mutated an unsupported sink");
}

bool file_error_tests() {
  using namespace shibori::engine;
  const auto missing =
      FileByteSource::open("shibori-engine-definitely-missing.bin");
  const std::string path = "shibori-engine-io-limit-test.bin";
  FileCleanup cleanup(path);
  auto sink = FileByteSink::open(path, 1);
  const std::array input{std::byte{1}, std::byte{2}};
  const auto limited = write_all(**sink, input);
  return expect(
             !missing &&
                 missing.error().code() == ErrorCode::io_read_failed,
             "Missing file did not produce an I/O error") &&
         expect(
             !limited &&
                 limited.error().code() == ErrorCode::limit_exceeded,
             "File sink bound was ignored");
}

}  // namespace

int main() {
  return file_round_trip() && deterministic_adapter_tests() &&
                 failure_and_seek_tests() && file_error_tests()
             ? 0
             : 1;
}
