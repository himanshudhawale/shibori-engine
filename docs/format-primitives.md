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
