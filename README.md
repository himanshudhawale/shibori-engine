<p align="center">
  <img src="assets/shibori-engine-icon.svg" alt="Shibori Engine icon" width="96">
</p>

# Shibori Engine

Adaptive, type-aware compression for database data.

Shibori Engine analyzes typed blocks and selects an encoding and compression
codec based on data characteristics, compression ratio, and decode cost.

The project is in its design phase. Implementation contracts are documented
before the container and API are stabilized.

## Documentation

- [Product requirements](docs/product-requirements.md)
- [Architecture](docs/architecture.md)
- [Data model](docs/data-model.md)
- [Logical types](docs/logical-types.md)
- [Immutable schemas](docs/schemas.md)
- [Typed column storage](docs/columns.md)
- [Bounded immutable blocks](docs/blocks.md)
- [Bounded byte I/O](docs/byte-io.md)
- [Container format](docs/container-format.md)
- [Compression pipeline](docs/compression-pipeline.md)
- [Adaptive policy](docs/adaptive-policy.md)
- [Public API](docs/api.md)
- [Error handling](docs/error-handling.md)
- [Resource governance](docs/resource-governance.md)
- [Integrity value types](docs/integrity-values.md)
- [Reliability and security](docs/reliability-security.md)
- [Testing strategy](docs/testing.md)
- [Compatibility policy](docs/compatibility.md)
- [Build and packaging](docs/build-and-packaging.md)
- [Roadmap](docs/roadmap.md)
- [Support plan](docs/support.md)
- [Glossary](docs/glossary.md)

## License

Apache License 2.0.

Third-party dependency licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Build

Shibori Engine requires CMake 3.25 and a C++23 compiler.

```powershell
cmake --preset dev-static
cmake --build --preset dev-static
ctest --preset dev-static
```

Use `dev-shared` instead to build and test the shared library.
