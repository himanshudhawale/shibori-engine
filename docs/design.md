# Initial Design

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
