# Typed Column Storage

Typed columns use immutable buffers and dense non-null values. `Validity`
stores one bit per row, validates its exact byte length and zero padding, and
records the null count. Boolean values use a dense bit vector. Fixed-width
values remain byte-exact, including every IEEE floating-point bit pattern.
String and binary columns retain 64-bit offsets and an unmodified payload.

`ByteBuffer` and `OffsetBuffer` make ownership explicit. `copy` creates
engine-owned immutable storage. `share` retains a
`shared_ptr<const vector<...>>` without copying; callers selecting this path
must publish storage as immutable before sharing it. Finished columns retain
their buffers independently of builders and temporary caller views.

The type-specific builders prevent variable-width types from entering
fixed-width storage and require all physical components. Full sizing, offset,
schema, and resource-limit validation occurs when an immutable block is
finished.
