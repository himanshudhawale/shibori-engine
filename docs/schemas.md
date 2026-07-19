# Immutable Schemas

Schemas are constructed in two validated stages. `FieldBuilder` copies metadata
and produces an immutable `Field`; `SchemaBuilder` then validates field
identity, top-level name uniqueness, aggregate limits, and metadata before
producing an immutable, thread-shareable `Schema`.

Field IDs are nonzero. Field names and metadata keys are strict UTF-8.
Metadata keys are qualified with a namespace and slash; only
`shibori.engine/` may use the reserved `shibori.*` namespace. The configured
metadata byte limit includes field names, keys, and values across the schema.

Canonical schema bytes use fixed little-endian framing, ordered fields, sorted
metadata keys, logical type parameter bytes, and explicit nullability. They do
not depend on locale, map insertion order, native padding, or caller buffer
lifetime. The 128-bit fingerprint is deterministically derived from those
bytes. Both canonical bytes and the fingerprint are schema identities, not
authentication mechanisms.
