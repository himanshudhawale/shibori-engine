# Format Primitives

Container integers are encoded explicitly in little-endian order. Internal
helpers support unsigned 16-, 32-, and 64-bit values and never serialize native
object representations.

Encoding returns fixed-size byte arrays. Decoding accepts a bounded byte span,
checks the complete width before reading any byte, and reports
`unexpected_end` for truncated input. Extra bytes remain untouched so callers
can decode fields from larger record buffers.

These helpers are internal format infrastructure rather than installed public
API. Preamble and record-envelope child issues build on them.

## File preamble encoding

The format 1.0 preamble encoder emits exactly 16 bytes:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 8 | `SHIBORI` followed by `0x00` |
| 8 | 2 | little-endian major version `1` |
| 10 | 2 | little-endian minor version `0` |
| 12 | 4 | little-endian CRC32C over bytes 0 through 11 |

The format 1.0 checksum is `0x9ec3bdeb`, serialized as
`eb bd c3 9e`. Encoding is deterministic and uses the portable CRC32C path so
its bytes do not depend on runtime acceleration availability.

Preamble parsing and validation remain a separate child issue.

## File preamble parsing

Parsing first requires all 16 bytes, then validates the eight-byte magic before
reading version fields. It verifies CRC32C before interpreting version support,
so a corrupted version cannot be misreported as merely unsupported.

Failures are distinct:

- `unexpected_end` for fewer than 16 bytes;
- `invalid_magic` for a non-Shibori file;
- `checksum_mismatch` for corrupted preamble bytes;
- `unsupported_format_version` for a valid but unsupported major version.

Format 1 readers accept a higher minor value at this layer. Required feature
negotiation determines whether the rest of that container is readable.
