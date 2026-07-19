#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <shibori/engine/export.hpp>
#include <shibori/engine/result.hpp>

namespace shibori::engine {

[[nodiscard]] SHIBORI_ENGINE_API Result<std::uint64_t> checked_add(
    std::uint64_t left,
    std::uint64_t right,
    Operation operation,
    std::string_view subject);

[[nodiscard]] SHIBORI_ENGINE_API Result<std::uint64_t> checked_multiply(
    std::uint64_t left,
    std::uint64_t right,
    Operation operation,
    std::string_view subject);

[[nodiscard]] SHIBORI_ENGINE_API Status checked_range(
    std::uint64_t offset,
    std::uint64_t length,
    std::uint64_t total_size,
    Operation operation,
    std::string_view subject);

template <std::integral To, std::integral From>
[[nodiscard]] Result<To> checked_cast(
    From value,
    Operation operation,
    std::string_view subject) {
  if (!std::in_range<To>(value)) {
    return std::unexpected(Error(
        ErrorCode::arithmetic_overflow,
        operation,
        std::string(subject) + " cannot be represented by the target type"));
  }
  return static_cast<To>(value);
}

}  // namespace shibori::engine
