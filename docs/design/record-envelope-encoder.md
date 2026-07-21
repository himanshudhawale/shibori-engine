# Record Envelope Encoder

## Problem

Every Shibori record needs the same fixed framing before record-specific bytes
can be written. Encoding this framing separately in each future writer would
risk inconsistent byte order, checksum coverage, flag handling, and sequence
placement.

## Design

The internal `RecordEnvelope` model carries the record type, format 1 flags,
extension and payload lengths, sequence number, and the checksum of the
extension plus payload. `encode_record_envelope` emits the normative 32-byte
layout using explicit little-endian helpers.

The encoder calculates the envelope CRC32C over bytes 0 through 23 and places
the caller-provided payload CRC32C at bytes 28 through 31. It uses the portable
CRC32C implementation so identical inputs produce identical bytes regardless
of CPU capabilities.

Format 1 defines only flag bit 0, which marks a record as mandatory. The
encoder rejects any set bit in the `0xfe` reserved mask with `invalid_record`
rather than emitting a container that format 1 readers must reject.

## Alternatives

Accepting a precomputed envelope checksum would let callers create
self-inconsistent models. Computing the payload checksum in this function
would require owning extension and payload bytes, coupling fixed framing to
record buffering. Keeping payload checksum calculation with the record writer
supports both buffered and streaming implementations.

## Edge Cases

Zero lengths and the complete unsigned ranges are representable because the
wire format defines fixed-width fields. Length limits are reader and writer
policy concerns outside this byte-level encoder. Unknown numeric record types
remain representable through an explicit `RecordType` conversion for future
registered and experimental records.

## Validation

A golden test pins every byte for non-symmetric length values, verifies
deterministic encoding, checks the mandatory flag model, and confirms that a
reserved flag is rejected with the typed encoding error.
