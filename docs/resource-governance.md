# Resource Governance

## Purpose

Shibori validates arithmetic and reserves bounded resources before parsing,
decoding, or allocating from untrusted size claims. Limits fail explicitly;
zero never means unlimited.

## Checked arithmetic

The core provides checked unsigned addition, multiplication, integral casts, and
bounded ranges. Failures return `arithmetic_overflow` or `range_exceeded`
without performing the unsafe operation.

Range validation uses:

```text
offset <= total_size
length <= total_size - offset
```

This avoids computing an overflowing `offset + length`.

## Default limits

Initial defaults bound resident memory, rows per block, decoded block bytes,
record bytes, metadata bytes, fields, encoding depth, workers, and queued
blocks. Applications may choose lower or higher positive values, but related
limits must remain internally consistent.

Defaults are safe starting points rather than format maxima or performance
guarantees. Readers always validate caller-provided limits before consuming
input.

## Hierarchical budgets

`ResourceBudget` represents one resource kind. A root context budget funds an
operation child; the operation funds block children; blocks may fund candidate
children:

```text
context
  -> operation
      -> block
          -> candidate
```

Creating a child reserves its complete capacity from the parent. This prevents
sibling scopes from collectively promising more than the parent owns. Destroying
the final child handle releases that parent capacity.

## Reservations

Work acquires a move-only `ResourceReservation` before consuming capacity. Its
destructor releases exactly once. Move operations transfer ownership, explicit
release is idempotent, and failed reservations leave usage unchanged.

RAII ensures normal return, typed failure, and cooperative cancellation paths
release reservations without separate cleanup logic.

## Concurrency

Budget accounting is synchronized and safe for independent worker tasks.
Reservations do not allocate after capacity has been charged. Allocation of a
new child state occurs before parent accounting changes, so allocation failure
cannot leak parent capacity.

## Failure behavior

- Invalid zero or inconsistent limits return `invalid_resource_limit`.
- Arithmetic overflow returns `arithmetic_overflow`.
- An invalid bounded region returns `range_exceeded`.
- Exhausted capacity returns `limit_exceeded`.
- Budget state allocation failure returns `allocation_failed`.

Diagnostics identify the resource kind and operation but do not include source
values.
