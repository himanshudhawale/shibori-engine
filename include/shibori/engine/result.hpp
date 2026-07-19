#pragma once

#include <expected>
#include <utility>

#include <shibori/engine/error.hpp>

namespace shibori::engine {

template <typename T>
using Result = std::expected<T, Error>;

using Status = Result<void>;

template <typename T>
[[nodiscard]] std::unexpected<Error> fail(
    ErrorCode code,
    Operation operation,
    T&& message) {
  return std::unexpected(
      Error(code, operation, std::forward<T>(message)));
}

}  // namespace shibori::engine
