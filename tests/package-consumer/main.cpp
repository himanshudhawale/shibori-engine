#include <shibori/engine/version.hpp>

int main() {
  return shibori::engine::version_string() == "0.1.0" ? 0 : 1;
}
