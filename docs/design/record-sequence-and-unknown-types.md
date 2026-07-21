# Record Sequence and Unknown-Type Handling

## Problem

Valid framing alone does not establish that records are in order or that an
unknown record can be ignored safely. Skipping unchecked bytes could hide
corruption, while accepting an unknown mandatory record could change container
meaning.

## Design

`verify_record` combines bounded envelope parsing with exact extension and
payload length checks and CRC32C verification. Only this operation creates a
`VerifiedRecord`.

`RecordSequenceTracker` accepts verified records in order, beginning with
sequence zero. Known record types are returned for processing. Unknown optional
types are returned as skippable, while unknown mandatory types fail with
`unsupported_feature` and carry the numeric type as the component ID.

The expected sequence advances only after a record receives a successful
disposition. A mismatch is `invalid_record` corruption.

## Alternatives

A boolean `payload_verified` parameter would allow callers to assert safety
without proof. Letting each record-specific parser track sequence would
duplicate state and make unknown optional records impossible to handle
consistently.

## Edge Cases

Payload CRC covers extension bytes followed by payload bytes, including either
empty region. Supplied region sizes must match the envelope exactly. Unknown
record values remain representable even though only IDs `0x01` through `0x06`
are currently known.

## Validation

Tests cover known processing, verified optional skipping, mandatory rejection
with type context, sequence mismatch, and rejection of a corrupt optional
payload before it can reach disposition handling.
