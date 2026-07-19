# Glossary

## Adaptive policy

A bounded decision procedure that evaluates compatible encoding and codec
candidates and selects one according to a declared goal and hard resource
limits.

## Block

The independently encoded, compressed, and integrity-protected unit in a
Shibori container. A block contains the same row range across its columns.

## Candidate

A complete reversible transformation considered for a field or block,
including all serialized parameters and metadata costs.

## Codec

A general byte-level compression algorithm such as Zstandard or LZ4. A codec
receives the output of zero or more type-aware encodings.

## Column

The values for one schema field within a block, including its null information.

## Container

The portable Shibori representation containing a preamble, schema, blocks,
integrity information, optional index, and final summary.

## Decoded bytes

The canonical byte size of logical values after all Shibori encodings and
codecs have been reversed. It is not necessarily the source database's physical
storage size.

## Deterministic output

The property that a fixed engine version, configuration, schema, and ordered
input produce identical container bytes. This does not promise identical bytes
across engine versions unless a compatibility mode says so.

## Encoding

A reversible, type-aware transformation such as delta encoding, bit packing, or
dictionary encoding. Encodings expose patterns before an optional byte codec.

## Encoding chain

The ordered sequence of reversible encodings applied before a codec. The
container records the chain and all parameters needed to reverse it.

## Feature flag

A container declaration that changes interpretation or required reader
capability. Unknown mandatory features cause rejection; explicitly designated
optional metadata may be skipped.

## Field

A schema element with a stable identifier, name, logical type, nullability, and
type parameters.

## Finalized container

A container whose writer completed all required blocks and terminal metadata.
Only finalized containers may claim a complete block index and final totals.

## Format version

The version governing the serialized container contract. It is separate from
the engine library and API versions.

## Logical checksum

An optional checksum over canonical logical values. It can verify equivalent
content across different valid encoding choices but costs additional work.

## Logical type

A portable semantic type defined by Shibori, independent of a database's
physical page or driver representation.

## Mandatory feature

A feature that a reader must understand to interpret the container correctly.
Readers reject a container when such a feature is unknown or unavailable.

## Metadata overhead

All serialized bytes needed in addition to transformed values, including
dictionaries, parameters, indexes, checksums, and alignment.

## Physical type

The canonical in-memory or serialized representation used to carry a logical
type, such as a signed 64-bit integer for a timestamp count.

## Plugin

A separately supplied implementation of a registered encoding or codec
contract. A plugin is not a database connector.

## Policy

The caller-visible goal and limits that constrain adaptive selection. Initial
goals are `balanced`, `maximum-ratio`, and `fast-decode`.

## Raw storage

The required fallback that serializes canonical values without a compression
codec. Type framing and integrity metadata still apply.

## Schema

The ordered collection of fields that describes every block in a container or
schema segment.

## Seekable mode

A container mode whose sink and source permit random offsets. It may support a
terminal block index and selective block reads.

## Streaming mode

A mode that can write to or read from a non-seekable stream without buffering
the complete container. Metadata needed to decode a block precedes that block.

## Type-aware compression

Compression that uses a value's declared logical type and validated statistics
to choose reversible transformations.

## Verification

Structural, compatibility, bounds, and integrity checks performed without
trusting container metadata. Verification is not cryptographic authentication.

## Writer acknowledgement

Confirmation that a complete block's bytes were accepted by the configured
sink. It does not guarantee durable storage unless the embedding application
also requests and confirms durability from that sink.
