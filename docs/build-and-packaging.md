# Build and Packaging

## Goals

The build establishes a small C++23 core with no database or compression-codec
dependency. It supports static and shared libraries, local tests, installation,
and downstream consumption through `find_package`.

## Targets

The project builds `shibori_engine` and exports it as `Shibori::engine`.
Installed public headers contain only Shibori and C++ standard-library types.
Generated symbol-visibility declarations support both static and shared builds.

## Local workflow

Configure, build, and test a static development build:

```powershell
cmake --preset dev-static
cmake --build --preset dev-static
ctest --preset dev-static
```

Replace `dev-static` with `dev-shared` to exercise the shared-library boundary.
The presets intentionally leave generator selection to the host so Visual
Studio, Ninja, or Make can be used without repository-specific paths.

## Installation contract

Installation provides:

- the Shibori Engine library;
- public and generated export headers;
- `ShiboriEngineConfig.cmake`;
- compatible version metadata;
- `Shibori::engine` imported target.

A consumer uses:

```cmake
find_package(ShiboriEngine 0.1 REQUIRED)
target_link_libraries(application PRIVATE Shibori::engine)
```

The package uses same-major compatibility while the `0.x` API remains
experimental. Stable compatibility rules will be tightened before version 1.0.

## Continuous integration

CI builds static and shared variants on Linux and Windows, runs CTest, installs
each variant, and compiles a separate downstream consumer using only the
installed package.
