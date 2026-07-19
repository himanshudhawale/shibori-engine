# Integrity Value Types

Shibori represents serialized integrity results independently from the
algorithms that produce them.

`Crc32c` stores the 32-bit Castagnoli checksum value and converts explicitly to
and from the format's four little-endian bytes. Callers never serialize its
native in-memory representation.

`Blake3Digest` stores exactly 32 bytes. Its text form is 64 lowercase hexadecimal
characters; parsing also accepts uppercase input and normalizes output to
lowercase. Invalid lengths and characters return `invalid_digest_text`.

These types do not yet calculate checksums or digests. Portable CRC32C and
BLAKE3 hashers are separate child issues under integrity epic #14.

## Portable CRC32C

`Crc32cHasher` implements the reflected Castagnoli polynomial `0x82f63b78`,
with an initial and final XOR of `0xffffffff`. It accepts any number of byte
fragments, returns the same value as one contiguous update, and can be reset for
reuse. The implementation is portable and independent of host byte order.

Runtime hardware acceleration remains a separate child issue so the portable
implementation stays available as the reference and unconditional fallback.

On supported x86 hosts, automatic mode detects SSE4.2 CRC32C instructions at
runtime and uses them without changing the checksum. Callers can select portable
mode explicitly for reproducibility checks. Unsupported CPUs and architectures
always use the scalar implementation; executing an unsupported instruction is
never part of feature detection.

## BLAKE3 implementation dependency

The engine pins the official BLAKE3 C implementation at version 1.8.5 and
commit `93a431c78a52d7ccf0f366f106467f5070e6075e`. CMake verifies the source
archive SHA-256 before building its portable C files as a private object target.

The dependency is dual-licensed under Apache-2.0 or CC0-1.0. License texts are
installed with Shibori, and the exact source and checksum are recorded in
`THIRD_PARTY_NOTICES.md`. No BLAKE3 header or native target type appears in the
installed Shibori API.
