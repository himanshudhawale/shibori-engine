#pragma once

#include <string_view>

#include <shibori/engine/export.hpp>

namespace shibori::engine {

struct Version {
  int major;
  int minor;
  int patch;

  friend constexpr bool operator==(const Version&, const Version&) = default;
};

[[nodiscard]] constexpr Version version() noexcept {
  return Version{0, 1, 0};
}

[[nodiscard]] SHIBORI_ENGINE_API std::string_view version_string() noexcept;

}  // namespace shibori::engine
