#include <shibori/engine/version.hpp>

#include <iostream>

int main() {
  constexpr auto expected = shibori::engine::Version{0, 1, 0};

  if (shibori::engine::version() != expected) {
    std::cerr << "Unexpected compile-time engine version\n";
    return 1;
  }

  if (shibori::engine::version_string() != "0.1.0") {
    std::cerr << "Unexpected runtime engine version\n";
    return 1;
  }

  return 0;
}
