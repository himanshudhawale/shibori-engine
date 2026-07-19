#include <shibori/engine/io.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <system_error>
#include <utility>

namespace shibori::engine {
namespace {

#if defined(_WIN32)
using NativeOffset = __int64;

int file_seek(std::FILE* file, NativeOffset offset, int origin) {
  return _fseeki64(file, offset, origin);
}

NativeOffset file_tell(std::FILE* file) {
  return _ftelli64(file);
}

std::FILE* open_file(const char* path, const char* mode) {
  std::FILE* file = nullptr;
  return fopen_s(&file, path, mode) == 0 ? file : nullptr;
}
#else
using NativeOffset = off_t;

int file_seek(std::FILE* file, NativeOffset offset, int origin) {
  return fseeko(file, offset, origin);
}

NativeOffset file_tell(std::FILE* file) {
  return ftello(file);
}

std::FILE* open_file(const char* path, const char* mode) {
  return std::fopen(path, mode);
}
#endif

Error file_error(ErrorCode code, Operation operation, std::string action) {
  const auto message = std::error_code(errno, std::generic_category()).message();
  return Error(
      code,
      operation,
      std::move(action) + ": " + message);
}

Status cancelled(Operation operation) {
  return fail(
      ErrorCode::operation_cancelled,
      operation,
      "file I/O operation was cancelled");
}

Result<NativeOffset> checked_offset(
    std::uint64_t position,
    Operation operation) {
  if (position >
      static_cast<std::uint64_t>(std::numeric_limits<NativeOffset>::max())) {
    return fail(
        ErrorCode::range_exceeded,
        operation,
        "file position exceeds the platform offset range");
  }
  return static_cast<NativeOffset>(position);
}

}  // namespace

class FileByteSource::Impl {
 public:
  Impl(
      std::FILE* file_value,
      std::uint64_t size_value,
      std::size_t maximum_read_size_value)
      : file(file_value),
        size(size_value),
        maximum_read_size(maximum_read_size_value) {}
  ~Impl() {
    if (file != nullptr) {
      std::fclose(file);
    }
  }
  std::FILE* file;
  std::uint64_t position = 0;
  std::uint64_t size;
  std::size_t maximum_read_size;
};

FileByteSource::FileByteSource(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
FileByteSource::~FileByteSource() = default;

Result<std::unique_ptr<FileByteSource>> FileByteSource::open(
    std::string path,
    std::size_t maximum_read_size) {
  if (path.empty() || maximum_read_size == 0) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::configure,
        "file source path and read size must be valid");
  }
  auto* file = open_file(path.c_str(), "rb");
  if (file == nullptr) {
    return std::unexpected(
        file_error(ErrorCode::io_read_failed, Operation::read, "open failed"));
  }
  if (file_seek(file, 0, SEEK_END) != 0) {
    const auto error =
        file_error(ErrorCode::io_read_failed, Operation::read, "size failed");
    std::fclose(file);
    return std::unexpected(error);
  }
  const auto end = file_tell(file);
  if (end < 0 || file_seek(file, 0, SEEK_SET) != 0) {
    const auto error =
        file_error(ErrorCode::io_read_failed, Operation::read, "size failed");
    std::fclose(file);
    return std::unexpected(error);
  }
  try {
    return std::unique_ptr<FileByteSource>(new FileByteSource(
        std::make_unique<Impl>(
            file,
            static_cast<std::uint64_t>(end),
            maximum_read_size)));
  } catch (const std::bad_alloc&) {
    std::fclose(file);
    return fail(
        ErrorCode::allocation_failed,
        Operation::configure,
        "unable to allocate file byte source");
  }
}

Result<ReadOutcome> FileByteSource::read(
    std::span<std::byte> destination,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return std::unexpected(std::move(cancelled(Operation::read).error()));
  }
  const auto available = impl_->size - impl_->position;
  const auto count = std::min({
      destination.size(),
      impl_->maximum_read_size,
      static_cast<std::size_t>(std::min<std::uint64_t>(
          available,
          std::numeric_limits<std::size_t>::max()))});
  const auto read = count == 0
                        ? std::size_t{0}
                        : std::fread(destination.data(), 1, count, impl_->file);
  impl_->position += read;
  if (read < count && std::ferror(impl_->file) != 0) {
    return std::unexpected(file_error(
        ErrorCode::io_read_failed,
        Operation::read,
        "file read failed"));
  }
  return ReadOutcome{
      .bytes_read = read,
      .eof = impl_->position == impl_->size ||
             std::feof(impl_->file) != 0};
}
ByteSourceCapabilities FileByteSource::capabilities() const noexcept {
  return {true, true, true};
}
std::optional<std::uint64_t> FileByteSource::position() const noexcept {
  return impl_->position;
}
std::optional<std::uint64_t> FileByteSource::size() const noexcept {
  return impl_->size;
}
Status FileByteSource::seek(
    std::uint64_t position,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::read);
  }
  if (position > impl_->size) {
    return fail(
        ErrorCode::io_read_failed,
        Operation::read,
        "file source seek is beyond the end");
  }
  auto offset = checked_offset(position, Operation::read);
  if (!offset) {
    return std::unexpected(std::move(offset.error()));
  }
  if (file_seek(impl_->file, *offset, SEEK_SET) != 0) {
    return std::unexpected(file_error(
        ErrorCode::io_read_failed,
        Operation::read,
        "file seek failed"));
  }
  impl_->position = position;
  std::clearerr(impl_->file);
  return {};
}

class FileByteSink::Impl {
 public:
  Impl(
      std::FILE* file_value,
      std::uint64_t maximum_size_value,
      std::uint64_t extent_value,
      std::size_t maximum_write_size_value)
      : file(file_value),
        maximum_size(maximum_size_value),
        extent(extent_value),
        maximum_write_size(maximum_write_size_value) {}
  ~Impl() {
    if (file != nullptr) {
      std::fclose(file);
    }
  }
  std::FILE* file;
  std::uint64_t position = 0;
  std::uint64_t maximum_size;
  std::uint64_t extent;
  std::size_t maximum_write_size;
};

FileByteSink::FileByteSink(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
FileByteSink::~FileByteSink() = default;

Result<std::unique_ptr<FileByteSink>> FileByteSink::open(
    std::string path,
    std::uint64_t maximum_size,
    bool truncate,
    std::size_t maximum_write_size) {
  if (path.empty() || maximum_write_size == 0) {
    return fail(
        ErrorCode::invalid_configuration,
        Operation::configure,
        "file sink path and write size must be valid");
  }
  auto* file = open_file(path.c_str(), truncate ? "w+b" : "r+b");
  if (file == nullptr) {
    return std::unexpected(
        file_error(ErrorCode::io_write_failed, Operation::write, "open failed"));
  }
  std::uint64_t extent = 0;
  if (!truncate) {
    if (file_seek(file, 0, SEEK_END) != 0) {
      const auto error =
          file_error(ErrorCode::io_write_failed, Operation::write, "size failed");
      std::fclose(file);
      return std::unexpected(error);
    }
    const auto end = file_tell(file);
    if (end < 0 || file_seek(file, 0, SEEK_SET) != 0) {
      const auto error =
          file_error(ErrorCode::io_write_failed, Operation::write, "size failed");
      std::fclose(file);
      return std::unexpected(error);
    }
    extent = static_cast<std::uint64_t>(end);
    if (extent > maximum_size) {
      std::fclose(file);
      return fail(
          ErrorCode::limit_exceeded,
          Operation::configure,
          "existing file exceeds the sink bound");
    }
  }
  try {
    return std::unique_ptr<FileByteSink>(new FileByteSink(
        std::make_unique<Impl>(
            file,
            maximum_size,
            extent,
            maximum_write_size)));
  } catch (const std::bad_alloc&) {
    std::fclose(file);
    return fail(
        ErrorCode::allocation_failed,
        Operation::configure,
        "unable to allocate file byte sink");
  }
}

Result<std::size_t> FileByteSink::write(
    std::span<const std::byte> source,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return std::unexpected(std::move(cancelled(Operation::write).error()));
  }
  const auto available = impl_->maximum_size - impl_->position;
  if (!source.empty() && available == 0) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::write,
        "file sink maximum size was reached");
  }
  const auto count = std::min({
      source.size(),
      impl_->maximum_write_size,
      static_cast<std::size_t>(std::min<std::uint64_t>(
          available,
          std::numeric_limits<std::size_t>::max()))});
  const auto written =
      count == 0 ? std::size_t{0}
                 : std::fwrite(source.data(), 1, count, impl_->file);
  if (written < count && std::ferror(impl_->file) != 0) {
    return std::unexpected(file_error(
        ErrorCode::io_write_failed,
        Operation::write,
        "file write failed"));
  }
  impl_->position += written;
  impl_->extent = std::max(impl_->extent, impl_->position);
  return written;
}
ByteSinkCapabilities FileByteSink::capabilities() const noexcept {
  return {true, true, true};
}
std::optional<std::uint64_t> FileByteSink::position() const noexcept {
  return impl_->position;
}
Status FileByteSink::seek(
    std::uint64_t position,
    const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  if (position > impl_->extent) {
    return fail(
        ErrorCode::io_write_failed,
        Operation::write,
        "file sink cannot seek beyond written bytes");
  }
  auto offset = checked_offset(position, Operation::write);
  if (!offset) {
    return std::unexpected(std::move(offset.error()));
  }
  if (file_seek(impl_->file, *offset, SEEK_SET) != 0) {
    return std::unexpected(file_error(
        ErrorCode::io_write_failed,
        Operation::write,
        "file seek failed"));
  }
  impl_->position = position;
  std::clearerr(impl_->file);
  return {};
}
Status FileByteSink::flush(const CancellationToken& cancellation) {
  if (cancellation.is_cancelled()) {
    return cancelled(Operation::write);
  }
  if (std::fflush(impl_->file) != 0) {
    return std::unexpected(file_error(
        ErrorCode::io_write_failed,
        Operation::write,
        "file flush failed"));
  }
  return {};
}

}  // namespace shibori::engine
