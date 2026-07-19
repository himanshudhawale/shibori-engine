# Product Requirements

## 1. Purpose

Shibori Engine is a lossless, embeddable compression engine for structured
database data. It accepts typed data in bounded blocks, evaluates compatible
encodings and codecs, writes a portable self-describing container, and restores
the original logical values without requiring the source database.

This document defines the first stable product boundary. Database extraction,
credentials, snapshot management, command-line workflows, and language-specific
ergonomics belong to adjacent Shibori projects.

## 2. Problem Statement

General-purpose compressors treat database exports as byte streams. They cannot
reliably exploit facts such as:

- an integer column has a narrow range;
- timestamps are mostly ordered;
- a string column repeats a small vocabulary;
- nulls follow a sparse or clustered pattern;
- floating-point values change by small bit patterns;
- one column is incompressible and should bypass expensive analysis.

Database-native compression can exploit some of this structure, but its output
is normally tied to a particular product, version, page layout, or restore
process. Shibori needs a portable layer that preserves useful type information
without inheriting database transaction or storage-engine responsibilities.

## 3. Product Goals

### G1: Portable lossless round trips

A supported writer and reader must reproduce the same schema and logical values
on every supported platform. Container decoding must not depend on the original
database or host byte order.

### G2: Better decisions through type awareness

The engine must evaluate only transformations compatible with a column's
logical type and statistics. Adaptive behavior must be explainable: callers can
inspect what was selected and why.

### G3: Predictable resource use

Compression and decompression must operate incrementally under explicit block,
memory, worker, and analysis limits. Inputs larger than memory must be
supported.

### G4: Safe long-term storage

Containers must be versioned, self-describing, integrity-protected, and strict
about malformed sizes and unsupported mandatory features.

### G5: Reusable embedding boundary

The engine must expose a native C++ API and a narrow C ABI suitable for the
Shibori SDK. It must not require a database driver, network service, or CLI.

### G6: Evidence-based performance

Compression ratio, encode cost, decode cost, memory, and metadata overhead must
be measurable under a reproducible methodology. No policy may claim universal
superiority.

## 4. Non-Goals for Version 1

- Replacing a database storage engine or buffer manager.
- Intercepting live SQL queries or changing transactional behavior.
- Taking database snapshots or managing source credentials.
- Compressing encrypted values before they are decrypted.
- Deduplicating blocks across independent containers.
- Providing lossy numeric, image, audio, or model compression.
- Hiding arbitrary schemas or metadata through encryption.
- Guaranteeing meaningful size reduction for already-compressed or random data.
- Supporting in-place mutation of a finalized container.
- Automatically uploading containers to remote storage.

## 5. Primary Users

### Backup and archive tools

These tools need portable output, bounded memory, integrity verification,
streaming writes, and reliable restoration years later.

### Database connectors

Connectors need a stable typed-block contract, explicit schema mapping,
backpressure, cancellation, and statistics that can be included in an export
manifest.

### Data infrastructure developers

Developers embedding the engine need deterministic APIs, typed errors, control
over resource budgets, and compatibility negotiation.

### Performance engineers

They need policy explanations and detailed counters to compare Shibori with
general-purpose codecs under repeatable workloads.

## 6. Core Use Cases

### UC1: Compress a typed snapshot stream

1. A caller creates a writer with a schema, policy, and resource limits.
2. The caller submits ordered blocks that conform to the schema.
3. The engine validates each block, selects compatible transformations, and
   writes complete container blocks.
4. The writer finalizes the index and file-level metadata.
5. The caller receives totals, selected techniques, and a content checksum.

Success means the container is independently readable and every acknowledged
block is represented exactly once.

### UC2: Restore a container sequentially

1. A caller opens a reader under explicit size and memory limits.
2. The engine validates the preamble, version, schema, and feature flags.
3. The caller reads typed blocks until end of stream.
4. The engine verifies each block before exposing decoded values.
5. The caller verifies final totals and optional file-level checksum.

Success means the original schema and logical values are returned in order.

### UC3: Inspect without decoding all values

A caller reads the schema, format version, feature requirements, block index,
encoding chains, codec settings, row counts, sizes, and checksums. Inspection
must not allocate buffers based on untrusted values before limits are checked.

### UC4: Verify archival integrity

Verification checks structural validity, supported features, metadata
consistency, block checksums, decoded lengths, and optional logical checksums.
The result identifies the first failing region and never reports success after
a skipped mandatory check.

### UC5: Evaluate a policy on a sample

A caller submits a representative block and resource budget. The engine returns
candidate outcomes, rejected candidates, estimated costs, and the selected
encoding chain without requiring a complete container write.

### UC6: Read selected blocks

For a finalized seekable container, a caller uses the block index to retrieve a
known block range. The engine verifies every retrieved block. Version 1 does not
promise predicate evaluation or arbitrary row-level random access.

## 7. Functional Requirements

### Input and schema

- **FR-001:** The engine shall represent a schema as ordered fields with stable
  identifiers, names, logical types, nullability, and type parameters.
- **FR-002:** The engine shall reject duplicate field identifiers and invalid
  type parameters before writing data.
- **FR-003:** Every submitted block shall declare a row count and provide one
  value vector or encoded source for every schema field.
- **FR-004:** Block validation shall detect length mismatches, invalid null
  maps, values outside declared type constraints, and unsupported types.
- **FR-005:** Binary values shall never be interpreted as text unless the
  schema declares a text type and validation mode.

### Compression

- **FR-010:** Raw storage shall always be an available fallback.
- **FR-011:** The engine shall apply only encoding chains registered as
  reversible for the field's logical type.
- **FR-012:** Candidate evaluation shall obey configured time, byte, and memory
  budgets.
- **FR-013:** A selected candidate shall account for its complete serialized
  size, including dictionaries, indexes, parameters, and padding.
- **FR-014:** The caller shall be able to request deterministic output for a
  fixed engine version, configuration, schema, and input sequence.
- **FR-015:** A caller shall be able to prohibit specific codecs or encodings.
- **FR-016:** Failure to load an explicitly required codec shall be an error,
  not a silent fallback.

### Container writing

- **FR-020:** Writers shall emit a format preamble before data blocks.
- **FR-021:** A writer shall acknowledge a block only after its complete bytes
  have been accepted by the configured output sink.
- **FR-022:** Finalization shall write or complete the block index and summary
  metadata when the output mode supports them.
- **FR-023:** Finalization shall be idempotent only after a successful first
  completion; writing after finalization shall fail.
- **FR-024:** A failed writer shall remain failed and reject subsequent writes.
- **FR-025:** Non-seekable output shall use a format mode that does not require
  rewriting previously emitted bytes.

### Container reading

- **FR-030:** Readers shall validate magic, version, feature flags, lengths, and
  checksums before exposing decoded data.
- **FR-031:** Readers shall reject unknown mandatory features and may ignore
  unknown optional metadata only where the format permits it.
- **FR-032:** All untrusted lengths and counts shall be checked against
  configured limits and arithmetic overflow before allocation.
- **FR-033:** Readers shall distinguish truncation, corruption, unsupported
  features, resource exhaustion, and I/O failures.
- **FR-034:** Seekable readers shall expose block-index metadata when present.
- **FR-035:** Readers shall not return partially verified decoded blocks.

### Observability

- **FR-040:** Operations shall report logical bytes, encoded bytes, compressed
  bytes, rows, blocks, elapsed stages, and selected techniques.
- **FR-041:** Policy explanations shall identify evaluated candidates and
  machine-readable rejection reasons.
- **FR-042:** Diagnostics shall never include input values unless a caller
  explicitly installs a value-aware diagnostic sink.
- **FR-043:** Cancellation shall be observable as a distinct result.

### Extensibility

- **FR-050:** Codec and encoding identifiers in the container shall be stable
  numeric values governed by the format specification.
- **FR-051:** Plugins shall declare ABI version, capabilities, parameter schema,
  and deterministic behavior.
- **FR-052:** A container requiring an unavailable plugin shall fail with the
  missing identifier and version requirement.

## 8. Non-Functional Requirements

### Correctness

- **NFR-001:** Every supported encoding and codec combination must pass
  property-based round-trip tests.
- **NFR-002:** Cross-platform golden fixtures must decode to identical logical
  values.
- **NFR-003:** Checksums must cover enough metadata to prevent data from being
  interpreted under a corrupted encoding description.

### Resource control

- **NFR-010:** Peak engine-managed memory shall remain within the configured
  budget or fail before exceeding it.
- **NFR-011:** Default blocks shall be bounded; no API shall require loading the
  complete input or output.
- **NFR-012:** Worker counts and queued blocks shall have explicit limits.
- **NFR-013:** Cancellation shall be checked at bounded intervals during
  analysis, encoding, codec execution where possible, and I/O coordination.

### Performance

- **NFR-020:** The raw path shall avoid unnecessary value transformation.
- **NFR-021:** Candidate analysis shall be bounded independently from final
  compression work.
- **NFR-022:** Performance claims shall specify dataset, policy, codec settings,
  hardware, worker count, and methodology version.
- **NFR-023:** A policy may choose raw storage when compression benefit does not
  cover configured cost and metadata thresholds.

### Portability

- **NFR-030:** The format shall define byte order, integer widths, floating-point
  representation, string encoding, and alignment independent of the host ABI.
- **NFR-031:** Stable releases shall support Linux and Windows on x86-64.
- **NFR-032:** The build shall not require a database client library.

### Security and resilience

- **NFR-040:** Parsers shall be fuzzed with malformed and adversarial inputs.
- **NFR-041:** The reader shall enforce configurable limits for nesting,
  fields, blocks, rows, decoded bytes, metadata bytes, and dictionary entries.
- **NFR-042:** Codec execution shall receive bounded input and output sizes.
- **NFR-043:** The project shall publish security-supported release lines before
  declaring version 1 production-ready.

## 9. Product Boundaries

| Concern | Owner |
| --- | --- |
| Typed block model and container | Shibori Engine |
| Encoding and codec selection | Shibori Engine |
| C++ API and C ABI | Shibori Engine |
| Idiomatic language packages | Shibori SDK |
| Database snapshots and type mapping | Shibori Connectors |
| Commands, config, and terminal UX | Shibori CLI |
| Comparative methodology and datasets | Shibori Bench |
| Authentication and secret storage | Embedding application or connector |
| Encryption and key management | External layer |
| Remote object transport | Connector or embedding application |

## 10. Acceptance Criteria for Engine MVP

The MVP is complete when:

1. A documented primitive schema can be written and restored through bounded
   streaming APIs.
2. Raw, Zstandard, and LZ4 block codecs interoperate with null-map, integer
   delta, bit-pack, and string-dictionary encodings.
3. A finalized container is self-describing and independently inspectable.
4. Corruption, truncation, unsupported features, and resource-limit violations
   produce distinct typed errors.
5. Non-seekable writing and sequential reading work without buffering the full
   container.
6. Golden fixtures decode on Linux and Windows x86-64.
7. Property tests and fuzzing cover each parser and reversible transformation.
8. Shibori Bench can compare all MVP policies against raw, Zstandard, and LZ4.
9. The experimental C ABI can perform compress, decompress, inspect, and verify
   operations without exposing C++ types.
10. The specification documents every emitted byte and compatibility rule.

## 11. Risks

| Risk | Consequence | Mitigation |
| --- | --- | --- |
| Format is stabilized too early | Permanent complexity or poor layout | Golden drafts before format 1 |
| Too many candidates | High and unpredictable CPU cost | Strict budgets and pruning |
| Type model mirrors one database | Poor portability | Logical types plus connector mappings |
| Plugins weaken reproducibility | Non-deterministic containers | Capability and determinism declarations |
| Metadata leaks sensitive structure | Archive information disclosure | Document exposure; pair with encryption |
| Checksums are mistaken for security | False authenticity assumptions | State that checksums are not signatures |
| Benchmarks overfit synthetic data | Misleading policy defaults | Multiple families and public methodology |

## 12. Open Decisions

- Exact container checksum algorithms and whether a logical content checksum is
  mandatory.
- Initial decimal precision and timestamp timezone semantics.
- Whether nested list and struct types enter format 1 or a later feature set.
- Stable codec plugin distribution and trust model.
- Minimum supported compiler versions.
- Criteria for graduating the C ABI from experimental to stable.

Open decisions must be resolved through a versioned design decision before
format 1 or API 1 is declared stable.
