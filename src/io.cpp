#include <shibori/engine/io.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace shibori::engine {
namespace {

Status cancelled(Operation operation) {
  return fail(
      ErrorCode::operation_cancelled,
      operation,
      "byte I/O operation was cancelled");
}

Status unsupported(std::string_view feature) {
  return fail(
      ErrorCode::unsupported_feature,
      Operation::configure,
      std::string(feature) + " capability is required");
}

}  // namespace

class CancellationToken::State {
 public:
  std::atomic_bool cancelled{false};
};

CancellationToken::CancellationToken()
    : state_(std::make_shared<State>()) {}
CancellationToken::CancellationToken(std::shared_ptr<State> state) noexcept
    : state_(std::move(state)) {}
CancellationToken::~CancellationToken() = default;
CancellationToken::CancellationToken(const CancellationToken&) = default;
CancellationToken& CancellationToken::operator=(const CancellationToken&) =
    default;
CancellationToken::CancellationToken(CancellationToken&&) noexcept = default;
CancellationToken& CancellationToken::operator=(
    CancellationToken&&) noexcept = default;
bool CancellationToken::is_cancelled() const noexcept {
  return state_ && state_->cancelled.load(std::memory_order_acquire);
}

CancellationSource::CancellationSource()
    : state_(std::make_shared<CancellationToken::State>()) {}
CancellationSource::~CancellationSource() = default;
CancellationSource::CancellationSource(CancellationSource&&) noexcept = default;
CancellationSource& CancellationSource::operator=(
    CancellationSource&&) noexcept = default;
CancellationToken CancellationSource::token() const noexcept {
  return CancellationToken(state_);
}
void CancellationSource::request_cancel() noexcept {
  state_->cancelled.store(true, std::memory_order_release);
}

ByteSource::~ByteSource() = default;
std::optional<std::uint64_t> ByteSource::position() const noexcept {
  return std::nullopt;
}
std::optional<std::uint64_t> ByteSource::size() const noexcept {
  return std::nullopt;
}
Status ByteSource::seek(
    std::uint64_t,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::read);
  }
  return unsupported("source seek");
}

ByteSink::~ByteSink() = default;
std::optional<std::uint64_t> ByteSink::position() const noexcept {
  return std::nullopt;
}
Status ByteSink::seek(
    std::uint64_t,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  return unsupported("sink seek");
}
Status ByteSink::flush(const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  return unsupported("sink flush");
}

Status require_capabilities(
    const ByteSource& source,
    ByteSourceCapabilities required) {
  const auto actual = source.capabilities();
  if (required.position && !actual.position) {
    return unsupported("source position");
  }
  if (required.size && !actual.size) {
    return unsupported("source size");
  }
  if (required.seek && !actual.seek) {
    return unsupported("source seek");
  }
  return {};
}

Status require_capabilities(
    const ByteSink& sink,
    ByteSinkCapabilities required) {
  const auto actual = sink.capabilities();
  if (required.position && !actual.position) {
    return unsupported("sink position");
  }
  if (required.seek && !actual.seek) {
    return unsupported("sink seek");
  }
  if (required.flush && !actual.flush) {
    return unsupported("sink flush");
  }
  return {};
}

Status read_exact(
    ByteSource& source,
    std::span<std::byte> destination,
    const CancellationToken& cancellation) {
  std::size_t completed = 0;
  while (completed < destination.size()) {
    if (cancellation.is_cancelled()) {
      return cancelled(Operation::read);
    }
    auto outcome = source.read(destination.subspan(completed), cancellation);
    if (!outcome) {
      return std::unexpected(std::move(outcome.error()));
    }
    const auto remaining = destination.size() - completed;
    if (outcome->bytes_read > remaining) {
      return fail(
          ErrorCode::plugin_contract_violation,
          Operation::read,
          "byte source reported more bytes than requested");
    }
    if (outcome->bytes_read == 0) {
      return fail(
          outcome->eof ? ErrorCode::unexpected_end
                       : ErrorCode::io_no_progress,
          Operation::read,
          outcome->eof ? "byte source ended before the requested bytes"
                       : "byte source made no progress");
    }
    completed += outcome->bytes_read;
    if (outcome->eof && completed != destination.size()) {
      return fail(
          ErrorCode::unexpected_end,
          Operation::read,
          "byte source ended before the requested bytes");
    }
  }
  return {};
}

Status write_all(
    ByteSink& sink,
    std::span<const std::byte> source,
    const CancellationToken& cancellation) {
  std::size_t completed = 0;
  while (completed < source.size()) {
    if (cancellation.is_cancelled()) {
      return cancelled(Operation::write);
    }
    auto written = sink.write(source.subspan(completed), cancellation);
    if (!written) {
      return std::unexpected(std::move(written.error()));
    }
    const auto remaining = source.size() - completed;
    if (*written > remaining) {
      return fail(
          ErrorCode::plugin_contract_violation,
          Operation::write,
          "byte sink reported more bytes than supplied");
    }
    if (*written == 0) {
      return fail(
          ErrorCode::io_no_progress,
          Operation::write,
          "byte sink made no progress");
    }
    completed += *written;
  }
  return {};
}

Status read_exact_at(
    ByteSource& source,
    std::uint64_t position,
    std::span<std::byte> destination,
    const CancellationToken& cancellation) {
  if (auto status = require_capabilities(
          source, ByteSourceCapabilities{false, false, true});
      !status) {
    return status;
  }
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::read);
  }
  if (auto status = source.seek(position, cancellation); !status) {
    return status;
  }
  return read_exact(source, destination, cancellation);
}

Status write_all_at(
    ByteSink& sink,
    std::uint64_t position,
    std::span<const std::byte> source,
    const CancellationToken& cancellation) {
  if (auto status = require_capabilities(
          sink, ByteSinkCapabilities{false, true, false});
      !status) {
    return status;
  }
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  if (auto status = sink.seek(position, cancellation); !status) {
    return status;
  }
  return write_all(sink, source, cancellation);
}

class MemoryByteSource::Impl {
 public:
  Impl(
      std::shared_ptr<const std::vector<std::byte>> bytes_value,
      std::size_t maximum_read_size_value)
      : bytes(std::move(bytes_value)),
        maximum_read_size(maximum_read_size_value) {}
  std::shared_ptr<const std::vector<std::byte>> bytes;
  std::size_t maximum_read_size;
  std::uint64_t position = 0;
};

MemoryByteSource::MemoryByteSource(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
MemoryByteSource::~MemoryByteSource() = default;

Result<std::unique_ptr<MemoryByteSource>> MemoryByteSource::copy(
    std::span<const std::byte> bytes,
    std::size_t maximum_read_size) {
  if (maximum_read_size == 0) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::configure,
        "memory source maximum read size must be nonzero");
  }
  try {
    auto storage = std::make_shared<const std::vector<std::byte>>(
        bytes.begin(), bytes.end());
    return std::unique_ptr<MemoryByteSource>(new MemoryByteSource(
        std::make_unique<Impl>(std::move(storage), maximum_read_size)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::configure,
        "unable to allocate memory byte source");
  }
}

Result<std::unique_ptr<MemoryByteSource>> MemoryByteSource::share(
    std::shared_ptr<const std::vector<std::byte>> bytes,
    std::size_t maximum_read_size) {
  if (!bytes || maximum_read_size == 0) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::configure,
        "shared memory source and read size must be valid");
  }
  try {
    return std::unique_ptr<MemoryByteSource>(new MemoryByteSource(
        std::make_unique<Impl>(std::move(bytes), maximum_read_size)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::configure,
        "unable to retain shared memory source");
  }
}

Result<ReadOutcome> MemoryByteSource::read(
    std::span<std::byte> destination,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return std::unexpected(
        std::move(cancelled(Operation::read).error()));
  }
  const auto available =
      static_cast<std::uint64_t>(impl_->bytes->size()) - impl_->position;
  const auto count = std::min({
      destination.size(),
      impl_->maximum_read_size,
      static_cast<std::size_t>(available)});
  if (count != 0) {
    std::memcpy(
        destination.data(),
        impl_->bytes->data() + static_cast<std::size_t>(impl_->position),
        count);
    impl_->position += count;
  }
  return ReadOutcome{
      .bytes_read = count,
      .eof = impl_->position == impl_->bytes->size()};
}
ByteSourceCapabilities MemoryByteSource::capabilities() const noexcept {
  return {true, true, true};
}
std::optional<std::uint64_t> MemoryByteSource::position() const noexcept {
  return impl_->position;
}
std::optional<std::uint64_t> MemoryByteSource::size() const noexcept {
  return static_cast<std::uint64_t>(impl_->bytes->size());
}
Status MemoryByteSource::seek(
    std::uint64_t position,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::read);
  }
  if (position > impl_->bytes->size()) {
    return fail(
        ErrorCode::io_read_failed,
        Operation::read,
        "memory source seek is beyond the end");
  }
  impl_->position = position;
  return {};
}

class MemoryByteSink::Impl {
 public:
  Impl(std::uint64_t maximum_size_value, std::size_t maximum_write_size_value)
      : maximum_size(maximum_size_value),
        maximum_write_size(maximum_write_size_value) {}
  std::vector<std::byte> bytes;
  std::uint64_t position = 0;
  std::uint64_t maximum_size;
  std::size_t maximum_write_size;
};

MemoryByteSink::MemoryByteSink(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
MemoryByteSink::~MemoryByteSink() = default;
Result<std::unique_ptr<MemoryByteSink>> MemoryByteSink::create(
    std::uint64_t maximum_size,
    std::size_t maximum_write_size) {
  if (maximum_write_size == 0 ||
      maximum_size >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::configure,
        "memory sink size and write size must be valid");
  }
  try {
    return std::unique_ptr<MemoryByteSink>(new MemoryByteSink(
        std::make_unique<Impl>(maximum_size, maximum_write_size)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::configure,
        "unable to allocate memory byte sink");
  }
}
Result<std::size_t> MemoryByteSink::write(
    std::span<const std::byte> source,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return std::unexpected(
        std::move(cancelled(Operation::write).error()));
  }
  if (impl_->position > impl_->maximum_size) {
    return fail(
        ErrorCode::invariant_violation,
        Operation::write,
        "memory sink position exceeds its bound");
  }
  const auto available = impl_->maximum_size - impl_->position;
  if (!source.empty() && available == 0) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::write,
        "memory sink maximum size was reached");
  }
  const auto count = std::min({
      source.size(),
      impl_->maximum_write_size,
      static_cast<std::size_t>(available)});
  try {
    const auto end = impl_->position + count;
    if (end > impl_->bytes.size()) {
      impl_->bytes.resize(static_cast<std::size_t>(end));
    }
    if (count != 0) {
      std::memcpy(
          impl_->bytes.data() + static_cast<std::size_t>(impl_->position),
          source.data(),
          count);
      impl_->position = end;
    }
    return count;
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::write,
        "unable to grow memory byte sink");
  }
}
ByteSinkCapabilities MemoryByteSink::capabilities() const noexcept {
  return {true, true, true};
}
std::optional<std::uint64_t> MemoryByteSink::position() const noexcept {
  return impl_->position;
}
Status MemoryByteSink::seek(
    std::uint64_t position,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  if (position > impl_->bytes.size()) {
    return fail(
        ErrorCode::io_write_failed,
        Operation::write,
        "memory sink cannot seek beyond written bytes");
  }
  impl_->position = position;
  return {};
}
Status MemoryByteSink::flush(const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  return {};
}
std::span<const std::byte> MemoryByteSink::bytes() const noexcept {
  return impl_->bytes;
}

}  // namespace shibori::engine
