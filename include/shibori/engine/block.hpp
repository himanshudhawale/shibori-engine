#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <shibori/engine/column.hpp>
#include <shibori/engine/resource.hpp>
#include <shibori/engine/schema.hpp>

namespace shibori::engine {

class Block {
 public:
  SHIBORI_ENGINE_API ~Block();
  SHIBORI_ENGINE_API Block(const Block& other);
  SHIBORI_ENGINE_API Block& operator=(const Block& other);
  SHIBORI_ENGINE_API Block(Block&& other) noexcept;
  SHIBORI_ENGINE_API Block& operator=(Block&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t id() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::uint64_t row_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const Schema& schema() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API std::size_t column_count() const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API const Column& column(
      std::size_t index) const;
  [[nodiscard]] SHIBORI_ENGINE_API const Column* find_column(
      std::uint32_t field_id) const noexcept;
  [[nodiscard]] SHIBORI_ENGINE_API Result<std::span<const std::byte>>
  variable_value(std::uint32_t field_id, std::uint64_t dense_index) const;

 private:
  friend class BlockBuilder;
  class Impl;
  explicit Block(std::shared_ptr<const Impl> impl) noexcept;
  std::shared_ptr<const Impl> impl_;
};

class BlockBuilder {
 public:
  SHIBORI_ENGINE_API BlockBuilder(
      Schema schema,
      std::uint64_t id,
      std::uint64_t row_count);
  SHIBORI_ENGINE_API ~BlockBuilder();
  BlockBuilder(const BlockBuilder&) = delete;
  BlockBuilder& operator=(const BlockBuilder&) = delete;
  SHIBORI_ENGINE_API BlockBuilder(BlockBuilder&& other) noexcept;
  SHIBORI_ENGINE_API BlockBuilder& operator=(BlockBuilder&& other) noexcept;

  [[nodiscard]] SHIBORI_ENGINE_API Status set_column(
      std::uint32_t field_id,
      Column column);
  [[nodiscard]] SHIBORI_ENGINE_API Result<Block> finish(
      const ResourceLimits& limits = default_resource_limits()) &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shibori::engine
