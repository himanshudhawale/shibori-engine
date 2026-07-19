# Bounded Byte I/O

`ByteSource` and `ByteSink` are the common contracts for memory, files, pipes,
and future callbacks. Capabilities explicitly advertise position, size, seek,
and flush support. `require_capabilities` lets an operation reject unsupported
behavior before it reads, writes, or seeks.

Reads report both progress and EOF. Short reads and writes are normal;
`read_exact` and `write_all` retry them to completion. Zero progress without
EOF is an `io_no_progress` error, early EOF is `unexpected_end`, and an adapter
claiming more progress than requested violates the adapter contract.

Every operation accepts a shareable cancellation token and checks it before
mutation. Memory adapters own a copy or retain explicitly immutable shared
input, expose deterministic position and size, and enforce configured output
bounds. Flush means adapter completion only and never claims durable storage.

File adapters use the same contracts, report position/size/seek capabilities,
enforce per-operation and total-size bounds, and map open, read, write, seek,
and flush failures to typed I/O errors. Positioned helpers check seek support
before changing adapter position or destination bytes.

Tests use deterministic scripted adapters for exact short-operation schedules,
zero-progress responses, unsupported seek behavior, and injected read/write
failures. This makes retry and failure semantics reproducible without depending
on operating-system buffering.
