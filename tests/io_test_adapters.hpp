#pragma once

#include <shibori/engine/io.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace shibori::engine::test {

class DeterministicSource final : public ByteSource {
 public:
  DeterministicSource(
      std::vector<std::byte> bytes,
      std::size_t maximum_read,
      bool seekable = true,
      bool zero_progress = false,
      std::optional<std::size_t> failure_call = std::nullopt)
      : bytes_(std::move(bytes)),
        maximum_read_(maximum_read),
        seekable_(seekable),
        zero_progress_(zero_progress),
        failure_call_(failure_call) {}

  Result<ReadOutcome> read(
      std::span<std::byte> destination,
      const CancellationToken&) override {
    ++read_calls;
    if (failure_call_ && read_calls == *failure_call_) {
      return fail(
          ErrorCode::io_read_failed,
          Operation::read,
          "injected source failure");
    }
    if (zero_progress_) {
      return ReadOutcome{0, false};
    }
    const auto count = std::min({
        destination.size(), maximum_read_, bytes_.size() - position_});
    if (count != 0) {
      std::memcpy(destination.data(), bytes_.data() + position_, count);
      position_ += count;
    }
    return ReadOutcome{count, position_ == bytes_.size()};
  }
  ByteSourceCapabilities capabilities() const noexcept override {
    return {true, true, seekable_};
  }
  std::optional<std::uint64_t> position() const noexcept override {
    return position_;
  }
  std::optional<std::uint64_t> size() const noexcept override {
    return bytes_.size();
  }
  Status seek(
      std::uint64_t position,
      const CancellationToken&) override {
    ++seek_calls;
    if (!seekable_) {
      return fail(
          ErrorCode::unsupported_feature,
          Operation::read,
          "injected source is not seekable");
    }
    if (position > bytes_.size()) {
      return fail(
          ErrorCode::io_read_failed,
          Operation::read,
          "injected source seek failed");
    }
    position_ = static_cast<std::size_t>(position);
    return {};
  }

  std::size_t read_calls = 0;
  std::size_t seek_calls = 0;

 private:
  std::vector<std::byte> bytes_;
  std::size_t maximum_read_;
  bool seekable_;
  bool zero_progress_;
  std::optional<std::size_t> failure_call_;
  std::size_t position_ = 0;
};

class DeterministicSink final : public ByteSink {
 public:
  DeterministicSink(
      std::size_t maximum_write,
      bool seekable = true,
      bool zero_progress = false,
      std::optional<std::size_t> failure_call = std::nullopt)
      : maximum_write_(maximum_write),
        seekable_(seekable),
        zero_progress_(zero_progress),
        failure_call_(failure_call) {}

  Result<std::size_t> write(
      std::span<const std::byte> source,
      const CancellationToken&) override {
    ++write_calls;
    if (failure_call_ && write_calls == *failure_call_) {
      return fail(
          ErrorCode::io_write_failed,
          Operation::write,
          "injected sink failure");
    }
    if (zero_progress_) {
      return std::size_t{0};
    }
    const auto count = std::min(source.size(), maximum_write_);
    const auto end = position_ + count;
    if (end > bytes.size()) {
      bytes.resize(end);
    }
    std::copy_n(source.begin(), count, bytes.begin() + position_);
    position_ = end;
    return count;
  }
  ByteSinkCapabilities capabilities() const noexcept override {
    return {true, seekable_, true};
  }
  std::optional<std::uint64_t> position() const noexcept override {
    return position_;
  }
  Status seek(
      std::uint64_t position,
      const CancellationToken&) override {
    ++seek_calls;
    if (!seekable_) {
      return fail(
          ErrorCode::unsupported_feature,
          Operation::write,
          "injected sink is not seekable");
    }
    if (position > bytes.size()) {
      return fail(
          ErrorCode::io_write_failed,
          Operation::write,
          "injected sink seek failed");
    }
    position_ = static_cast<std::size_t>(position);
    return {};
  }
  Status flush(const CancellationToken&) override { return {}; }

  std::vector<std::byte> bytes;
  std::size_t write_calls = 0;
  std::size_t seek_calls = 0;

 private:
  std::size_t maximum_write_;
  bool seekable_;
  bool zero_progress_;
  std::optional<std::size_t> failure_call_;
  std::size_t position_ = 0;
};

}  // namespace shibori::engine::test
