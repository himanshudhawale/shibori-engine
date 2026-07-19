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
