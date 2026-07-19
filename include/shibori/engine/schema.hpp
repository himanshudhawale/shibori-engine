#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <shibori/engine/export.hpp>
#include <shibori/engine/logical_type.hpp>
#include <shibori/engine/resource.hpp>

namespace shibori::engine {

class Field {
 public:
  SHIBORI_ENGINE_API ~Field();
  SHIBORI_ENGINE_API Field(const Field& other);
  SHIBORI_ENGINE_API Field& operator=(const Field& other);
  SHIBORI_ENGINE_API Field(Field&& other) noexcept;
  SHIBORI_ENGINE_API Field& operator=(Field&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API std::uint32_t id() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::string_view name() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const LogicalType& type() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API bool nullable() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::size_t metadata_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::span<const std::byte>>
  metadata(std::string_view key) const noexcept;

 private:
  friend class FieldBuilder;
  friend class SchemaBuilder;
  class Impl;
  explicit Field(std::shared_ptr<const Impl> impl) noexcept;

  std::shared_ptr<const Impl> impl_;
};

class FieldBuilder {
 public:
  SHIBORI_ENGINE_API FieldBuilder(
      std::uint32_t id,
      std::string name,
      LogicalType type,
      bool nullable);
  SHIBORI_ENGINE_API ~FieldBuilder();
  FieldBuilder(const FieldBuilder&) = delete;
  FieldBuilder& operator=(const FieldBuilder&) = delete;
  SHIBORI_ENGINE_API FieldBuilder(FieldBuilder&& other) noexcept;
  SHIBORI_ENGINE_API FieldBuilder& operator=(FieldBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status add_metadata(
      std::string key,
      std::span<const std::byte> value);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Field> finish(
      const ResourceLimits& limits = default_resource_limits()) &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class Schema {
 public:
  SHIBORI_ENGINE_API ~Schema();
  SHIBORI_ENGINE_API Schema(const Schema& other);
  SHIBORI_ENGINE_API Schema& operator=(const Schema& other);
  SHIBORI_ENGINE_API Schema(Schema&& other) noexcept;
  SHIBORI_ENGINE_API Schema& operator=(Schema&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API std::size_t field_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const Field& field(
      std::size_t index) const;
  [[nodiscard]] SHIBORI_ENGINE_API const Field* find_field(
      std::uint32_t id) const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::size_t metadata_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::optional<std::span<const std::byte>>
  metadata(std::string_view key) const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::span<const std::byte> canonical_bytes()
      const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::array<std::byte, 16> fingerprint()
      const noexcept;

 private:
  friend class SchemaBuilder;
  class Impl;
  explicit Schema(std::shared_ptr<const Impl> impl) noexcept;

  std::shared_ptr<const Impl> impl_;
};

class SchemaBuilder {
 public:
  SHIBORI_ENGINE_API SchemaBuilder();
  SHIBORI_ENGINE_API ~SchemaBuilder();
  SchemaBuilder(const SchemaBuilder&) = delete;
  SchemaBuilder& operator=(const SchemaBuilder&) = delete;
  SHIBORI_ENGINE_API SchemaBuilder(SchemaBuilder&& other) noexcept;
  SHIBORI_ENGINE_API SchemaBuilder& operator=(SchemaBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status add_field(Field field);
  [[nodiscard]] SHIBORI_ENGINE_API Status add_metadata(
      std::string key,
      std::span<const std::byte> value);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Schema> finish(
      const ResourceLimits& limits = default_resource_limits()) &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
