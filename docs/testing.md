# Testing Strategy

## 1. Objectives

Testing must demonstrate exact round trips, bounded behavior on malformed
inputs, compatibility across versions and platforms, deterministic output where
promised, and performance claims that can be reproduced.

Test success is not based only on compression ratio.

## 2. Test Layers

### Unit tests

Cover checked arithmetic, varints, bit operations, offsets, schema validation,
statistics, score calculation, state machines, and individual transforms.

### Property tests

Generate schemas and values, apply every compatible encoding and codec chain,
and assert exact logical and floating-point bit-pattern round trips.

Properties include:

- decode(encode(values)) equals values;
- encoded size respects descriptor bounds;
- deterministic mode produces equal bytes;
- invalid parameters never reach unsafe operations;
- scalar and optimized implementations agree.

### Component tests

Exercise writer and reader flows with memory, file, short-read, short-write,
seekable, and non-seekable adapters.

### Contract tests

Validate the installed C++ API, C ABI, plugin descriptors, container fixtures,
and later SDK bindings without internal headers.

### Integration tests

Use public API packages as downstream projects on supported compilers and
platforms.

## 3. Golden Fixtures

The repository will maintain:

- minimal valid streaming and indexed containers;
- one fixture for every logical type;
- every built-in encoding and codec;
- nullable, all-null, empty-value, and multi-schema cases;
- deterministic containers with expected byte hashes;
- older format-minor fixtures retained permanently.

Golden files are generated only by an explicit maintenance command. Tests do
not silently rewrite expected bytes.

Each fixture has a human-readable manifest describing schema, values, expected
records, feature IDs, and digest.

## 4. Malformed Fixtures

Negative fixtures cover:

- magic, version, sync, and checksum corruption;
- truncated fixed and variable fields;
- overlong or overflowing varints;
- unknown mandatory records and features;
- reserved bits;
- duplicate IDs and invalid type parameters;
- out-of-range and overlapping chunk offsets;
- excessive decoded length and compression ratio;
- invalid dictionaries, indices, frames, runs, and padding;
- inconsistent footer totals and locator offsets.

Tests assert a stable error category and relevant context, not fragile complete
message text.

## 5. Fuzzing

Separate fuzz targets isolate:

1. preamble and record envelope;
2. file header and feature lists;
3. schema records;
4. block directories;
5. each encoding parameter parser;
6. each encoding decoder;
7. codec adapter bounds;
8. complete streaming reader;
9. indexed lookup;
10. C ABI options and lifecycle.

Fuzzers use low resource limits and allocation instrumentation. Corpus entries
include valid golden fixtures and minimized prior failures.

Every discovered crash becomes a regression test before closure.

## 6. Fault Injection

Source and sink adapters inject:

- short reads and writes;
- EOF at every byte boundary;
- I/O failure at every operation count;
- failed flush and seek;
- cancellation at every pipeline stage;
- allocation failure at reservation boundaries;
- worker scheduling permutations;
- plugin construction and execution failures.

Writer tests verify acknowledgement boundaries and terminal state. Reader tests
verify no partial block escapes.

## 7. Concurrency Tests

Tests vary worker count, queue depth, block count, and task scheduling. They use
thread sanitizers where available and assert:

- ordered publication;
- deterministic bytes independent of workers;
- no reservation leaks;
- safe context sharing across independent operations;
- cancellation and destruction do not race callbacks;
- no concurrent use is accidentally allowed on single-operation handles.

## 8. Compatibility Matrix

CI exercises:

- current writer to current reader;
- retained old writer fixtures to current reader;
- current writer constrained to old compatible minor format;
- C clients compiled against prior stable ABI headers;
- supported static and shared library configurations;
- little-endian supported architectures;
- Linux and Windows supported toolchains.

Breaking fixture changes require a format-major decision, not an expected-file
update.

## 9. Performance Tests

Microbenchmarks measure transforms and codecs. End-to-end benchmarks measure:

- complete serialized bytes;
- encode and decode throughput;
- median and tail block latency;
- peak tracked and process memory;
- analysis overhead;
- selected-read amplification;
- scaling by worker count.

Performance tests record CPU, OS, compiler, library versions, dataset seed,
policy, block size, and methodology version.

Correctness CI does not fail on noisy timing. Scheduled reference machines apply
statistical regression thresholds.

## 10. Coverage Expectations

Line coverage is diagnostic, not the release goal. Release review requires:

- every error category intentionally triggered;
- every parser field covered by valid and invalid input;
- every state transition exercised;
- every built-in chain round-tripped;
- every configured limit tested at below, equal, and above boundaries;
- every public C and C++ function covered through installed interfaces.

## 11. Release Gates

A release candidate must pass:

1. all unit, property, component, contract, and integration tests;
2. golden and malformed fixtures;
3. sanitizer jobs for supported configurations;
4. minimum continuous-fuzzing soak target without unresolved crash;
5. dependency and license inventory;
6. compatibility matrix for supported release lines;
7. deterministic output comparison;
8. reference benchmark review;
9. package-consumer builds;
10. documentation consistency and format registry checks.

Format 1 and API 1 additionally require successful prototype decoding through
at least one non-C++ language binding.
