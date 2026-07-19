# Compatibility Policy

## 1. Independent Version Dimensions

Shibori has separate versions for:

| Dimension | Governs |
| --- | --- |
| engine release | library implementation and defaults |
| C++ API | source-facing native interfaces |
| C ABI | binary interface used by SDK bindings |
| container format | interpretation of stored bytes |
| plugin ABI | native plugin loading contract |
| policy algorithm | adaptive selection behavior |

Equal engine versions do not imply equal container bytes unless deterministic
configuration is pinned. Container decoding does not depend on the original
policy version.

## 2. Container Compatibility

A format 1 reader:

- accepts format 1 minor versions whose mandatory features it supports;
- rejects unknown mandatory records, flags, types, encodings, codecs, and
  features;
- validates unknown optional records before skipping them;
- never reinterprets an existing identifier;
- applies configured resource limits even to valid files.

Writers emit the lowest minor version required by used features.

New major formats require explicit opt-in from writers and a migration or
conversion story before becoming default.

## 3. API Compatibility

Before engine 1.0, public APIs are experimental and may break with release
notes. After 1.0, semantic versioning applies to the documented public C++ API.

The C++ compatibility window will specify supported major versions and compiler
ABIs. Source compatibility is the primary promise; binary compatibility is
promised only where release packaging explicitly states it.

## 4. C ABI Compatibility

Stable ABI revisions may add:

- new functions;
- trailing fields to size-tagged structures;
- new capability values;
- new error codes that old callers treat as unknown.

They may not:

- reorder, remove, or resize existing fields;
- change calling conventions;
- expose C++ standard-library types;
- reuse identifiers or status codes;
- change ownership of existing parameters.

The runtime exposes its ABI range before handle creation.

## 5. Plugin Compatibility

Plugins declare an ABI version and implementation capabilities. A plugin ABI
match permits loading; format identifiers separately determine whether it can
read or write a chunk.

The engine may refuse a plugin with:

- incompatible ABI;
- identifier collision;
- invalid parameter schema;
- missing size bounds;
- unsupported thread-safety declaration;
- known vulnerable implementation version.

Loading compatibility is not a trust endorsement.

## 6. Policy Compatibility

Policy defaults may change as benchmarks improve. Each decision reports policy
name and algorithm version.

Callers needing reproducible selection pin:

- engine release;
- policy algorithm;
- codec implementations;
- deterministic mode;
- all effective limits and allowlists.

Old containers remain readable because actual chains and parameters are stored
in the container.

## 7. Deprecation

After 1.0, a public API deprecation includes:

- replacement API;
- migration example;
- first deprecated release;
- earliest removal major release;
- diagnostic that can be enabled without changing runtime behavior.

Format identifiers are never removed from readers merely because writers stop
emitting them, while their release line remains security-supported.

## 8. Support Windows

Before production release, exact dates remain unset. The project intends to
support:

- the current stable engine major;
- at least one prior stable minor line for security fixes;
- every published format 1 fixture in current format 1 readers;
- supported compiler and OS combinations listed in release metadata.

Unsupported platforms may work but are not claimed until automated release
testing exists.

## 9. Capability Removal

A vulnerable or incorrect writer capability may be disabled in a minor security
release. Reader capability may be disabled only when safe bounded decoding is
not possible.

Such removal must:

- use a specific error;
- identify affected IDs and versions;
- publish an advisory;
- preserve unaffected raw and container capabilities;
- provide conversion guidance when safe tooling exists.

## 10. Compatibility Verification

Every stable release archives:

- installed public headers;
- ABI metadata and symbol inventory;
- container golden fixtures;
- format and capability registries;
- package dependency versions;
- deterministic output fixtures;
- migration and deprecation notes.

CI tests the current release against retained artifacts. Compatibility failures
block release unless accompanied by the required major-version decision.
