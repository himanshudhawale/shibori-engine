# Data Model

## 1. Design Goals

The model must carry enough semantics for safe type-aware compression while
remaining portable across databases and languages. It describes logical values,
not source pages, SQL syntax, ORM objects, or driver-specific buffers.

The model separates:

- **logical type:** meaning visible to connectors and SDK users;
- **physical representation:** canonical values consumed by the engine;
- **encoding:** reversible transformation selected for a block;
- **serialization:** bytes defined by the container format.

## 2. Schema

A schema is an ordered immutable list of fields plus optional namespaced
metadata. Its identity is a canonical fingerprint over interpretation-relevant
properties.

```text
Schema
  fields: Field[]
  metadata: Map<QualifiedName, ByteString>
  fingerprint: 128-bit value
```

Metadata is non-semantic unless its namespace is registered as a mandatory
feature. Cosmetic metadata does not change value interpretation.

### Field

```text
Field
  id: unsigned 32-bit integer
  name: UTF-8 string
  type: LogicalType
  nullable: boolean
  metadata: Map<QualifiedName, ByteString>
```

Field IDs are unique within a schema and remain stable across compatible schema
revisions. Names are descriptive and need not be unique across nested scopes;
top-level names are required to be unique in format 1 for predictable mapping.

## 3. Initial Logical Types

| Type | Parameters | Canonical physical form |
| --- | --- | --- |
| `bool` | none | one logical bit per non-null value |
| `int8/16/32/64` | signed width | two's-complement integer |
| `uint8/16/32/64` | unsigned width | unsigned integer |
| `float32/64` | IEEE width | IEEE 754 bit pattern |
| `decimal` | precision, scale | signed fixed-width integer |
| `date` | unit fixed to day | signed days from Unix epoch |
| `time` | unit | signed units from midnight |
| `timestamp` | unit, timezone mode | signed units from Unix epoch |
| `duration` | unit | signed unit count |
| `string` | UTF-8 validation mode | offsets plus UTF-8 bytes |
| `binary` | none | offsets plus uninterpreted bytes |
| `fixed_binary` | byte width | contiguous fixed-width bytes |
| `uuid` | byte order fixed by format | 16 canonical bytes |

Nested list, map, struct, union, and arbitrary precision types are deferred
until their canonical semantics and resource limits are specified.

## 4. Nullability

Null is distinct from every non-null value, including zero, empty strings, NaN,
and empty binary values.

Nullable columns carry a validity bitmap with one bit per row. A set bit means
valid. Padding bits are zero and ignored by readers after validation.

Non-nullable columns omit the validity bitmap and reject null input. Encodings
operate on the dense sequence of non-null values unless their contract
explicitly includes null positions.

This separation prevents sentinel values from changing the logical domain.

## 5. Fixed-Width Columns

A fixed-width column contains:

```text
FixedColumn
  row_count
  validity?          // row_count bits
  values             // canonical values for non-null rows
```

The public API may accept strided or borrowed input, but the logical model is
the same. Canonical serialization uses the format byte order, not host object
layout.

Boolean values are logically fixed-width but may use a bit vector directly.

## 6. Variable-Width Columns

Strings and binary values contain:

```text
VariableColumn
  row_count
  validity?          // row_count bits
  offsets            // non-null value count + 1
  payload             // concatenated value bytes
```

Offsets begin at zero, are monotonically non-decreasing, and end at payload
length. Empty values repeat the previous offset. Null rows do not consume a
payload entry in the dense physical representation.

The engine validates offsets before reading payload ranges. Format 1 limits a
single value and block payload through configurable reader limits even if the
serialized offset width can represent more.

## 7. Numeric Semantics

### Integers

Signed integers use mathematical values within their declared width. Encodings
must use checked arithmetic or unsigned modular operations whose reversal is
defined for every bit pattern.

### Floating point

Every IEEE bit pattern is a valid logical value, including signed zero,
subnormal values, infinities, and NaNs with payloads. Lossless round trips
preserve the exact bit pattern, not merely numeric equality.

Statistics may classify exceptional values, but policy selection cannot
canonicalize NaNs or signed zero.

### Decimal

A decimal value is:

```text
unscaled_integer * 10^(-scale)
```

Precision is the maximum decimal digits in the unscaled integer. Scale may be
positive, zero, or negative within format limits. Connectors must explicitly
handle source values whose precision exceeds supported widths.

The initial implementation should support decimal widths only after exact
maximum precision and serialization are fixed in the container specification.

## 8. Temporal Semantics

Time units are one of second, millisecond, microsecond, or nanosecond.

- `date` is a signed day count from `1970-01-01`.
- `time` is an elapsed unit count from local midnight and must be within one
  day for its unit.
- `duration` is a signed elapsed unit count without calendar semantics.
- `timestamp` is a signed unit count from the Unix epoch.

Timestamp timezone modes:

- **instant:** the count identifies a UTC instant; an optional zone label is
  descriptive metadata;
- **local:** the count represents a wall-clock value without an offset.

The two modes are not interchangeable. Connectors own database-specific
timezone conversion and daylight-saving rules.

## 9. Text Semantics

Format 1 string values are byte sequences intended to be UTF-8. Validation mode
is one of:

- `strict`: every non-null value must be valid UTF-8;
- `trusted`: the connector asserts validity and the engine may validate
  according to configured safety policy.

Invalid source text that must be preserved exactly should be mapped to `binary`,
not silently repaired. Unicode normalization is never implicit.

Field names and registered metadata keys are always strict UTF-8.

## 10. Blocks

A block is:

```text
Block
  id: unsigned 64-bit integer
  row_count: unsigned 64-bit integer
  schema_fingerprint
  columns: Column[field_count]
```

Block IDs increase monotonically within a container segment. All columns have
the same logical row count. Empty blocks are legal only where the writer API
explicitly allows them; they are normally omitted.

The default target block size is a policy input rather than a format guarantee.
Hard row and byte limits are enforced independently.

## 11. Canonical Value Bytes

Canonical bytes provide:

- raw fallback input;
- logical checksum input;
- deterministic comparison fixtures;
- a stable boundary between the data model and encoding stages.

Rules include:

1. fixed-width numbers use the format byte order;
2. floating-point values use their exact IEEE bits;
3. validity precedes dense values;
4. variable values use canonical offsets and unmodified payload bytes;
5. type parameters come from the schema, not repeated per value;
6. no compiler padding or native object representation is serialized.

The container specification defines exact widths and framing.

## 12. Schema Evolution

Format 1 containers use immutable schema segments. A new schema begins a new
segment and receives a new fingerprint.

Compatibility classifications:

- **identical:** same interpretation-relevant schema;
- **projectable:** a reader can select common stable field IDs;
- **convertible:** a connector or SDK may perform an explicit checked
  conversion;
- **incompatible:** values cannot be interpreted without loss or ambiguity.

The engine does not automatically widen, narrow, rename, or reinterpret fields.
It reports schemas and supports projection only through explicit APIs.

## 13. Metadata

Metadata keys use qualified namespaces such as:

```text
shibori.engine/example
org.example.application/source-id
```

Limits apply to key count, key bytes, value bytes, and aggregate metadata.
Reserved `shibori.*` namespaces require specification ownership.

Metadata may reveal schema and source information. It is integrity-protected
but not encrypted by the engine.

## 14. Statistics Model

Statistics are scoped to a column chunk and divided into:

- **observed:** calculated from the complete block;
- **sampled:** estimated from a declared sample;
- **derived:** calculated from candidate output;
- **serialized:** retained for inspection or future reads.

Every statistic records its kind and scope. The planner cannot treat a sampled
statistic as proof of an encoding precondition.

Potentially sensitive values such as exact minima, maxima, or dictionary entries
are not serialized unless the format feature explicitly requires them.

## 15. Limits

Readers and writers enforce limits for:

- fields per schema;
- schema and metadata bytes;
- rows per block;
- canonical and decoded bytes per block and column;
- variable-width value length;
- dictionary entries and bytes;
- encoding chain depth;
- blocks per container;
- total logical bytes where known.

Format maxima prevent ambiguity; configured maxima may be lower. Exceeding
either is a typed resource or format-limit error.

## 16. Data Model Invariants

1. Field identity is stable and independent of position.
2. A logical value has one unambiguous canonical representation.
3. Null never aliases a non-null value.
4. Floating-point bit patterns round-trip exactly.
5. Variable-width offsets are validated before payload access.
6. Source database semantics enter only through explicit logical types and
   metadata.
7. Sampled statistics never establish correctness.
8. Schema conversion is explicit and outside basic decoding.
9. Every block is independently bounded.
10. Unsupported values fail rather than being silently coerced.
