# Reliability and Security

## 1. Reliability Objective

Shibori must either return verified logical values or a precise failure. It must
not silently omit blocks, reinterpret unsupported features, exceed declared
resource limits, or report an incomplete stream as finalized.

Durability of the destination, database consistency, encryption, and
authentication remain responsibilities of higher layers.

## 2. Trust Model

Untrusted:

- container bytes and metadata;
- schemas and type parameters read from containers;
- compressed and decoded length claims;
- encoding and codec parameter bytes;
- files, pipes, and network-backed sources.

Trusted in-process:

- embedding application;
- caller-provided source and sink implementations;
- explicitly loaded native plugins;
- linked codec libraries.

Trusted code can violate process safety. Plugin loading is therefore opt-in and
outside the parser's hostile-input boundary.

## 3. Primary Threats

| Threat | Control |
| --- | --- |
| oversized length or integer overflow | validate before allocation with checked arithmetic |
| decompression bomb | reserve declared decoded limit before codec execution |
| malicious dictionary | cap entries, bytes, offsets, and hash work |
| deep encoding chain | fixed format and configured depth limits |
| corrupt metadata selecting wrong decoder | checksum metadata and validate type transitions |
| truncated stream | exact framing and distinct truncation errors |
| crafted codec input | maintained codec versions, fuzzing, and process limits |
| checksum mistaken for authenticity | explicit documentation and external signatures |
| schema information disclosure | document visibility and recommend encryption |
| plugin ID confusion | registry collision rejection and capability validation |
| cancellation race | operation-owned state machine and RAII reservations |

## 4. Parsing Discipline

Parsers follow length-delimited, validate-then-use processing:

1. Read a fixed bounded prefix.
2. Decode integers with overflow checks.
3. Compare lengths with format and configured limits.
4. Validate checksums before interpreting payload fields.
5. Validate counts and ranges before indexing.
6. Reserve memory before allocation or codec execution.
7. Publish decoded data only after complete block verification.

No parser holds a pointer into a buffer across an operation that may reallocate
that buffer.

## 5. Resource Governance

Memory reservations cover input retention, metadata, decoded output, scratch
buffers, dictionaries, codec contexts where measurable, and queued publication.
Reservations are hierarchical:

```text
context limit
  -> operation reservation
      -> block reservation
          -> column and candidate reservations
```

A child cannot exceed its parent. Failed or cancelled work releases through
RAII. The engine also limits workers, queue depth, fields, rows, records,
metadata, candidate count, and analysis work.

Time limits are cooperative, not hard real-time isolation. Applications reading
hostile data should additionally use operating-system process limits where
appropriate.

## 6. Integrity

CRC32C detects accidental record and chunk corruption. Optional BLAKE3 digests
support full-file and logical-content verification.

Integrity checks do not provide:

- author identity;
- tamper resistance against an active attacker;
- confidentiality;
- rollback detection;
- trusted timestamps.

Applications needing those properties should sign and encrypt finalized
containers with a reviewed external format and key-management system.

## 7. Failure Atomicity

### Writing

A block is acknowledged only after its complete record is accepted by the sink.
An operation failure leaves prior complete streaming records readable but does
not produce a valid final footer.

The engine never modifies an existing destination path. Atomic temporary-file
rename belongs to the CLI or embedding application.

### Reading

A decoded block is returned only after all required chunks and invariants pass.
The standard reader stops permanently on corruption. Recovery scanning is a
separate forensic mode and never reports normal verification.

## 8. Crash and Interruption Semantics

After interruption:

- a locator and valid footer identify a finalized indexed file;
- complete records before a torn record may be diagnosed in streaming mode;
- totals are authoritative only when a valid footer covers them;
- no automatic resume position is inferred from unchecked trailing bytes.

Future resumable writing requires an explicit checkpoint protocol and sink
durability contract; it is not implied by format 1 framing.

## 9. Determinism

Deterministic output removes timestamps and random IDs, fixes candidate order,
and uses deterministic codec settings. It aids reproducible builds and logical
comparison but can expose when two inputs are equal. External randomized
encryption can address that leakage.

## 10. Sensitive Information

The engine does not intentionally log values. Diagnostics may include field
IDs, type IDs, sizes, counts, codec names, and format offsets.

Schemas, field names, row counts, sizes, codec choices, dictionaries, and user
metadata may reveal information in an unencrypted container. Higher layers
should minimize metadata and encrypt containers at rest and in transit when
needed.

## 11. Plugin Security

Native plugins execute with process privileges. The engine:

- loads plugins only through explicit application configuration;
- validates ABI and identifier declarations;
- rejects collisions;
- records required format identifiers, not filesystem paths;
- bounds input and output passed through the contract;
- treats descriptor-bound violations as plugin failures.

The engine cannot sandbox arbitrary native code. A future out-of-process plugin
host would be a separate security design.

## 12. Codec Supply Chain

Codec adapters pin minimum supported library versions, expose version
information, and run upstream malformed-input suites where licensing permits.
Security releases may remove a vulnerable writer capability while retaining a
safe reader path, or vice versa.

Optional codec support must not weaken raw-container parsing.

## 13. Error Disclosure

Errors include structural context but not source values. Plugin and OS messages
are sanitized before crossing a language or process boundary. Paths are
included only when the caller's adapter chooses to add them.

The C ABI provides bounded error copying; no pointer to temporary exception text
escapes a call.

## 14. Security Validation

- Continuous fuzzing of preamble, records, schemas, directories, parameters,
  encoding decoders, and C ABI argument validation.
- Address, undefined-behavior, and thread sanitizers on supported toolchains.
- Integer-boundary and allocation-failure injection.
- Decompression-ratio and worst-case dictionary tests.
- Differential scalar and optimized decoder testing.
- Dependency vulnerability monitoring and reproducible release manifests.
- Manual review before expanding format parser privileges or plugin loading.

## 15. Vulnerability Response

Before 1.0, the project will publish:

- a private reporting channel;
- supported release lines;
- response and disclosure targets;
- severity criteria;
- patched-release and advisory process;
- policy for malformed files created by vulnerable writers.

A security fix never silently changes the interpretation of an existing format
identifier. If safe decoding is impossible, the affected capability is rejected
with a specific unsupported or security error.
