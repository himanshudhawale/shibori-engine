# Design

## Problem

Database data has structure that general-purpose compression cannot fully
exploit. Integers, timestamps, repeated strings, and sparse values each benefit
from different encodings. A single fixed codec also cannot balance storage,
CPU, and read latency for every workload.

## Responsibility

Shibori Engine owns the database-independent compression core:

- typed block and schema model;
- sampling and codec selection;
- reversible encoding and decoding;
- block metadata, checksums, and versioning;
- stable interfaces consumed by the CLI, SDK, and connectors.

It does not connect to databases, manage credentials, or replace transaction
and query engines.

## Initial Architecture

Input flows through validation, optional type-aware encoding, compression, and
container serialization. Decompression performs those stages in reverse and
verifies integrity before returning data.

The first codec set will use raw storage plus Zstandard and LZ4 fallbacks.
Planned encodings include delta and delta-of-delta integers, bit packing,
run-length encoding, dictionary encoding, null bitmaps, and Gorilla-style
floating-point encoding.

## Design Principles

- Lossless and deterministic by default.
- Streaming operation with bounded memory.
- Versioned, self-describing blocks.
- Explicit corruption and unsupported-version errors.
- Codec plugins isolated behind a stable contract.
- Measured decisions rather than assumptions about a dataset.

## Initial Validation

Round-trip property tests, malformed-input tests, golden container fixtures,
and comparative benchmarks will gate the first implementation.

## Component Model

The implementation is planned as a C++23 library with these internal modules:

1. `model` defines schemas, logical types, columns, and bounded blocks.
2. `analysis` gathers low-cost statistics from a configurable sample.
3. `policy` selects encodings and codecs under ratio, CPU, and latency goals.
4. `encoding` transforms typed values without losing information.
5. `codec` integrates general compression implementations.
6. `container` serializes headers, block indexes, metadata, and checksums.
7. `runtime` coordinates streaming, memory limits, cancellation, and metrics.

The public boundary will expose a native C++ API and a narrow C ABI used by the
SDK. The container specification will be documented independently from the
implementation.

## Container Invariants

Each file has a magic value, format version, feature flags, schema, and block
index. Each block records row count, encoded and decoded sizes, encoding chain,
codec parameters, and checksum. Readers reject unknown mandatory features and
integer overflows before allocation.

Format evolution is additive within a major version. A new major format
requires an explicit migration path; writers never silently emit a format newer
than requested.

## Policy Model

Policies combine a goal (`balanced`, `maximum-ratio`, or `fast-decode`) with
hard limits for block size, memory, and candidate evaluation time. Selection
compares raw storage and viable encoding chains against sampled data. A
candidate must beat raw storage by enough to cover its metadata and CPU cost.

## Edge Cases

Empty blocks, all-null columns, high-cardinality strings, non-monotonic
timestamps, NaN payloads, malformed UTF-8, huge values, schema evolution,
already-compressed payloads, and cancellation between blocks require explicit
tests. Binary values are never interpreted as text without a declared type.

## Alternatives

A database storage-engine plugin could be faster but would bind releases to one
database and risk transactional behavior. A remote compression service would
simplify deployment but add network cost and a sensitive-data boundary. The
chosen embeddable library and portable container provide the safest reusable
foundation; services and native plugins can be added later.
