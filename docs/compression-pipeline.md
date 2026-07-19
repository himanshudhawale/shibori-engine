# Compression Pipeline

## 1. Purpose

Shibori combines type-aware reversible encodings with optional general byte
codecs. This document defines stage contracts, initial encoding behavior,
fallback rules, and correctness requirements. Exact serialized parameters
remain provisional until container format 1 freezes.

## 2. Pipeline Model

```text
logical column
    |
    v
canonical physical representation
    |
    +--> zero or more type-aware encodings
    |
    v
encoded byte stream
    |
    +--> zero or one byte codec
    |
    v
container chunk
```

Decompression applies the codec inverse, then reverses encodings in reverse
order. Every stage must be independently reversible under recorded parameters.

The planner chooses a complete chain before the writer commits chunk bytes.
Failure during execution fails the block; it does not silently substitute a
different chain.

## 3. Stage Contract

Each encoding descriptor declares:

- stable format identifier and implementation version;
- accepted logical and physical input types;
- produced physical type;
- parameter schema and validation function;
- minimum and maximum input lengths;
- worst-case output and scratch-memory bounds;
- whether it supports streaming, sampling, and deterministic mode;
- whether it requires complete-block statistics;
- encode, decode, and validate operations;
- supported CPU or platform implementations with identical semantics.

Each codec descriptor declares equivalent byte-level properties plus supported
levels, dictionary behavior, maximum compressed size, and native library
version.

Descriptors are immutable after registration with a context.

## 4. Canonical Input

Encodings consume the dense non-null value sequence plus separate row validity
unless their descriptor explicitly accepts a combined stream. This keeps null
positions from distorting numeric deltas and dictionaries.

The chunk reconstructs:

1. row count;
2. validity bitmap when nullable;
3. non-null value count;
4. canonical values in row order.

The planner accounts for validity bytes in every candidate's total size.

## 5. Raw Fallback

Raw storage is not an encoding stage. It is an empty encoding chain with codec
ID 0 over canonical chunk bytes.

Raw is always eligible unless the caller explicitly sets a policy that requires
compression and accepts failure when no candidate meets it. Standard policies
never remove raw.

Raw protects against:

- incompressible binary and encrypted values;
- tiny chunks where headers dominate;
- evaluation-budget exhaustion;
- unsupported plugin combinations;
- codec expansion;
- pathological data distributions.

## 6. Validity Bitmap

### Applicability

Nullable fields.

### Representation

One bit per row, least-significant bit first within each byte. One means valid.
Unused high bits in the final byte are zero.

The bitmap may be:

- stored directly;
- run-length encoded when long runs are present;
- passed to the selected byte codec.

It is logically separate from dense values even when serialized in the same
column chunk.

### Preconditions

Bitmap length must equal `ceil(row_count / 8)`. The number of set bits must
equal the dense value count.

## 7. Integer Delta Encoding

### Applicability

Signed and unsigned integers, date, time, timestamp, and duration physical
values.

### Transform

For values `v[0..n)`:

```text
base = v[0]
delta[i] = v[i] - v[i - 1], for i >= 1
```

Signed subtraction is implemented with a specified checked or modular mapping
that preserves every source bit pattern. Deltas are mapped to unsigned values
with ZigZag where the parameter contract requests it.

Output records the base, count, arithmetic mode, and transformed deltas.

### Selection signals

- monotonic or slowly changing values;
- delta bit width materially below value bit width;
- enough values to cover base and parameter overhead.

### Rejection conditions

- arithmetic mode cannot represent a reversible delta;
- sampled benefit is below threshold;
- block is empty or too small;
- configured maximum transformed width is exceeded.

Decoding validates count and arithmetic at every reconstruction step.

## 8. Delta-of-Delta Encoding

### Applicability

Ordered temporal and integer sequences whose increments are stable.

### Transform

The stream records the first value and first delta. Subsequent values store:

```text
delta2[i] = (v[i] - v[i - 1]) - (v[i - 1] - v[i - 2])
```

The arithmetic mapping and overflow behavior are explicit parameters.

### Selection signals

- timestamps with regular intervals;
- sequence numbers with stable increments;
- second-delta width below first-delta width.

It is never selected solely from a monotonicity flag; measured widths and
serialized overhead are required.

## 9. Frame-of-Reference and Bit Packing

### Applicability

Unsigned integers or signed values first mapped to a non-negative range.

### Transform

For a frame:

```text
base = minimum(values)
offset[i] = values[i] - base
bit_width = bits_required(max(offset))
```

Offsets are packed consecutively, least-significant bit first, with zero
padding. Bit width zero represents a constant frame.

Frames have a fixed value count chosen by policy within format limits. Smaller
frames adapt to local ranges but add headers.

### Parameters

- source width and signed mapping;
- frame value count;
- per-frame base;
- per-frame bit width;
- final frame count.

### Validation

Decoders verify frame counts, packed lengths, bit widths not exceeding the
source width, checked base addition, and zero padding.

## 10. Run-Length Encoding

### Applicability

Fixed-width values, dictionary indices, and bitmaps with repeated runs.

### Transform

Output is a sequence of `(run_length, value)` pairs. Run lengths are positive
minimal `varuint` values. The sum must equal the expected output count.

### Selection signals

- long identical runs;
- low transition count;
- clustered null patterns;
- sorted low-cardinality columns.

RLE is rejected when pair overhead is not lower than its input after accounting
for the downstream codec.

## 11. Dictionary Encoding

### Applicability

String, binary, UUID, fixed binary, and optionally other equality-comparable
fixed-width values.

### Transform

Distinct values are assigned deterministic unsigned indices. Output contains:

- dictionary entry count;
- canonical dictionary offsets and payload or fixed values;
- index width;
- packed or canonical index stream.

In deterministic mode, dictionary IDs follow first appearance in row order.
Other orderings require an explicit format parameter.

### Resource controls

Construction stops when any configured limit is reached:

- maximum entries;
- dictionary payload bytes;
- hash-table memory;
- analysis time;
- distinct-to-value ratio.

Stopping dictionary evaluation does not fail the block; it rejects that
candidate with a budget reason.

### Validation

Every decoded index must be below dictionary count. Dictionary offsets and
payload use the same validation rules as variable-width canonical columns.

## 12. Floating-Point XOR Encoding

### Applicability

`float32` and `float64`.

### Transform

The first value stores its exact IEEE bits. Each subsequent value XORs its bits
with the previous value. Encoded control information distinguishes:

- XOR is zero;
- reuse prior significant-bit window;
- declare a new leading/trailing-zero window.

The exact control-bit layout will be fixed with golden fixtures.

### Correctness

The transform preserves all bit patterns, including NaN payloads, infinities,
subnormals, and signed zero. It does not compare numeric equality.

### Selection signals

Repeated values and stable leading or trailing XOR zeros. Random mantissas
typically reject the candidate.

## 13. String-Specific Analysis

Format 1 initially relies on dictionary encoding and byte codecs for strings.
The analyzer may measure:

- count and total bytes;
- length distribution;
- exact distinct values under a cap;
- common-prefix estimates;
- UTF-8 byte classes;
- sampled byte entropy.

Prefix, suffix, token, or FSST-like encodings require separate specifications
and identifiers. The planner must not invent undocumented transformations.

## 14. Codec Layer

### No codec

Codec ID 0 copies encoded bytes unchanged. It remains a candidate after a useful
type encoding when another codec adds no benefit.

### Zstandard

The adapter exposes a bounded set of canonical levels rather than every native
library option. Initial candidates should include a fast, balanced, and
high-ratio level mapped by engine version.

The container records the actual canonical level and any dictionary identifier.
Writer defaults changing in future versions do not alter old files.

### LZ4 block

LZ4 uses independent bounded blocks suitable for fast decode. The format records
the canonical acceleration or mode parameters needed for reproducibility.

### Dictionaries

Codec dictionaries are separate from column value dictionaries. A container
using a codec dictionary must embed it in a specified record or identify an
external required feature. Format 1 MVP should embed required dictionaries to
preserve portability.

## 15. Candidate Composition

The registry declares legal physical transitions. Example candidates for an
integer column may include:

```text
raw
raw -> Zstandard
raw -> LZ4
delta
delta -> Zstandard
frame-of-reference -> bit-pack
frame-of-reference -> bit-pack -> Zstandard
RLE
RLE -> Zstandard
```

The planner does not generate arbitrary stage permutations. It uses registered
templates with validated type transitions and maximum chain depth.

## 16. Size Accounting

Candidate size includes:

- validity stream;
- encoding parameters and per-frame headers;
- dictionaries and offsets;
- encoded values;
- codec parameters;
- column directory entry;
- chunk checksum;
- alignment or padding when defined.

Ratio is:

```text
canonical_logical_bytes / complete_candidate_bytes
```

The engine also reports savings:

```text
1 - complete_candidate_bytes / canonical_logical_bytes
```

Empty canonical input reports bytes rather than an infinite or undefined ratio.

## 17. Execution and Buffering

Each stage receives a reserved output bound and scratch budget. Where possible,
stages write into engine-owned buffers that can move into the next stage without
copying.

The operation coordinator may:

- fuse analysis with canonicalization;
- reuse buffers after their last consumer;
- execute independent columns in parallel;
- spill no intermediate data unless a future explicit spill API is configured.

Optimizations cannot alter serialized semantics or bypass validation.

## 18. Failure Rules

- Invalid input fails before candidate execution.
- Candidate budget exhaustion during evaluation rejects only that candidate.
- Required plugin absence fails planning.
- Allocation or codec failure during final execution fails the block.
- Output larger than a descriptor's declared maximum is an internal or plugin
  contract error.
- Decoded output not matching declared length is corruption.
- Reconstructed values violating type invariants are corruption.

No stage returns partially decoded values.

## 19. SIMD and Platform Optimizations

Optimized implementations may accelerate bit packing, checksums, statistics,
and transforms. Runtime dispatch must preserve:

- byte-identical deterministic encoding;
- identical validation;
- identical overflow handling;
- identical error categories;
- safe fallback when an instruction set is unavailable.

Every optimized decoder is tested against the scalar reference implementation.

## 20. Verification Requirements

For every stage and supported composition:

1. Property tests generate valid values and assert exact round trips.
2. Boundary tests cover empty, one-value, all-null, maximum-width, and maximum-
   length inputs.
3. Malformed tests mutate counts, widths, parameters, padding, and lengths.
4. Differential tests compare scalar and optimized implementations.
5. Deterministic tests compare bytes across worker counts.
6. Fuzz targets exercise parameter parsing and decoding under strict budgets.
7. Benchmarks include favorable, neutral, and hostile distributions.
