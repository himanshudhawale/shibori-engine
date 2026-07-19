# Shibori Container Format

**Status:** pre-implementation draft  
**Proposed format version:** 1.0

## 1. Purpose

The Shibori Container Format stores typed database data in independently
verifiable compressed blocks. It supports sequential pipes, finalized indexed
files, deterministic output, capability negotiation, and bounded parsing.

This draft is normative where it uses **MUST**, **MUST NOT**, **SHOULD**, or
**MAY**. No version is stable until golden fixtures and two independent parser
paths validate every emitted byte.

## 2. Design Requirements

The format must:

- decode without the source database;
- preserve schemas and logical values exactly;
- permit block-by-block streaming;
- reject unsupported mandatory features before value interpretation;
- place limits around every attacker-controlled length and count;
- support type-aware encoding chains and general byte codecs;
- detect accidental corruption in metadata and payloads;
- permit new optional records and metadata without redefining format 1;
- avoid host ABI, alignment, locale, and byte-order dependencies.

The format does not provide encryption, signatures, transactional updates, or
cross-container deduplication.

## 3. Conventions

### 3.1 Byte order

All fixed-width integers use little-endian byte order. Floating-point values use
their IEEE 754 bit patterns serialized as little-endian unsigned integers.

### 3.2 Integer notation

`u8`, `u16`, `u32`, and `u64` are unsigned integers of the stated bit width.
`i8`, `i16`, `i32`, and `i64` are two's-complement signed integers.

`varuint` is an unsigned LEB128 value limited to ten bytes and the range of
`u64`. Non-minimal encodings are invalid. A parser MUST detect overflow before
shifting or accumulating.

`varbytes` is `varuint byte_length` followed by exactly that many bytes.

### 3.3 Strings

Format-level strings are strict UTF-8 carried as `varbytes`. They are not
NUL-terminated and are not normalized. Value payloads follow their logical type
rules and may be binary.

### 3.4 Checksums

CRC32C uses the Castagnoli polynomial and the conventional initial and final
XOR values. Checksums are serialized as `u32`.

BLAKE3-256 digests are 32 raw bytes. They are used only where a record declares
them. Neither checksum authenticates an attacker-controlled container.

### 3.5 Alignment

Records and payloads have no implicit alignment. Any padding MUST be declared by
a record-specific field, MUST contain zero bytes, and is included in checksums.

## 4. File Layout

```text
+--------------------------+
| 16-byte file preamble    |
+--------------------------+
| FILE_HEADER record       |
+--------------------------+
| SCHEMA record            |
+--------------------------+
| DATA_BLOCK record 0      |
+--------------------------+
| DATA_BLOCK record 1      |
+--------------------------+
| ...                      |
+--------------------------+
| BLOCK_INDEX record?      |
+--------------------------+
| FILE_FOOTER record?      |
+--------------------------+
| 24-byte locator?         |
+--------------------------+
```

A streaming container requires the preamble, `FILE_HEADER`, at least one
`SCHEMA`, and zero or more `DATA_BLOCK` records. It MAY end without an index,
footer, or locator, in which case it is an incomplete or open stream and cannot
claim finalized totals.

An indexed finalized container requires one `FILE_FOOTER` and a terminal
locator. A footer declares whether a block index is present.

## 5. File Preamble

The preamble is exactly 16 bytes:

| Offset | Size | Field | Value |
| ---: | ---: | --- | --- |
| 0 | 8 | magic | ASCII `SHIBORI` followed by `0x00` |
| 8 | 2 | major | `1` |
| 10 | 2 | minor | `0` |
| 12 | 4 | preamble CRC32C | CRC32C of bytes 0 through 11 |

A reader MUST compare all eight magic bytes. Unknown major versions are
unsupported. A reader MAY accept a greater minor version only when every
mandatory feature and record is understood.

## 6. Record Envelope

Every record begins with a 32-byte fixed envelope:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | sync word: ASCII `SHR1` |
| 4 | 1 | record type |
| 5 | 1 | record flags |
| 6 | 2 | extension length |
| 8 | 8 | payload length |
| 16 | 8 | sequence number |
| 24 | 4 | envelope CRC32C |
| 28 | 4 | payload CRC32C |

The envelope CRC covers bytes 0 through 23. The payload checksum covers the
extension bytes followed by payload bytes. The extension immediately follows
the envelope, followed by the payload.

The reader MUST:

1. validate the sync word;
2. read fixed fields without allocating;
3. check extension and payload lengths against format and configured limits;
4. validate the envelope CRC;
5. acquire the bounded extension and payload;
6. validate the payload CRC before interpreting record contents.

Sequence numbers begin at zero for `FILE_HEADER` and increase by one for every
record. A mismatch is corruption.

Record flag bit 0 means **mandatory**. An unknown mandatory record causes an
unsupported-feature error. Unknown optional records MAY be skipped after
envelope and payload checksum validation.

Other record flag bits are reserved and MUST be zero in format 1.

## 7. Record Types

| ID | Name | Mandatory |
| ---: | --- | --- |
| `0x01` | `FILE_HEADER` | yes |
| `0x02` | `SCHEMA` | yes |
| `0x03` | `DATA_BLOCK` | yes |
| `0x04` | `BLOCK_INDEX` | no |
| `0x05` | `FILE_FOOTER` | finalized files |
| `0x06` | `USER_METADATA` | no |
| `0x07`-`0x7f` | reserved by Shibori | unspecified |
| `0x80`-`0xff` | private experimental | never stable |

Private experimental records MUST NOT appear in a container claiming stable
format 1 compatibility.

## 8. FILE_HEADER Record

The header payload is:

```text
u32 required_feature_count
repeated required_feature_count:
    u32 feature_id
    u16 minimum_major
    u16 minimum_minor

u32 optional_feature_count
repeated optional_feature_count:
    u32 feature_id
    u16 minimum_major
    u16 minimum_minor

u64 writer_flags
u64 created_unix_nanoseconds
16 bytes container_id
varbytes writer_name
varbytes writer_version
varbytes application_metadata
```

`created_unix_nanoseconds` MAY be zero when deterministic output is requested.
`container_id` MAY be all zero for deterministic output. Application metadata
is opaque and limited by the metadata budget.

Unknown required feature IDs cause rejection. Optional feature declarations are
informational and cannot change interpretation of mandatory records.

## 9. SCHEMA Record

The schema record extension is:

| Field | Type |
| --- | --- |
| schema segment ID | `u32` |
| first block ID | `u64` |
| schema fingerprint | 16 bytes |

The payload is:

```text
u32 field_count
repeated field_count:
    u32 field_id
    varbytes field_name
    u16 logical_type_id
    u16 field_flags
    varbytes type_parameters
    varbytes field_metadata
varbytes schema_metadata
```

Field flag bit 0 is `nullable`; all other bits are reserved in format 1.

The schema fingerprint is the first 16 bytes of BLAKE3-256 over a canonical
schema encoding defined by a future appendix before format 1 freezes. It
includes field order, IDs, names, types, nullability, and interpretation-
relevant parameters, but excludes cosmetic metadata.

Schema segment IDs begin at zero and increase by one. The first schema has
`first_block_id = 0`. A schema applies until the next schema record.

### 9.1 Logical type IDs

| ID | Type | Parameter payload |
| ---: | --- | --- |
| 1 | bool | empty |
| 2 | int8 | empty |
| 3 | int16 | empty |
| 4 | int32 | empty |
| 5 | int64 | empty |
| 6 | uint8 | empty |
| 7 | uint16 | empty |
| 8 | uint32 | empty |
| 9 | uint64 | empty |
| 10 | float32 | empty |
| 11 | float64 | empty |
| 12 | decimal | `u16 precision`, `i16 scale`, `u16 bit_width` |
| 13 | date | empty |
| 14 | time | `u8 unit` |
| 15 | timestamp | `u8 unit`, `u8 timezone_mode`, optional zone label |
| 16 | duration | `u8 unit` |
| 17 | string | `u8 validation_mode` |
| 18 | binary | empty |
| 19 | fixed_binary | `u32 byte_width` |
| 20 | uuid | empty |

Time units are 0 second, 1 millisecond, 2 microsecond, and 3 nanosecond.
Timezone modes are 0 local and 1 instant.

Unknown type IDs are unsupported unless a declared mandatory feature defines
them.

## 10. DATA_BLOCK Record

The block extension is:

| Field | Type |
| --- | --- |
| block ID | `u64` |
| schema segment ID | `u32` |
| row count | `u64` |
| column count | `u32` |
| decoded logical bytes | `u64` |
| block flags | `u32` |

Block IDs begin at zero and increase by one. Column count MUST equal the active
schema's field count. Block flag bits are reserved and zero in format 1.

The payload begins with a column directory followed by chunk bytes:

```text
u32 directory_entry_count
repeated directory_entry_count:
    u32 field_id
    u64 chunk_offset
    u64 chunk_length
    u64 decoded_length
    u32 chunk_crc32c
    u16 encoding_count
    repeated encoding_count:
        u32 encoding_id
        varbytes encoding_parameters
    u32 codec_id
    varbytes codec_parameters
    u32 chunk_flags

byte[] chunk_region
```

Chunk offsets are relative to the first byte of `chunk_region`. Directory
entries MUST be in schema field order. Ranges MUST be non-overlapping,
monotonically ordered, and completely within the chunk region. Gaps are invalid
in format 1.

`chunk_crc32c` covers compressed chunk bytes. The enclosing record checksum also
covers the complete directory and chunk region. Chunk checksums permit
independent verification and indexed field reads.

Chunk flag bit 0 indicates a validity stream is included in the encoded input.
Other bits are reserved.

### 10.1 Encoding order

Encoding IDs are listed in application order. A reader applies the byte codec
first in reverse, then reverses encodings from last to first.

For example:

```text
canonical integers
  -> delta encoding (ID 2)
  -> bit packing (ID 3)
  -> Zstandard codec (ID 1)
```

Readers MUST validate that the chain is registered for the field's logical type
and that each stage's output physical type matches the next stage's input.

### 10.2 Built-in encoding IDs

| ID | Encoding |
| ---: | --- |
| 0 | canonical raw |
| 1 | validity bitmap |
| 2 | integer delta |
| 3 | unsigned bit packing |
| 4 | dictionary indices and dictionary |
| 5 | run-length |
| 6 | delta-of-delta |
| 7 | floating-point XOR |

An empty encoding list means canonical raw input to the codec. Encoding ID 0
MUST NOT appear in a non-empty chain.

### 10.3 Built-in codec IDs

| ID | Codec |
| ---: | --- |
| 0 | none |
| 1 | Zstandard |
| 2 | LZ4 block |

Codec parameters are canonical Shibori parameter bytes, not native library
structures. Exact parameter schemas must be frozen with golden fixtures before
implementation claims format 1.

## 11. Canonical Raw Chunks

A canonical raw chunk begins with:

```text
u8 layout_version       // 0
u8 layout_kind
u16 reserved            // zero
u64 row_count
u64 non_null_count
```

Layout kinds:

- 0: fixed-width;
- 1: boolean bitmap;
- 2: variable-width;

Nullable columns next contain `ceil(row_count / 8)` validity bytes. Padding bits
MUST be zero.

Fixed-width layouts then contain exactly `non_null_count * width` bytes.
Boolean layouts contain `ceil(non_null_count / 8)` value bytes with zero
padding. Variable-width layouts contain `non_null_count + 1` `u64` offsets
followed by payload bytes.

Offsets use little-endian fixed `u64`, begin at zero, are monotonic, and end at
payload length. Encodings may replace this raw layout, but decoding always
reconstructs its logical equivalent.

## 12. BLOCK_INDEX Record

The index extension contains:

| Field | Type |
| --- | --- |
| indexed block count | `u64` |
| first block ID | `u64` |

The payload contains fixed entries:

```text
repeated indexed block count:
    u64 block_id
    u32 schema_segment_id
    u64 record_offset
    u64 record_length
    u64 row_count
    u64 decoded_logical_bytes
```

Entries are ordered by block ID. Record offsets point to the `S` in the
`DATA_BLOCK` sync word. An index may cover all blocks only in format 1. Partial
indexes require a future feature.

Readers treat the index as untrusted optimization data. They validate the target
record envelope, sequence, block ID, and checksum before returning values.

## 13. FILE_FOOTER Record

The footer extension is empty. Its payload is:

```text
u64 record_count
u64 schema_segment_count
u64 block_count
u64 row_count
u64 decoded_logical_bytes
u64 encoded_payload_bytes
u64 block_index_record_offset   // zero when absent
u64 block_index_record_length   // zero when absent
u64 footer_flags
32 bytes records_digest
32 bytes logical_digest         // all zero when absent
```

Footer flag bit 0 means `records_digest` is present and MUST be verified when a
full-file verification is requested. Bit 1 means `logical_digest` is present.
Other bits are reserved.

`records_digest` is BLAKE3-256 over every byte from the file preamble through
the record preceding `FILE_FOOTER`. It excludes the footer and locator.

`logical_digest` is BLAKE3-256 over the canonical schema and canonical decoded
blocks in order. Its exact domain separation and framing must be defined before
format 1 freezes. It permits logical equivalence checks across different valid
compression choices.

## 14. Terminal Locator

A finalized seekable container ends with 24 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 8 | ASCII `SHIBEND` followed by `0x00` |
| 8 | 8 | footer record offset |
| 16 | 4 | locator CRC32C over bytes 0 through 15 |
| 20 | 4 | reserved zero |

The locator is not a record and is not included in the records digest. A reader
MUST validate that the footer offset points within the file and that the target
is a valid `FILE_FOOTER`.

Absence of the locator does not make sequential records invalid, but the
container is not finalized.

## 15. USER_METADATA Record

User metadata is optional and does not alter value interpretation.

The extension contains a strict UTF-8 namespace as `varbytes`. The payload is
opaque. Namespaces beginning with `shibori.` are reserved.

Readers MAY skip metadata after checksum validation. Writers SHOULD avoid source
names, SQL text, credentials, personal data, or secrets unless the embedding
application explicitly requests them.

## 16. Streaming Rules

A streaming writer:

- writes complete records once and never seeks backward;
- emits a schema before blocks using it;
- keeps block metadata before block chunks;
- may omit the block index, footer, and locator;
- cannot claim final totals until a valid footer is emitted.

A streaming reader can return a verified block after its complete
`DATA_BLOCK` record is read. It does not wait for a footer. Applications that
require proof of complete transfer MUST also require a valid footer and compare
declared totals.

## 17. Deterministic Mode

For a fixed format version, engine version, schema, ordered blocks, policy, and
codec implementations, deterministic mode requires:

- zero creation time and container ID;
- canonical metadata key ordering;
- stable field and directory ordering;
- fixed worker-independent publication order;
- deterministic codec settings;
- zero padding;
- no unregistered application metadata;
- canonical parameter serialization.

Format compatibility does not imply deterministic identity across engine or
codec versions.

## 18. Reader Validation Order

A conforming reader validates:

1. configured minimum limits are internally consistent;
2. file preamble magic and CRC;
3. record envelope sync, bounded lengths, sequence, and envelope CRC;
4. complete extension and payload acquisition;
5. record payload CRC;
6. record-specific counts, offsets, flags, and reserved fields;
7. required features and implementation capabilities;
8. schema and type constraints;
9. block directory bounds and non-overlap;
10. chunk checksum;
11. codec output bounds and exact expected length;
12. reverse encoding invariants;
13. reconstructed column and block invariants;
14. optional footer totals and digests.

A failure at any step prevents the affected block from being exposed.

## 19. Resource Limits

The format permits large `u64` lengths, but implementations MUST apply lower
configured limits before allocation. At minimum:

- extension bytes;
- record payload bytes;
- schema fields and metadata;
- rows per block;
- columns per block;
- encoded and decoded chunk bytes;
- dictionary entries and bytes;
- encoding chain length;
- records and blocks;
- total decoded bytes.

All addition, multiplication, offset, and range calculations use checked
arithmetic. A valid range satisfies `offset <= size` and
`length <= size - offset`.

## 20. Compatibility

Major versions may change framing or interpretation. Minor versions may add:

- optional records;
- optional metadata;
- new registered type, encoding, or codec IDs when declared as required
  features;
- stricter writer recommendations that do not invalidate prior files.

A minor version MUST NOT reinterpret an existing ID or field. Reserved bits
becoming meaningful require a feature declaration when old readers would
otherwise misinterpret data.

Writers SHOULD emit the lowest minor version that represents all used features.

## 21. Recovery and Corruption

The sync word and sequence number can assist diagnostic scanning, but format 1
does not define automatic recovery after a corrupt mandatory record. Standard
readers stop at the first failure.

Tools MAY report later candidate sync words as forensic information. They MUST
NOT present recovered values as a verified container without a separate,
explicit recovery status.

## 22. Security Considerations

- Lengths and offsets are attacker-controlled.
- Compression ratios can cause extreme decoded sizes.
- Dictionaries and encoding parameters can consume disproportionate memory.
- Native codec libraries may have their own vulnerabilities and limits.
- Metadata and schemas remain visible without external encryption.
- CRC32C and BLAKE3 digests without a secret do not prove authorship.
- Plugin IDs do not establish that plugin code is trustworthy.

Readers must enforce resource budgets during parsing and codec execution, not
only after decompression.

## 23. Format Freeze Checklist

Before declaring version 1.0 stable:

1. Assign all feature, type, encoding, and codec IDs in a registry.
2. Define canonical schema fingerprint bytes.
3. Define exact encoding and codec parameter schemas.
4. Define logical digest framing and domain separation.
5. Publish valid golden files for every type and built-in chain.
6. Publish malformed fixtures for every validation class.
7. Decode fixtures on all supported architectures.
8. Validate streaming and indexed files with independent parser paths.
9. Run parser fuzzing under sanitizers.
10. Document maximum format values and supported implementation defaults.
