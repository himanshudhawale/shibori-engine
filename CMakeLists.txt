cmake_minimum_required(VERSION 3.25)

project(
  ShiboriEngine
  VERSION 0.1.0
  DESCRIPTION "Adaptive, type-aware compression for database data"
  HOMEPAGE_URL "https://github.com/himanshudhawale/shibori-engine"
  LANGUAGES C CXX
)

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(GNUInstallDirs)
include(cmake/Blake3.cmake)

option(BUILD_SHARED_LIBS "Build Shibori Engine as a shared library" OFF)
option(SHIBORI_BUILD_TESTS "Build Shibori Engine tests" ${PROJECT_IS_TOP_LEVEL})

add_library(shibori_engine)
add_library(Shibori::engine ALIAS shibori_engine)
target_sources(shibori_engine PRIVATE $<TARGET_OBJECTS:shibori_blake3>)
target_include_directories(
  shibori_engine
  PRIVATE "${blake3_source_SOURCE_DIR}/c"
)

target_sources(
  shibori_engine
  PRIVATE
    src/block.cpp
    src/checked_arithmetic.cpp
    src/column.cpp
    src/error.cpp
    src/file_io.cpp
    src/io.cpp
    src/integrity.cpp
    src/logical_type.cpp
    src/resource.cpp
    src/schema.cpp
    src/version.cpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86)$")
  target_sources(shibori_engine PRIVATE src/crc32c_x86.cpp)
  target_compile_definitions(shibori_engine PRIVATE SHIBORI_HAS_X86_CRC32C=1)
  if(NOT MSVC)
    set_source_files_properties(
      src/crc32c_x86.cpp
      PROPERTIES COMPILE_OPTIONS "-msse4.2"
    )
  endif()
endif()

target_compile_features(shibori_engine PUBLIC cxx_std_23)

target_include_directories(
  shibori_engine
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_properties(
  shibori_engine
  PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME engine
    OUTPUT_NAME shibori-engine
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)

generate_export_header(
  shibori_engine
  EXPORT_FILE_NAME generated/shibori/engine/export.hpp
  EXPORT_MACRO_NAME SHIBORI_ENGINE_API
)

if(MSVC)
  target_compile_options(shibori_engine PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
  target_compile_options(
    shibori_engine
    PRIVATE
      -Wall
      -Wextra
      -Wconversion
      -Wpedantic
      -Wshadow
  )
endif()

if(SHIBORI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

configure_package_config_file(
  cmake/ShiboriEngineConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ShiboriEngineConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ShiboriEngine"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ShiboriEngineConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)

install(
  TARGETS shibori_engine
  EXPORT ShiboriEngineTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
  DIRECTORY include/shibori
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/shibori/engine/export.hpp"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/shibori/engine"
)

install(
  EXPORT ShiboriEngineTargets
  FILE ShiboriEngineTargets.cmake
  NAMESPACE Shibori::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ShiboriEngine"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ShiboriEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ShiboriEngineConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ShiboriEngine"
)
