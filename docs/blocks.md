# Bounded Immutable Blocks

`BlockBuilder` binds immutable columns to stable schema field IDs and validates
the complete block before publishing it. A completed `Block` owns or safely
shares its schema and column buffers, so builders and temporary caller inputs
may be destroyed immediately.

Validation requires one column per field, matching logical types and row
counts, omitted validity for non-nullable fields, exact dense fixed-width and
boolean storage, and zero boolean padding. Configured row, record, and decoded
byte limits are enforced with checked arithmetic.

Variable offsets must have `dense value count + 1` entries, begin at zero,
never decrease, and end exactly at the payload size. All offsets and value
lengths are validated before payload bytes are accessed. Strict strings are
then checked as separate UTF-8 values. Only a validated `Block` exposes
variable-value ranges.
