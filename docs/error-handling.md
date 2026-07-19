# Error Handling

## Purpose

Shibori returns typed errors rather than using exceptions for expected parser,
I/O, validation, compatibility, resource, and cancellation failures. The model
preserves actionable context while keeping database values out of default
diagnostics.

## Categories and codes

`ErrorCategory` is the broad behavior class used by callers. `ErrorCode` is the
stable, specific reason. Numeric codes are allocated in category ranges and do
not reuse operating-system or third-party codec values:

| Range | Category |
| --- | --- |
| 1000-1999 | Invalid argument or lifecycle |
| 2000-2999 | Invalid typed input |
| 3000-3999 | I/O |
| 4000-4999 | Truncation |
| 5000-5999 | Corruption |
| 6000-6999 | Unsupported capability |
| 7000-7999 | Resource limit |
| 8000-8999 | Cancellation |
| 9000-9999 | Plugin |
| 10000+ | Internal invariant |

New codes may be added without changing the meaning of existing codes. Codes
are never reassigned to another category.

## Diagnostic context

An error records its operation and can carry:

- byte offset;
- block ID;
- field ID;
- encoding or codec component ID;
- a nested engine error.

The standard description contains identifiers and structural context only.
Callers must not place field values, credentials, dictionary entries, SQL text,
or other sensitive input in error messages.

## Result propagation

`Result<T>` uses the C++23 `std::expected<T, Error>` contract and therefore
supports move-only values. `Status` is `Result<void>`. The `fail` helper creates
a typed unexpected result without throwing.

Programming bugs may still terminate or use development assertions at internal
boundaries. Public operations convert recoverable conditions into `Result`.
Exceptions never cross the planned C ABI.

## Nested causes

Platform and codec adapters map native failures to stable Shibori codes. A
nested cause may retain additional Shibori context, but native numeric values
are not promoted into the stable `ErrorCode` namespace.
