add_executable(shibori_engine_smoke_test smoke_test.cpp)
target_link_libraries(shibori_engine_smoke_test PRIVATE Shibori::engine)

add_executable(shibori_engine_error_test error_test.cpp)
target_link_libraries(shibori_engine_error_test PRIVATE Shibori::engine)

if(MSVC)
  target_compile_options(
    shibori_engine_smoke_test
    PRIVATE /W4 /permissive-
  )
  target_compile_options(
    shibori_engine_error_test
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
endif()

add_test(NAME shibori_engine.smoke COMMAND shibori_engine_smoke_test)
add_test(NAME shibori_engine.error COMMAND shibori_engine_error_test)

if(WIN32)
  set_tests_properties(
    shibori_engine.smoke
    shibori_engine.error
    PROPERTIES
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:shibori_engine>"
  )
endif()
