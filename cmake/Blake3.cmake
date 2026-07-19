include(FetchContent)

set(SHIBORI_BLAKE3_VERSION "1.8.5" CACHE INTERNAL "Pinned BLAKE3 version")
set(
  SHIBORI_BLAKE3_COMMIT
  "93a431c78a52d7ccf0f366f106467f5070e6075e"
  CACHE INTERNAL
  "Pinned BLAKE3 commit"
)

FetchContent_Declare(
  blake3_source
  URL
    "https://github.com/BLAKE3-team/BLAKE3/archive/${SHIBORI_BLAKE3_COMMIT}.tar.gz"
  URL_HASH
    "SHA256=b92d5bc3450c25007562799542126b408cde9b89f841e8dd503396c24b33e02d"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR shibori-unused
)

FetchContent_MakeAvailable(blake3_source)

add_library(
  shibori_blake3
  OBJECT
    "${blake3_source_SOURCE_DIR}/c/blake3.c"
    "${blake3_source_SOURCE_DIR}/c/blake3_dispatch.c"
    "${blake3_source_SOURCE_DIR}/c/blake3_portable.c"
)

target_compile_definitions(
  shibori_blake3
  PRIVATE
    BLAKE3_NO_AVX2
    BLAKE3_NO_AVX512
    BLAKE3_NO_SSE2
    BLAKE3_NO_SSE41
    BLAKE3_USE_NEON=0
)

set_target_properties(
  shibori_blake3
  PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

install(
  FILES
    "${blake3_source_SOURCE_DIR}/LICENSE_A2"
    "${blake3_source_SOURCE_DIR}/LICENSE_CC0"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/shibori-engine/licenses/blake3"
)
