# Roadmap

## Phase 0: Contracts

- Freeze logical types, error categories, codec plugin contract, and benchmark
  result schema.
- Publish container format draft and golden fixtures.
- Establish fuzzing, property tests, and compatibility CI.

## Phase 1: Engine MVP

- Implement bounded streaming blocks and raw container storage.
- Add Zstandard and LZ4 codecs.
- Add null bitmap, integer delta, bit packing, and string dictionary encodings.
- Ship `balanced`, `maximum-ratio`, and `fast-decode` policies.
- Release a C++ API and experimental C ABI.

## Phase 2: Adaptive Compression

- Add delta-of-delta, run-length, frame-of-reference, and Gorilla encodings.
- Introduce sample-based candidate selection and policy explanations.
- Add block index and selective decompression.
- Stabilize container format 1 and C ABI 1.

## Phase 3: Production Hardening

- Add corruption fuzzing, resource-limit enforcement, and long-running tests.
- Support schema evolution metadata and parallel block processing.
- Publish compatibility and security-support policies.
- Reach a stable 1.0 release after SDK and connector interoperability gates.

## Release Gates

Every release must pass cross-version golden fixtures, deterministic round
trips, sanitizer builds, malformed-input fuzzing, and benchmark regression
thresholds defined by Shibori Bench.
