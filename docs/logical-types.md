# Logical Types

`LogicalType` is the portable interpretation model used by schemas and blocks.
It contains no database-driver or codec types. Construction validates each
parameter before the value can enter a schema.

The model covers booleans; signed and unsigned integers; exact IEEE floating
point bit patterns; decimal values with precision 1–38 and scale within the
precision; date, time, timestamp, and duration values; UTF-8 strings; binary
values; fixed-width binary values; and canonical 16-byte UUIDs.

Time values use seconds, milliseconds, microseconds, or nanoseconds. Timestamps
also distinguish UTC instants from local wall-clock values. Strings explicitly
select strict or trusted UTF-8 validation. Fixed-binary widths must be nonzero.

Logical types are validated value objects. Copies preserve the complete type
and parameter set, and equality compares interpretation rather than object
identity.
