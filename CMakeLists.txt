
cmake_minimum_required(VERSION 3.14)

project(s2geography
  LANGUAGES CXX C
  VERSION 0.2.0)

include(FetchContent)
message(STATUS "Building using CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard to build with")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

option(S2GEOGRAPHY_BUILD_TESTS "Build tests" OFF)
option(S2GEOGRAPHY_CODE_COVERAGE "Enable coverage reporting" OFF)
option(S2GEOGRAPHY_BUILD_EXAMPLES "Build s2geography examples" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Dependencies
# ------------

# --- openssl on Apple / Homebrew (dependency via s2geometry)
# copied from Arrow
# (copyright The Apache Software Foundation, Apache License, Version 2.0)

if(APPLE AND NOT OPENSSL_ROOT_DIR)
  find_program(BREW_PROG brew)
  if(BREW_PROG)
    execute_process(COMMAND ${BREW_PROG} --prefix "openssl@1.1"
                    OUTPUT_VARIABLE OPENSSL11_BREW_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(OPENSSL11_BREW_PREFIX)
      set(OPENSSL_ROOT_DIR ${OPENSSL11_BREW_PREFIX})
    else()
      execute_process(COMMAND ${BREW_PROG} --prefix "openssl"
                      OUTPUT_VARIABLE OPENSSL_BREW_PREFIX
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(OPENSSL_BREW_PREFIX)
        set(OPENSSL_ROOT_DIR ${OPENSSL_BREW_PREFIX})
      endif()
    endif()
  endif()
endif()

# --- s2geometry

if(TARGET s2::s2)
  set(S2GEOGRAPHY_S2_SOURCE_DEFAULT "NONE")
elseif(DEFINED ENV{CONDA_PREFIX})
  set(S2GEOGRAPHY_S2_SOURCE_DEFAULT "CONDA")
else()
  set(S2GEOGRAPHY_S2_SOURCE_DEFAULT "AUTO")
endif()

set(S2GEOGRAPHY_S2_SOURCE
  "${S2GEOGRAPHY_S2_SOURCE_DEFAULT}"
  CACHE STRING "Method to use for acquiring the s2geometry dependency")

message(STATUS "Using ${S2GEOGRAPHY_S2_SOURCE} approach to find s2geometry")

macro(extract_s2_version VERSION_STRING)
  string(REPLACE "." ";" VERSION_LIST ${VERSION_STRING})
  list(GET VERSION_LIST 0 S2_VERSION_MAJOR)
  list(GET VERSION_LIST 1 S2_VERSION_MINOR)
  list(GET VERSION_LIST 2 S2_VERSION_PATCH)
endmacro()

macro(find_s2 ARG)
  # s2geometry v0.11.1 introduced a cmake config file. First check for that config
  # or fallback to using the finds2.cmake module included in this repository to find
  # an older version of s2geometry.
  find_package(s2 CONFIG QUIET)
  if(${s2_FOUND})
    set(s2_NOALIAS_TARGET "s2::s2")
    # FIXME: until s2geometry provides the version with its cmake configuration, let's
    # hard-code the version as an hugly workaround
    extract_s2_version("0.11.1")
    get_target_property(s2_INCLUDE_DIRS s2::s2 INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found s2 (version >= 0.11) using its installed cmake config file: ${s2_INCLUDE_DIRS}")

  else()
    find_package(s2 MODULE ${ARG})
    if(${s2_FOUND})
      set(s2_NOALIAS_TARGET "s2")
      add_library(s2::s2 ALIAS s2)
      # no easy way to get the actual version number but at least make it work
      # with s2geography conditional definitions.
      extract_s2_version("0.0.0")
    message(STATUS "Found s2 (version < 0.11) using the finds2.cmake module: ${s2_INCLUDE_DIRS}")
    endif()

  endif()
endmacro()

macro(build_s2)
  message(STATUS "Building s2geometry from source (version 0.11.1)")

  FetchContent_Declare(
    s2
    GIT_REPOSITORY https://github.com/google/s2geometry.git
    GIT_TAG tags/v0.11.1
    GIT_SHALLOW TRUE)

  set(BUILD_EXAMPLES OFF CACHE BOOL "Build s2 documentation examples." FORCE)
  set(BUILD_TESTS OFF CACHE BOOL "Build s2 unittests." FORCE)

  FetchContent_MakeAvailable(s2)

  # silence warnings in s2 headers
  # (note: the S2_USE_SYSTEM_INCLUDES=ON option doesn't seem to work with FetchContent)
  # (TODO: remove this and instead add SYSTEM in FetchContent_Declare - requires CMake >=3.25)
  get_target_property(S2_IID s2 INTERFACE_INCLUDE_DIRECTORIES)
  set_target_properties(s2 PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${S2_IID}")

  set_property(TARGET s2 PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD})

  get_target_property(S2_VERSION_STRING s2 VERSION)
  extract_s2_version(${S2_VERSION_STRING})
  set(s2_NOALIAS_TARGET "s2")
  add_library(s2::s2 ALIAS s2)
endmacro()

if(${S2GEOGRAPHY_S2_SOURCE} STREQUAL "CONDA")
  set(S2_ROOT_DIR "$ENV{CONDA_PREFIX}")
  set(S2_SOURCE "SYSTEM")
elseif(${S2GEOGRAPHY_S2_SOURCE} STREQUAL "BREW")
  set(S2_SOURCE "SYSTEM")
else()
  set(S2_SOURCE ${S2GEOGRAPHY_S2_SOURCE})
endif()

if(${S2_SOURCE} STREQUAL "AUTO")
  find_s2(QUIET)
  if(${s2_FOUND})
    message(STATUS "Found s2: ${s2_INCLUDE_DIRS}")
  else()
    build_s2()
  endif()
elseif(${S2_SOURCE} STREQUAL "BUNDLED")
  build_s2()
elseif(${S2_SOURCE} STREQUAL "SYSTEM")
  find_s2(REQUIRED)
  if(NOT ${s2_FOUND})
    message(FATAL_ERROR "Couldn't find s2geometry")
  endif()
endif()

# --- Abseil (bundled build not supported)

find_package(absl REQUIRED)
if(${absl_FOUND})
  get_target_property(ABSL_INCLUDE_DIRS absl::memory INTERFACE_INCLUDE_DIRECTORIES)
  message(STATUS "Found absl: ${ABSL_INCLUDE_DIRS}/absl")
else()
  message(FATAL_ERROR "Couldn't find absl")
endif()

# --- googletest (always bundled)

if(S2GEOGRAPHY_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG tags/release-1.12.0
    GIT_SHALLOW TRUE
    )

  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE)

  FetchContent_MakeAvailable(googletest)

  # do not install googletest by default when running cmake --install
  set_property(DIRECTORY ${googletest_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)

  # For testing GeoArrow
  FetchContent_Declare(
    nanoarrow
    URL https://github.com/apache/arrow-nanoarrow/archive/refs/tags/apache-arrow-nanoarrow-0.5.0.zip
    URL_HASH
      SHA256=cadbca542806a7554830cbca0393cda3470bdca6d0ce29b5e115b168c6c0b435)

  FetchContent_MakeAvailable(nanoarrow)

  FetchContent_Declare(
    geoarrow
    URL https://github.com/geoarrow/geoarrow-c/archive/refs/tags/geoarrow-c-python-0.1.3.zip
    URL_HASH
      SHA256=67a6781e1767ad666a0bcec5dcbd86356256a1dfd8aae9a54f4aee146e358ce6)

  FetchContent_MakeAvailable(geoarrow)

  if(S2GEOGRAPHY_CODE_COVERAGE)
    add_library(coverage_config INTERFACE)
  endif()
endif()

# Build s2geography
# -----------------

add_library(s2geography
    src/s2geography/accessors-geog.cc
    src/s2geography/coverings.cc
    src/s2geography/linear-referencing.cc
    src/s2geography/accessors.cc
    src/s2geography/distance.cc
    src/s2geography/predicates.cc
    src/s2geography/projections.cc
    src/s2geography/build.cc
    src/s2geography/geography.cc
    src/s2geography/geoarrow.cc
    src/s2geography/wkt-reader.cc
    src/s2geography/wkt-writer.cc
    src/s2geography/op/cell.cc
    src/s2geography/op/point.cc
    # geoarrow sources
    src/vendored/geoarrow/geoarrow.c
    src/vendored/ryu/d2s.c
    src/vendored/geoarrow/double_parse_fast_float.cc)

if(NOT BUILD_SHARED_LIBS)
  set_target_properties(s2geography PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(
  s2geography
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  src/vendored)

target_compile_definitions(
  s2geography
  PUBLIC
  S2_VERSION_MAJOR=${S2_VERSION_MAJOR}
  S2_VERSION_MINOR=${S2_VERSION_MINOR}
  S2_VERSION_PATCH=${S2_VERSION_PATCH}
  PRIVATE
  GEOARROW_USE_FAST_FLOAT=1
  GEOARROW_USE_RYU=1
  GEOARROW_NAMESPACE=S2Geography)

if (MSVC)
  # used in s2geometry's CMakeLists.txt but not defined in target
  target_compile_definitions(s2geography PUBLIC _USE_MATH_DEFINES)
  target_compile_definitions(s2geography PUBLIC NOMINMAX)
  target_compile_options(s2geography PUBLIC /J)
endif()

target_link_libraries(
  s2geography
  PUBLIC
  s2::s2
  absl::memory
  absl::str_format)

if(S2GEOGRAPHY_BUILD_TESTS)

  enable_testing()

  add_executable(distance_test src/s2geography/distance_test.cc)
  add_executable(geoarrow_test src/s2geography/geoarrow_test.cc)
  add_executable(geography_test src/s2geography/geography_test.cc)
  add_executable(wkt_writer_test src/s2geography/wkt-writer_test.cc)
  add_executable(op_cell_test src/s2geography/op/cell_test.cc)

  if (S2GEOGRAPHY_CODE_COVERAGE)
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
    target_link_libraries(s2geography PUBLIC coverage_config)
  endif()

  target_link_libraries(distance_test s2geography GTest::gtest_main)
  target_link_libraries(geoarrow_test s2geography nanoarrow geoarrow GTest::gtest_main GTest::gmock)
  target_link_libraries(geography_test s2geography nanoarrow GTest::gtest_main)
  target_link_libraries(wkt_writer_test s2geography GTest::gtest_main)
  target_link_libraries(op_cell_test s2geography GTest::gtest_main)

  target_include_directories(geoarrow_test PRIVATE src/vendored)

  include(GoogleTest)
  gtest_discover_tests(distance_test)
  gtest_discover_tests(geoarrow_test)
  gtest_discover_tests(geography_test)
  gtest_discover_tests(wkt_writer_test)
  gtest_discover_tests(op_cell_test)
endif()

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  add_executable(example-simple examples/example-simple/example-simple.cc)
  target_link_libraries(example-simple PUBLIC s2geography s2)
endif()

# Install s2geography
# -------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE
    STRING "install path for s2geography cmake configuration files")

message("-- Install prefix: " ${CMAKE_INSTALL_PREFIX})
message("  -> libs: " ${CMAKE_INSTALL_LIBDIR})
message("  -> includes: " ${CMAKE_INSTALL_INCLUDEDIR})
message("  -> cmake config: " ${CMAKECONFIG_INSTALL_DIR})

install(TARGETS s2geography
  EXPORT ${PROJECT_NAME}-targets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(DIRECTORY src/s2geography
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h")
install(FILES src/s2geography.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(S2GEOGRAPHY_BUILD_TESTS AND S2GEOGRAPHY_CODE_COVERAGE)
  install(TARGETS coverage_config
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  install(TARGETS example-simple DESTINATION examples)
endif()

# --- Package config (makes the package importable using find_package in another project)

configure_package_config_file(
  cmake/${PROJECT_NAME}Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR})

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${CMAKECONFIG_INSTALL_DIR})

install(EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}Targets.cmake
  DESTINATION ${CMAKECONFIG_INSTALL_DIR})
