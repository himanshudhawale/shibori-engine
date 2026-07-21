# Bounded Record Envelope Parser

## Problem

Record lengths are attacker-controlled and appear before extension or payload
bytes. A reader needs to validate fixed framing, configured bounds, and total
length arithmetic without allocating or reading beyond the 32-byte envelope.

## Design

`parse_record_envelope` requires the complete fixed envelope before accessing
any field. It validates the `SHR1` sync, reserved flags, extension and payload
limits, checked total-length arithmetic, and envelope CRC32C. On success it
returns the same internal model accepted by the encoder.

Limits are explicit parser inputs. This keeps format decoding independent of a
particular reader configuration while ensuring lengths are rejected before a
future acquisition or allocation step.

## Alternatives

Returning partially decoded fields on failure would make it easier for callers
to accidentally trust unverified values. Deferring bounds to record-specific
parsers would allow oversized extension or payload requests before common
framing validation.

## Edge Cases

Every input shorter than 32 bytes is truncated. Reserved flags are rejected
before checksum validation because they are independently invalid format 1
framing. A valid `u64` payload length may still overflow when extension and
envelope bytes are added, so total record length uses checked arithmetic.

## Validation

Tests cover encoder/parser round-trip, every truncation boundary, invalid sync,
CRC mismatch, reserved flags, independent extension and payload limits, and
unsigned total-length overflow.
