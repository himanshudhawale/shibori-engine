# Public API Design

**Status:** pre-implementation draft

## 1. Goals

The public API provides bounded, streaming access to compression,
decompression, inspection, and verification. It exposes Shibori concepts
without leaking third-party codec types or container parser internals.

Two surfaces are planned:

- an idiomatic C++23 API for native applications;
- a narrow C ABI used by Shibori SDK language bindings.

The C++ API may evolve before 1.0. The C ABI remains experimental until its
ownership, cancellation, callback, and compatibility contracts pass
cross-language tests.

## 2. Common Principles

1. Construction validates configuration before I/O begins.
2. Every operation receives explicit resource limits.
3. Inputs larger than memory are processed as bounded blocks.
4. Errors preserve category and context without exposing source values.
5. Cancellation is explicit and distinct from failure.
6. Writer and reader terminal states are observable.
7. No exception crosses the C ABI.
8. No public header includes a native codec header.
9. Ownership and thread safety are documented per type.
10. Unsupported requested behavior fails instead of silently degrading.

## 3. C++ Namespace and Value Types

The proposed namespace is `shibori`.

```cpp
namespace shibori {

enum class ErrorCategory;
class Error;

template <typename T>
using Result = /* expected-like value or Error */;

struct ResourceLimits;
struct ContextOptions;
class Context;

struct Field;
class Schema;
class Block;

struct CompressionPolicy;
struct DecisionExplanation;
struct OperationStatistics;

class ByteSource;
class ByteSink;
class Writer;
class Reader;

} // namespace shibori
```

The snippet describes shape, not final syntax. Public ownership uses RAII and
move-only operation handles. Value types are immutable after validation where
practical.

## 4. Error Model

`ErrorCategory` initially includes:

| Category | Meaning | Retry |
| --- | --- | --- |
| `invalid_argument` | caller configuration or lifecycle error | after correction |
| `invalid_data` | typed input violates its schema | after source correction |
| `io` | source or sink operation failed | source-dependent |
| `truncated` | input ended before a complete structure | after complete input |
| `corrupt` | checksum or structural invariant failed | no |
| `unsupported` | format feature, type, codec, or ABI unavailable | after capability change |
| `resource_limit` | configured bound would be exceeded | with explicit new limit |
| `cancelled` | cancellation requested | yes |
| `plugin` | trusted plugin violated or failed its contract | plugin-dependent |
| `internal` | engine invariant failed | no |

An error contains category, stable code, operation, message, optional byte
offset, block ID, field ID, component ID, and nested cause. Messages are for
humans; callers branch on category and stable code.

## 5. Context

A `Context` owns:

- encoding and codec registry;
- worker executor;
- default resource limits;
- allocator hooks where supported;
- diagnostic sink;
- optional trusted plugin handles;
- engine and capability information.

```cpp
Result<Context> Context::create(ContextOptions options);
Capabilities Context::capabilities() const;
```

Contexts are safe for concurrent creation of independent operations. Destroying
a context waits for or rejects outstanding operations according to the final
ownership model; it never leaves callbacks targeting freed state.

## 6. Resource Limits

```cpp
struct ResourceLimits {
  uint64_t maximum_resident_bytes;
  uint64_t maximum_block_rows;
  uint64_t maximum_decoded_block_bytes;
  uint64_t maximum_record_bytes;
  uint64_t maximum_metadata_bytes;
  uint32_t maximum_fields;
  uint32_t maximum_encoding_depth;
  uint32_t maximum_workers;
  uint32_t maximum_queued_blocks;
};
```

Every field has a safe library default and validated implementation maximum.
Zero does not mean unlimited; optional unlimited behavior must use a named
constant and is prohibited for untrusted reads.

## 7. Schema Construction

Schemas are built and validated before creating a writer.

```cpp
SchemaBuilder builder;
builder.add(Field{
    .id = 1,
    .name = "event_time",
    .type = TimestampType{TimeUnit::microsecond, TimezoneMode::instant},
    .nullable = false,
});
Result<Schema> schema = std::move(builder).finish();
```

`finish` validates identifiers, names, type parameters, metadata limits, and
portable format support. The resulting schema owns its strings and metadata.

## 8. Blocks and Ownership

A block contains one typed column per schema field. Builders may borrow caller
buffers until block submission returns or transfer shared ownership through an
explicit buffer object.

```cpp
BlockBuilder builder(schema, row_count);
builder.set_int64(field_id, values, validity);
Result<Block> block = std::move(builder).finish();
```

The final API will provide specific typed setters rather than an untyped pointer
plus enum. A finished block is immutable.

Writer submission states whether it:

- copies before returning;
- retains a reference until acknowledgement;
- takes ownership.

The default safe API takes an immutable owned or shared block.

## 9. Byte Sinks and Sources

```cpp
class ByteSink {
 public:
  virtual Result<size_t> write(std::span<const std::byte>) = 0;
  virtual Result<void> flush() = 0;
  virtual std::optional<uint64_t> position() const = 0;
  virtual Result<void> seek(uint64_t);
  virtual ~ByteSink() = default;
};
```

`ByteSource` provides bounded reads and optional seek and size. Short operations
are valid. Returning zero without EOF or error is invalid.

Adapters will cover files, memory spans, and callback-backed streams. Flush does
not imply durable media synchronization.

## 10. Writer

```cpp
struct WriterOptions {
  Schema schema;
  CompressionPolicy policy;
  ResourceLimits limits;
  ContainerMode mode;
  bool deterministic;
  bool logical_digest;
};

Result<Writer> Writer::open(
    Context& context,
    std::unique_ptr<ByteSink> sink,
    WriterOptions options);

Result<BlockAcknowledgement> Writer::write(Block block);
Result<WriterSummary> Writer::finish();
Result<void> Writer::cancel();
```

`write` applies backpressure and returns only after the block is accepted under
the documented ownership contract. An acknowledgement identifies the block and
published byte range.

`finish` writes required terminal structures and returns totals. After a
successful finish, repeated finish returns the same summary. Any other method
except inspection fails.

An asynchronous SDK may build on a separate completion interface; the initial
C++ API does not hide threads behind `std::future`.

## 11. Reader

```cpp
struct ReaderOptions {
  ResourceLimits limits;
  VerificationLevel verification;
  std::optional<FieldProjection> projection;
};

Result<Reader> Reader::open(
    Context& context,
    std::unique_ptr<ByteSource> source,
    ReaderOptions options);

const ContainerInfo& Reader::info() const;
Result<std::optional<Block>> Reader::read_next();
Result<Block> Reader::read_block(uint64_t block_id);
Result<VerificationSummary> Reader::verify_all();
Result<void> Reader::cancel();
```

`read_block` requires a valid index and seekable source. `read_next` returns no
value only at a structurally valid end of available records; a required footer
policy can turn an unfinalized stream into an error.

Decoded blocks are immutable and own or safely share their buffers independently
from the next reader call.

## 12. Inspection and Verification

Inspection exposes:

- format and feature versions;
- schema segments;
- finalized state;
- block index summaries;
- encoding and codec identifiers;
- logical, encoded, and compressed sizes;
- integrity fields and declared digests.

Verification levels:

1. `structure`: framing, bounds, flags, and metadata checksums;
2. `compressed`: all chunk and record checksums;
3. `decoded`: full decode and logical invariants;
4. `logical_digest`: decoded verification plus footer logical digest.

A level is never reported complete if a required check was skipped.

## 13. Statistics and Explanations

Statistics are immutable snapshots. Diagnostic callbacks receive structured
events but cannot alter operation decisions.

```cpp
using DiagnosticCallback = void(const DiagnosticEvent&);
```

Callbacks execute under documented threading rules, must not call the same
operation recursively, and must return quickly. Callback exceptions are caught
at the C++ boundary and converted to callback errors or ignored according to an
explicit diagnostic policy; they never unwind through worker threads.

## 14. Cancellation

Operations accept a context-owned cancellation token and expose `cancel`.
Cancellation is cooperative:

- admission stops promptly;
- workers check at bounded intervals;
- codec calls stop when their APIs permit;
- no unfinished block is acknowledged;
- owned resources are released;
- the operation enters a terminal cancelled state.

Cancellation does not delete or truncate caller-owned destinations. Higher
layers own temporary-file cleanup.

## 15. Thread Safety

| Type | Concurrent use |
| --- | --- |
| `Context` | yes, for independent operations |
| `Schema` | yes, immutable |
| `Block` | yes, immutable |
| `Writer` | no concurrent method calls unless later specified |
| `Reader` | no concurrent method calls unless later specified |
| statistics snapshots | yes, immutable |
| builders | no |

Moving an operation while calls are active is invalid.

## 16. C ABI

The ABI uses opaque handles, fixed-width integer types, byte spans, explicit
struct sizes, and status returns.

```c
typedef struct shibori_context shibori_context;
typedef struct shibori_writer shibori_writer;
typedef struct shibori_reader shibori_reader;

typedef struct {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t maximum_resident_bytes;
  /* fields appended in later compatible ABI versions */
} shibori_limits;

shibori_status shibori_context_create(
    const shibori_context_options* options,
    shibori_context** out_context);

void shibori_context_destroy(shibori_context* context);
```

Every options struct begins with `struct_size` and ABI version. The library
reads only fields within the supplied size and uses documented defaults for
absent trailing fields.

### ABI ownership

- Handles are created and destroyed by the same loaded library.
- Null destroy is safe.
- Byte spans are borrowed only for the duration documented by the call.
- Returned strings are copied into caller buffers or exposed through a
  library-owned view with a documented lifetime.
- Allocations are never freed by a different runtime allocator.

### ABI errors

Functions return a status containing a stable category and code. Detailed error
information is copied from the operation or context into caller-provided
buffers. Thread-local global errors are avoided because callbacks and language
runtimes complicate their lifetime.

### ABI callbacks

Callbacks receive a user-data pointer and never C++ objects. The binding must
keep user data alive until deregistration completes. Callback invocation,
reentrancy, and shutdown are covered by contract tests.

## 17. Capability Negotiation

Capabilities identify:

- supported format major and minor range;
- logical type IDs;
- encoding and codec IDs and versions;
- streaming, indexing, deterministic, and digest support;
- plugin ABI version;
- implementation limits.

Callers can validate requirements before opening output. Actual container
features remain authoritative during reading.

## 18. API Evolution

Before 1.0, breaking C++ changes are allowed with release notes. After 1.0:

- existing valid source usage remains supported within the published window;
- new options receive safe defaults;
- enum extension uses explicit unknown handling;
- format and API versions remain independent;
- deprecated APIs include a migration path and removal release.

The stable C ABI adds trailing struct fields and new functions. It does not
reorder fields, change integer widths, expose C++ layout, or reuse status codes.

## 19. API Acceptance Tests

The API is ready for implementation release when tests prove:

1. bounded file, memory, and callback source/sink round trips;
2. ownership safety after caller buffers are released;
3. writer and reader state transitions;
4. cancellation at every stage;
5. concurrent independent operations under one context;
6. typed errors with preserved offsets and identifiers;
7. C callers built by supported compilers can load the ABI;
8. Python and Java prototypes complete cross-language round trips;
9. old clients load additive ABI revisions;
10. no third-party codec type appears in installed headers.
