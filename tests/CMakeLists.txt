add_executable(shibori_engine_smoke_test smoke_test.cpp)
target_link_libraries(shibori_engine_smoke_test PRIVATE Shibori::engine)

add_executable(shibori_engine_error_test error_test.cpp)
target_link_libraries(shibori_engine_error_test PRIVATE Shibori::engine)

add_executable(shibori_engine_resource_test resource_test.cpp)
target_link_libraries(shibori_engine_resource_test PRIVATE Shibori::engine)

add_executable(shibori_engine_logical_type_test logical_type_test.cpp)
target_link_libraries(shibori_engine_logical_type_test PRIVATE Shibori::engine)

add_executable(shibori_engine_schema_test schema_test.cpp)
target_link_libraries(shibori_engine_schema_test PRIVATE Shibori::engine)

add_executable(shibori_engine_column_test column_test.cpp)
target_link_libraries(shibori_engine_column_test PRIVATE Shibori::engine)

add_executable(shibori_engine_block_test block_test.cpp)
target_link_libraries(shibori_engine_block_test PRIVATE Shibori::engine)

add_executable(shibori_engine_io_memory_test io_memory_test.cpp)
target_link_libraries(shibori_engine_io_memory_test PRIVATE Shibori::engine)

add_executable(shibori_engine_io_file_test io_file_test.cpp)
target_link_libraries(shibori_engine_io_file_test PRIVATE Shibori::engine)

add_executable(shibori_engine_integrity_value_test integrity_value_test.cpp)
target_link_libraries(shibori_engine_integrity_value_test PRIVATE Shibori::engine)

add_executable(shibori_engine_crc32c_test crc32c_test.cpp)
target_link_libraries(shibori_engine_crc32c_test PRIVATE Shibori::engine)

add_executable(shibori_engine_blake3_test blake3_test.cpp)
target_link_libraries(shibori_engine_blake3_test PRIVATE Shibori::engine)

add_executable(shibori_engine_format_endian_test format_endian_test.cpp)
target_sources(
  shibori_engine_format_endian_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src/format/endian.cpp"
)
target_link_libraries(shibori_engine_format_endian_test PRIVATE Shibori::engine)
target_include_directories(
  shibori_engine_format_endian_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)

add_executable(shibori_engine_preamble_encode_test preamble_encode_test.cpp)
target_sources(
  shibori_engine_preamble_encode_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src/format/endian.cpp"
    "${PROJECT_SOURCE_DIR}/src/format/preamble.cpp"
)
target_link_libraries(shibori_engine_preamble_encode_test PRIVATE Shibori::engine)
target_include_directories(
  shibori_engine_preamble_encode_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src/format"
)

add_executable(shibori_engine_preamble_parse_test preamble_parse_test.cpp)
target_sources(
  shibori_engine_preamble_parse_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src/format/endian.cpp"
    "${PROJECT_SOURCE_DIR}/src/format/preamble.cpp"
)
target_link_libraries(shibori_engine_preamble_parse_test PRIVATE Shibori::engine)
target_include_directories(
  shibori_engine_preamble_parse_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src/format"
)

add_executable(
  shibori_engine_record_envelope_encode_test
  record_envelope_encode_test.cpp
)
target_sources(
  shibori_engine_record_envelope_encode_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src/format/endian.cpp"
    "${PROJECT_SOURCE_DIR}/src/format/record.cpp"
)
target_link_libraries(
  shibori_engine_record_envelope_encode_test
  PRIVATE Shibori::engine
)
target_include_directories(
  shibori_engine_record_envelope_encode_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src/format"
)

add_executable(
  shibori_engine_record_envelope_parse_test
  record_envelope_parse_test.cpp
)
target_sources(
  shibori_engine_record_envelope_parse_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src/format/endian.cpp"
    "${PROJECT_SOURCE_DIR}/src/format/record.cpp"
)
target_link_libraries(
  shibori_engine_record_envelope_parse_test
  PRIVATE Shibori::engine
)
target_include_directories(
  shibori_engine_record_envelope_parse_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src/format"
)

if(MSVC)
  target_compile_options(
    shibori_engine_smoke_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_error_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_resource_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_logical_type_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_schema_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_column_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_block_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_io_memory_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_io_file_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_integrity_value_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_crc32c_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_blake3_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_format_endian_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_preamble_encode_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_preamble_parse_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_record_envelope_encode_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_record_envelope_parse_test
    PRIVATE /W4 /permissive-
  )
else()
  target_compile_options(
    shibori_engine_smoke_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_error_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_resource_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_logical_type_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_schema_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_column_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_block_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_io_memory_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_io_file_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_integrity_value_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_crc32c_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_blake3_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_format_endian_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_preamble_encode_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_preamble_parse_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_record_envelope_encode_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
  target_compile_options(
    shibori_engine_record_envelope_parse_test
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

add_test(NAME shibori_engine.smoke COMMAND shibori_engine_smoke_test)
add_test(NAME shibori_engine.error COMMAND shibori_engine_error_test)
add_test(NAME shibori_engine.resource COMMAND shibori_engine_resource_test)
add_test(
  NAME shibori_engine.logical_type
  COMMAND shibori_engine_logical_type_test
)
add_test(NAME shibori_engine.schema COMMAND shibori_engine_schema_test)
add_test(NAME shibori_engine.column COMMAND shibori_engine_column_test)
add_test(NAME shibori_engine.block COMMAND shibori_engine_block_test)
add_test(NAME shibori_engine.io_memory COMMAND shibori_engine_io_memory_test)
add_test(NAME shibori_engine.io_file COMMAND shibori_engine_io_file_test)
add_test(
  NAME shibori_engine.integrity_value
  COMMAND shibori_engine_integrity_value_test
)
add_test(NAME shibori_engine.crc32c COMMAND shibori_engine_crc32c_test)
add_test(NAME shibori_engine.blake3 COMMAND shibori_engine_blake3_test)
add_test(
  NAME shibori_engine.format_endian
  COMMAND shibori_engine_format_endian_test
)
add_test(
  NAME shibori_engine.preamble_encode
  COMMAND shibori_engine_preamble_encode_test
)
add_test(
  NAME shibori_engine.preamble_parse
  COMMAND shibori_engine_preamble_parse_test
)
add_test(
  NAME shibori_engine.record_envelope_encode
  COMMAND shibori_engine_record_envelope_encode_test
)
add_test(
  NAME shibori_engine.record_envelope_parse
  COMMAND shibori_engine_record_envelope_parse_test
)

if(WIN32)
  set_tests_properties(
    shibori_engine.smoke
    shibori_engine.error
    shibori_engine.resource
    shibori_engine.logical_type
    shibori_engine.schema
    shibori_engine.column
    shibori_engine.block
    shibori_engine.io_memory
    shibori_engine.io_file
    shibori_engine.integrity_value
    shibori_engine.crc32c
    shibori_engine.blake3
    shibori_engine.format_endian
    shibori_engine.preamble_encode
    shibori_engine.preamble_parse
    shibori_engine.record_envelope_encode
    shibori_engine.record_envelope_parse
    PROPERTIES
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:shibori_engine>"
  )
endif()
