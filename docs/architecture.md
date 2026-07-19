# Engine Architecture

## 1. Scope

Shibori Engine is an embeddable C++23 library. It transforms validated typed
blocks into portable container blocks and reverses that transformation. The
architecture deliberately excludes database drivers, snapshot coordination,
network protocols, user configuration files, and terminal behavior.

The primary architectural constraint is that neither compression nor
decompression may require the complete dataset in memory.

## 2. Context

```text
Database or file source
        |
        v
Shibori Connector ---- schema + typed blocks + checkpoints
        |
        v
Shibori SDK ---------- language-safe API and resource ownership
        |
        v
Shibori Engine ------- validation, selection, encoding, container
        |
        v
Caller-owned sink ---- file, pipe, memory, or remote transport
```

On restore, the flow is reversed. The engine does not know whether a block came
from PostgreSQL, MongoDB, Parquet, or an application buffer.

## 3. Dependency Direction

```text
public API
    |
    v
operation coordinator
    +--> schema and block model
    +--> analyzer --> policy planner
    +--> encoding registry --> encoding implementations
    +--> codec registry ----> codec adapters
    +--> container writer/reader
    +--> resource governor
    +--> diagnostics

platform adapters --> byte source/sink, threads, dynamic loading
```

Dependencies point inward toward contracts and value types. Container code
depends on registered identifiers and parameter bytes, not concrete Zstandard
or LZ4 classes. Policy code sees candidate capabilities and measured outcomes,
not plugin implementation details.

Database connectors and language SDKs depend on the engine; the engine never
depends on them.

## 4. Major Components

### 4.1 Schema model

Owns logical types, field identifiers, nullability, type parameters, schema
validation, and canonical fingerprints. A schema is immutable after a writer or
reader operation begins.

### 4.2 Block model

Represents one bounded row range across all fields. Columns may be supplied as
canonical contiguous values, offsets plus payload, or a documented streaming
source. The model exposes values without prescribing a database layout.

### 4.3 Validator

Checks schema invariants and block conformance before analysis. Validation
includes lengths, offset monotonicity, null-map size, UTF-8 mode, decimal
precision, and configured resource limits. Invalid data never enters a codec.

### 4.4 Analyzer

Collects bounded statistics needed by candidate filters and cost estimates:

- null count and run distribution;
- numeric minimum, maximum, monotonicity, deltas, and bit widths;
- distinct estimates and exact dictionaries within a cap;
- string and binary lengths, prefixes, and byte entropy samples;
- floating-point XOR patterns and exceptional values;
- raw canonical size.

Analysis may sample, but any statistic used to prove an encoding precondition
must be validated across the complete block during encoding.

### 4.5 Policy planner

Builds compatible candidates, prunes those that violate policy or capability
constraints, spends a bounded evaluation budget, and chooses the lowest score.
It always includes raw canonical storage.

The planner returns an immutable execution plan containing encoding identifiers,
parameters, codec settings, expected limits, and an explanation record.

### 4.6 Encoding pipeline

Executes reversible type-aware transformations. Each stage declares accepted
physical input, produced physical output, worst-case size, scratch memory,
streaming capability, determinism, and parameter schema.

An encoding chain is fixed before bytes for that column chunk are committed.
Failed stages do not publish partial output.

### 4.7 Codec layer

Wraps general byte compressors behind a bounded interface. Adapters validate
levels and dictionaries, calculate maximum output sizes, map native failures to
engine errors, and report actual bytes and timing.

Third-party codec types do not cross the public API or container layer.

### 4.8 Container writer

Serializes the preamble, schema, block envelopes, column chunks, checksums,
optional index, and footer. It tracks offsets and summaries independently from
compression planning.

Two output modes are supported:

- **streaming:** every block is self-contained and the footer is optional;
- **indexed:** finalization writes a block index and footer for seekable reads.

### 4.9 Container reader

Parses under strict limits, resolves required encoding and codec capabilities,
verifies envelopes and chunks, and returns decoded typed blocks. The parser
separates byte acquisition from interpretation so files, pipes, and bounded
memory can use the same validation path.

### 4.10 Resource governor

Accounts for resident input, output, scratch buffers, dictionaries, queued
work, metadata, and plugin reservations. Work begins only after the required
budget is reserved. Reservations use checked arithmetic and release through
RAII.

### 4.11 Operation coordinator

Owns lifecycle, cancellation, ordering, parallel execution, backpressure, and
terminal state. It is the only component allowed to acknowledge blocks to a
caller.

### 4.12 Diagnostics and statistics

Receives structured events rather than formatted log strings. Events contain
operation and block identifiers, stage, duration, byte counts, technique IDs,
and error category. Values and credentials are excluded.

## 5. Compression Data Flow

```text
submit block
    |
    v
validate schema conformance and limits
    |
    v
reserve block and analysis budget
    |
    v
analyze columns in bounded parallel tasks
    |
    v
plan candidates and freeze execution plan
    |
    v
encode columns --> apply byte codecs --> build chunk metadata
    |
    v
assemble block envelope and checksum
    |
    v
write complete block to sink
    |
    v
publish acknowledgement and statistics
```

The coordinator may process multiple blocks concurrently, but sink publication
preserves input order unless an API explicitly requests unordered block IDs.
The initial API preserves order.

If a stage fails, the current block is not acknowledged. Already acknowledged
streaming blocks remain valid, but the container is incomplete until a valid
footer declares completion.

## 6. Decompression Data Flow

```text
read and validate preamble
    |
    v
read schema and establish capability set
    |
    v
read bounded block envelope
    |
    v
validate offsets, lengths, identifiers, and checksum
    |
    v
reserve decoded and scratch budgets
    |
    v
decompress chunks --> reverse encoding chains
    |
    v
validate reconstructed column and block invariants
    |
    v
publish immutable typed block
```

No decoded block is exposed until every required chunk has passed integrity and
structural checks. A caller may request projected fields in a future API, but
the reader must still validate all metadata needed to locate them safely.

## 7. Concurrency Model

An engine context owns a fixed worker pool or a caller-supplied executor.
Operations are independent and may run concurrently. Individual writer and
reader handles are not concurrently callable unless a method explicitly says
otherwise.

Within a writer:

- input submission applies bounded backpressure;
- analysis and column encoding may run in parallel;
- publication is ordered;
- statistics are aggregated after publication;
- cancellation stops admission, requests task cancellation, and drains owned
  resources without acknowledging unfinished blocks.

Codec adapters must declare whether instances are reusable, thread-safe, or
single-operation. The engine never assumes thread safety from a plugin.

## 8. Memory Model

Memory is divided into tracked categories:

| Category | Examples |
| --- | --- |
| Input | caller block retained during work |
| Analysis | samples, histograms, distinct tables |
| Encoded | intermediate delta or dictionary streams |
| Codec | compression input, output, native scratch |
| Container | metadata and assembled block bytes |
| Decode | reconstructed values and offsets |
| Queue | blocks waiting for ordered publication |

The operation budget limits engine-owned and explicitly retained memory.
Caller-owned memory not retained by the engine is outside the count. APIs state
whether data is borrowed, retained, or copied.

The planner rejects a candidate whose declared worst-case reservations cannot
fit. Actual allocation failure remains a typed resource error.

## 9. I/O Abstractions

`ByteSink` supports bounded writes, optional current position, optional seek,
flush, and cancellation. `ByteSource` supports bounded reads, optional size,
optional seek, and cancellation.

The engine does not equate `flush` with durable storage. Durability belongs to
the embedding application because filesystem and remote-storage semantics vary.

Short reads and writes are normal and retried by the adapter contract. A
zero-byte write without completion is an I/O error to prevent infinite loops.

## 10. State Machines

### Writer

```text
created --> open --> finalizing --> finalized
              |          |
              +--------> failed
              |
              +--------> cancelled
```

Only `open` accepts blocks. `finalized`, `failed`, and `cancelled` are terminal.
Finalization after successful finalization returns the existing summary;
finalization after failure returns the original failure.

### Reader

```text
created --> open --> exhausted
              |
              +--> failed
              |
              +--> cancelled
```

An integrity, compatibility, or parsing error permanently fails the reader. It
cannot skip a damaged block and continue under the standard API.

## 11. Failure Model

Errors carry:

- stable category and operation;
- format offset and block or field identity when known;
- codec or encoding identifier when relevant;
- source error chain without source data;
- retry classification;
- human-readable message.

Programming errors such as use after finalization are distinct from input
corruption. Exceptions do not cross the C ABI. No component converts failure
into raw storage after output for a candidate has been committed.

## 12. Extension Model

Built-in and external encodings and codecs register descriptors with a context.
A descriptor includes stable identifier, implementation version, ABI version,
capabilities, accepted types, parameter schema, size bounds, and construction
factory.

Registration fails on identifier collision. Containers identify the format
contract, not a library filename. Loading native plugins is opt-in because it
executes trusted code in process.

## 13. Security Boundaries

Container bytes, schemas, plugin parameters, and decoded sizes are untrusted.
The parser and resource governor defend the process from malformed inputs.

Native plugins, the embedding application, and caller-provided source/sink
implementations are trusted in-process code. Checksums detect accidental
corruption; they do not authenticate an attacker-controlled container.
Encryption and signatures must wrap the container or be supplied by another
specified layer.

## 14. Build and Packaging

The engine builds as:

- a core library with no dynamic plugin requirement;
- optional built-in codec adapters selected by build configuration;
- a C ABI library;
- test and fuzz targets not installed with production packages.

Public headers avoid third-party codec headers. CMake package metadata exports
version and optional capability information. Static and shared builds must
produce the same container bytes for deterministic configurations.

## 15. Architectural Invariants

1. Raw canonical storage is always representable.
2. No untrusted length is allocated before checked validation.
3. No decoded block is returned before integrity verification.
4. No acknowledged write depends on mutable caller memory.
5. Database product details do not enter the container core.
6. Format identifiers are stable and independent of C++ class names.
7. Optional parallelism cannot change logical output ordering.
8. Resource limits fail explicitly rather than silently changing guarantees.
9. Checksums are never described as authentication.
10. Every emitted format feature has a reader and a golden fixture.
