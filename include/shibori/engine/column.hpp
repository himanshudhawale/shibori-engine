#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <shibori/engine/export.hpp>
#include <shibori/engine/logical_type.hpp>

namespace shibori::engine {

enum class BufferOwnership : std::uint8_t {
  owned,
  shared,
};

class ByteBuffer {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<ByteBuffer> copy(
      std::span<const std::byte> bytes);
  [[nodiscard]] SHIBORI_ENGINE_API static Result<ByteBuffer> share(
      std::shared_ptr<const std::vector<std::byte>> bytes);

  SHIBORI_ENGINE_API ~ByteBuffer();
  SHIBORI_ENGINE_API ByteBuffer(const ByteBuffer& other);
  SHIBORI_ENGINE_API ByteBuffer& operator=(const ByteBuffer& other);
  SHIBORI_ENGINE_API ByteBuffer(ByteBuffer&& other) noexcept;
  SHIBORI_ENGINE_API ByteBuffer& operator=(ByteBuffer&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API std::span<const std::byte> bytes()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t size() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API BufferOwnership ownership() const noexcept;

 private:
  class Impl;
  explicit ByteBuffer(std::shared_ptr<const Impl> impl) noexcept;
  std::shared_ptr<const Impl> impl_;
};

class OffsetBuffer {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<OffsetBuffer> copy(
      std::span<const std::uint64_t> offsets);
  [[nodiscard]] SHIBORI_ENGINE_API static Result<OffsetBuffer> share(
      std::shared_ptr<const std::vector<std::uint64_t>> offsets);

  SHIBORI_ENGINE_API ~OffsetBuffer();
  SHIBORI_ENGINE_API OffsetBuffer(const OffsetBuffer& other);
  SHIBORI_ENGINE_API OffsetBuffer& operator=(const OffsetBuffer& other);
  SHIBORI_ENGINE_API OffsetBuffer(OffsetBuffer&& other) noexcept;
  SHIBORI_ENGINE_API OffsetBuffer& operator=(OffsetBuffer&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API std::span<const std::uint64_t> values()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t size() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API BufferOwnership ownership() const noexcept;

 private:
  class Impl;
  explicit OffsetBuffer(std::shared_ptr<const Impl> impl) noexcept;
  std::shared_ptr<const Impl> impl_;
};

class Validity {
 public:
  [[nodiscard]] SHIBORI_ENGINE_API static Result<Validity> from_bitmap(
      std::uint64_t row_count,
      ByteBuffer bitmap);

  SHIBORI_ENGINE_API ~Validity();
  SHIBORI_ENGINE_API Validity(const Validity& other);
  SHIBORI_ENGINE_API Validity& operator=(const Validity& other);
  SHIBORI_ENGINE_API Validity(Validity&& other) noexcept;
  SHIBORI_ENGINE_API Validity& operator=(Validity&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t row_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t null_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API bool is_valid(
      std::uint64_t row) const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const ByteBuffer& bitmap() const noexcept;

 private:
  class Impl;
  explicit Validity(std::shared_ptr<const Impl> impl) noexcept;
  std::shared_ptr<const Impl> impl_;
};

class ValidityBuilder {
 public:
  explicit SHIBORI_ENGINE_API ValidityBuilder(std::uint64_t row_count);
  SHIBORI_ENGINE_API ~ValidityBuilder();
  ValidityBuilder(const ValidityBuilder&) = delete;
  ValidityBuilder& operator=(const ValidityBuilder&) = delete;
  SHIBORI_ENGINE_API ValidityBuilder(ValidityBuilder&& other) noexcept;
  SHIBORI_ENGINE_API ValidityBuilder& operator=(
      ValidityBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status append(bool valid);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Validity> finish() &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

enum class ColumnStorageKind : std::uint8_t {
  fixed_width,
  boolean,
  variable_width,
};

class Column {
 public:
  SHIBORI_ENGINE_API ~Column();
  SHIBORI_ENGINE_API Column(const Column& other);
  SHIBORI_ENGINE_API Column& operator=(const Column& other);
  SHIBORI_ENGINE_API Column(Column&& other) noexcept;
  SHIBORI_ENGINE_API Column& operator=(Column&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API const LogicalType& type() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API ColumnStorageKind storage_kind()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t row_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t null_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const Validity* validity() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const ByteBuffer* values() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const OffsetBuffer* offsets() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const ByteBuffer* payload() const noexcept;

 private:
  friend class FixedWidthColumnBuilder;
  friend class BooleanColumnBuilder;
  friend class VariableWidthColumnBuilder;
  class Impl;
  explicit Column(std::shared_ptr<const Impl> impl) noexcept;
  std::shared_ptr<const Impl> impl_;
};

class FixedWidthColumnBuilder {
 public:
  SHIBORI_ENGINE_API FixedWidthColumnBuilder(
      LogicalType type,
      std::uint64_t row_count);
  SHIBORI_ENGINE_API ~FixedWidthColumnBuilder();
  FixedWidthColumnBuilder(const FixedWidthColumnBuilder&) = delete;
  FixedWidthColumnBuilder& operator=(const FixedWidthColumnBuilder&) = delete;
  SHIBORI_ENGINE_API FixedWidthColumnBuilder(
      FixedWidthColumnBuilder&& other) noexcept;
  SHIBORI_ENGINE_API FixedWidthColumnBuilder& operator=(
      FixedWidthColumnBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status set_validity(Validity validity);
  [[nodiscard]] SHIBORI_ENGINE_API Status set_values(ByteBuffer values);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Column> finish() &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class BooleanColumnBuilder {
 public:
  explicit SHIBORI_ENGINE_API BooleanColumnBuilder(std::uint64_t row_count);
  SHIBORI_ENGINE_API ~BooleanColumnBuilder();
  BooleanColumnBuilder(const BooleanColumnBuilder&) = delete;
  BooleanColumnBuilder& operator=(const BooleanColumnBuilder&) = delete;
  SHIBORI_ENGINE_API BooleanColumnBuilder(
      BooleanColumnBuilder&& other) noexcept;
  SHIBORI_ENGINE_API BooleanColumnBuilder& operator=(
      BooleanColumnBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status set_validity(Validity validity);
  [[nodiscard]] SHIBORI_ENGINE_API Status set_values(ByteBuffer bit_values);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Column> finish() &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class VariableWidthColumnBuilder {
 public:
  SHIBORI_ENGINE_API VariableWidthColumnBuilder(
      LogicalType type,
      std::uint64_t row_count);
  SHIBORI_ENGINE_API ~VariableWidthColumnBuilder();
  VariableWidthColumnBuilder(const VariableWidthColumnBuilder&) = delete;
  VariableWidthColumnBuilder& operator=(
      const VariableWidthColumnBuilder&) = delete;
  SHIBORI_ENGINE_API VariableWidthColumnBuilder(
      VariableWidthColumnBuilder&& other) noexcept;
  SHIBORI_ENGINE_API VariableWidthColumnBuilder& operator=(
      VariableWidthColumnBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status set_validity(Validity validity);
  [[nodiscard]] SHIBORI_ENGINE_API Status set_offsets(OffsetBuffer offsets);
  [[nodiscard]] SHIBORI_ENGINE_API Status set_payload(ByteBuffer payload);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Column> finish() &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
