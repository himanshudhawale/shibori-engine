# Support Plan

No production support is claimed during the design phase.

| Capability | MVP | Later |
| --- | --- | --- |
| Input models | Bytes, typed columnar blocks | Nested and evolving schemas |
| Codecs | Raw, Zstandard, LZ4 | Additional codecs through plugins |
| Encodings | Null map, delta, bit pack, dictionary | RLE, delta-of-delta, Gorilla |
| Access | Sequential streaming | Indexed block reads |
| Platforms | Linux x86-64, Windows x86-64 | Linux ARM64, macOS ARM64 |
| API | C++23, experimental C ABI | Stable C ABI |

Database products are intentionally outside the engine support matrix. They are
owned by Shibori Connectors.
