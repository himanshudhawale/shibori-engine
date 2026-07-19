#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <shibori/engine/export.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine {

class CancellationToken {
 public:
  SHIBORI_ENGINE_API CancellationToken();
  SHIBORI_ENGINE_API ~CancellationToken();
  SHIBORI_ENGINE_API CancellationToken(const CancellationToken& other);
  SHIBORI_ENGINE_API CancellationToken& operator=(
      const CancellationToken& other);
  SHIBORI_ENGINE_API CancellationToken(CancellationToken&& other) noexcept;
  SHIBORI_ENGINE_API CancellationToken& operator=(
      CancellationToken&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API bool is_cancelled() const noexcept;

 private:
  friend class CancellationSource;
  class State;
  explicit CancellationToken(std::shared_ptr<State> state) noexcept;
  std::shared_ptr<State> state_;
};

class CancellationSource {
 public:
  SHIBORI_ENGINE_API CancellationSource();
  SHIBORI_ENGINE_API ~CancellationSource();
  CancellationSource(const CancellationSource&) = delete;
  CancellationSource& operator=(const CancellationSource&) = delete;
  SHIBORI_ENGINE_API CancellationSource(CancellationSource&& other) noexcept;
  SHIBORI_ENGINE_API CancellationSource& operator=(
      CancellationSource&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API CancellationToken token() const noexcept;
  SHIBORI_ENGINE_API void request_cancel() noexcept;

 private:
  std::shared_ptr<CancellationToken::State> state_;
};

struct ByteSourceCapabilities {
  bool position;
  bool size;
  bool seek;
};

struct ByteSinkCapabilities {
  bool position;
  bool seek;
  bool flush;
};

struct ReadOutcome {
  std::size_t bytes_read;
  bool eof;
};

class ByteSource {
 public:
  SHIBORI_ENGINE_API virtual ~ByteSource();

  [[nodiscard]] virtual Result<ReadOutcome> read(
      std::span<std::byte> destination,
      const CancellationToken& cancellation) = 0;
  [[nodiscard]] virtual ByteSourceCapabilities capabilities() const noexcept = 0;
  [[nodiscard]] SHIBORI_ENGINE_API virtual std::optional<std::uint64_t>
  position() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API virtual std::optional<std::uint64_t> size()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API virtual Status seek(
      std::uint64_t position,
      const CancellationToken& cancellation);
};

class ByteSink {
 public:
  SHIBORI_ENGINE_API virtual ~ByteSink();

  [[nodiscard]] virtual Result<std::size_t> write(
      std::span<const std::byte> source,
      const CancellationToken& cancellation) = 0;
  [[nodiscard]] virtual ByteSinkCapabilities capabilities() const noexcept = 0;
  [[nodiscard]] SHIBORI_ENGINE_API virtual std::optional<std::uint64_t>
  position() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API virtual Status seek(
      std::uint64_t position,
      const CancellationToken& cancellation);
  [[nodiscard]] SHIBORI_ENGINE_API virtual Status flush(
      const CancellationToken& cancellation);
};

[[nodiscard]] SHIBORI_ENGINE_API Status require_capabilities(
    const ByteSource& source,
    ByteSourceCapabilities required);
[[nodiscard]] SHIBORI_ENGINE_API Status require_capabilities(
    const ByteSink& sink,
    ByteSinkCapabilities required);
[[nodiscard]] SHIBORI_ENGINE_API Status read_exact(
    ByteSource& source,
    std::span<std::byte> destination,
    const CancellationToken& cancellation = {});
[[nodiscard]] SHIBORI_ENGINE_API Status write_all(
    ByteSink& sink,
    std::span<const std::byte> source,
    const CancellationToken& cancellation = {});

class MemoryByteSource final : public ByteSource {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<
      std::unique_ptr<MemoryByteSource>>
  copy(
      std::span<const std::byte> bytes,
      std::size_t maximum_read_size = static_cast<std::size_t>(-1));
  [[nodiscard]] SHIBORI_ENGINE_API static Result<
      std::unique_ptr<MemoryByteSource>>
  share(
      std::shared_ptr<const std::vector<std::byte>> bytes,
      std::size_t maximum_read_size = static_cast<std::size_t>(-1));

  SHIBORI_ENGINE_API ~MemoryByteSource() override;
  [[nodiscard]] SHIBORI_ENGINE_API Result<ReadOutcome> read(
      std::span<std::byte> destination,
      const CancellationToken& cancellation) override;
  [[nodiscard]] SHIBORI_ENGINE_API ByteSourceCapabilities capabilities()
      const noexcept override;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint64_t> position()
      const noexcept override;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint64_t> size()
      const noexcept override;
  [[nodiscard]] SHIBORI_ENGINE_API Status seek(
      std::uint64_t position,
      const CancellationToken& cancellation) override;

 private:
  class Impl;
  explicit MemoryByteSource(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

class MemoryByteSink final : public ByteSink {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<std::unique_ptr<MemoryByteSink>>
  create(
      std::uint64_t maximum_size,
      std::size_t maximum_write_size = static_cast<std::size_t>(-1));

  SHIBORI_ENGINE_API ~MemoryByteSink() override;
  [[nodiscard]] SHIBORI_ENGINE_API Result<std::size_t> write(
      std::span<const std::byte> source,
      const CancellationToken& cancellation) override;
  [[nodiscard]] SHIBORI_ENGINE_API ByteSinkCapabilities capabilities()
      const noexcept override;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::uint64_t> position()
      const noexcept override;
  [[nodiscard]] SHIBORI_ENGINE_API Status seek(
      std::uint64_t position,
      const CancellationToken& cancellation) override;
  [[nodiscard]] SHIBORI_ENGINE_API Status flush(
      const CancellationToken& cancellation) override;
  [[nodiscard]] SHIBORI_ENGINE_API std::span<const std::byte> bytes()
      const noexcept;

 private:
  class Impl;
  explicit MemoryByteSink(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
