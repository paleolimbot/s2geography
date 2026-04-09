cmake_minimum_required(VERSION 3.14)

# Use better timestamp logic for versions of CMake that include it
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

project(
  s2geography
  LANGUAGES CXX C
  VERSION 0.3.0)

include(FetchContent)
message(STATUS "Building using CMake version: ${CMAKE_VERSION}")

# We need at least C++17 for recent s2 but if this value is already set (e.g.,
# to a higher standard) it should be respected.
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD
      17
      CACHE STRING "The C++ standard to build with")
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

option(S2GEOGRAPHY_BUILD_TESTS "Build tests" OFF)
# These are opt-in because it's hard for all users to control the warnings from
# s2 and Abseil on all platforms. Even when turned on, the extra warnings are
# only added in debug mode.
option(S2GEOGRAPHY_EXTRA_WARNINGS
       "Enable extra compiler warnings in debug mode" OFF)
option(S2GEOGRAPHY_CODE_COVERAGE "Enable coverage reporting" OFF)
option(S2GEOGRAPHY_CLANG_TIDY "Run clang-tidy during build" OFF)
option(S2GEOGRAPHY_BUILD_EXAMPLES "Build s2geography examples" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Dependencies
# ------------

# --- openssl on Apple / Homebrew (dependency via s2geometry) copied from Arrow
# (copyright The Apache Software Foundation, Apache License, Version 2.0)

if(APPLE AND NOT OPENSSL_ROOT_DIR)
  find_program(BREW_PROG brew)
  if(BREW_PROG)
    execute_process(
      COMMAND ${BREW_PROG} --prefix "openssl@3"
      OUTPUT_VARIABLE OPENSSL3_BREW_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(OPENSSL3_BREW_PREFIX)
      set(OPENSSL_ROOT_DIR ${OPENSSL3_BREW_PREFIX})
    else()
      execute_process(
        COMMAND ${BREW_PROG} --prefix "openssl@1.1"
        OUTPUT_VARIABLE OPENSSL11_BREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(OPENSSL11_BREW_PREFIX)
        set(OPENSSL_ROOT_DIR ${OPENSSL11_BREW_PREFIX})
      else()
        execute_process(
          COMMAND ${BREW_PROG} --prefix "openssl"
          OUTPUT_VARIABLE OPENSSL_BREW_PREFIX
          OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(OPENSSL_BREW_PREFIX)
          set(OPENSSL_ROOT_DIR ${OPENSSL_BREW_PREFIX})
        endif()
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
  # s2geometry v0.11.1 introduced a cmake config file. First check for that
  # config or fallback to using the finds2.cmake module included in this
  # repository to find an older version of s2geometry.
  find_package(s2 CONFIG QUIET)
  if(${s2_FOUND})
    set(s2_NOALIAS_TARGET "s2::s2")
    # Extract version information from the found package
    if(DEFINED s2_VERSION)
      extract_s2_version("${s2_VERSION}")
      message(STATUS "Found s2 version: ${s2_VERSION}")
    else()
      # Fallback if s2_VERSION isn't defined
      extract_s2_version("0.11.1")
      message(STATUS "Using fallback s2 version: 0.11.1")
    endif()

    get_target_property(s2_INCLUDE_DIRS s2::s2 INTERFACE_INCLUDE_DIRECTORIES)
    message(
      STATUS
        "Found s2 (version >= 0.11) using its installed cmake config file: ${s2_INCLUDE_DIRS}"
    )

  else()
    find_package(s2 MODULE ${ARG})
    if(${s2_FOUND})
      set(s2_NOALIAS_TARGET "s2")
      add_library(s2::s2 ALIAS s2)
      # no easy way to get the actual version number but at least make it work
      # with s2geography conditional definitions.
      extract_s2_version("0.0.0")
      message(
        STATUS
          "Found s2 (version < 0.11) using the finds2.cmake module: ${s2_INCLUDE_DIRS}"
      )
    endif()

  endif()
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
    message(FATAL_ERROR "Couldn't find s2geometry")
  endif()
elseif(${S2_SOURCE} STREQUAL "SYSTEM")
  find_s2(REQUIRED)
  if(NOT ${s2_FOUND})
    message(FATAL_ERROR "Couldn't find s2geometry")
  endif()
elseif(${S2_SOURCE} STREQUAL "NONE")
  # This branch assumes allows a previously existing s2 target to be used (e.g.,
  # when using s2geography via add_subdirectory())
  message(STATUS "Using previously defined s2 target")
else()
  message(FATAL_ERROR "Invalid value for S2_SOURCE: '${S2_SOURCE}'")
endif()

# --- OpenSSL
find_package(OpenSSL REQUIRED)
get_target_property(OPENSSL_INCLUDE_DIRS OpenSSL::SSL
                    INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIRS}")

# --- Abseil (bundled build not supported)

find_package(absl REQUIRED)
if(${absl_FOUND})
  get_target_property(ABSL_INCLUDE_DIRS absl::memory
                      INTERFACE_INCLUDE_DIRECTORIES)
  message(STATUS "Found absl: ${ABSL_INCLUDE_DIRS}/absl")
else()
  message(FATAL_ERROR "Couldn't find absl")
endif()

# --- nanoarrow
if(NOT TARGET nanoarrow)
  find_package(nanoarrow QUIET)
  if(nanoarrow_FOUND)
    set(S2GEOGRAPHY_NANOARROW_TARGET nanoarrow::nanoarrow)
    get_target_property(NANOARROW_INCLUDE_DIRS nanoarrow::nanoarrow
                        INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found nanoarrow: ${NANOARROW_INCLUDE_DIRS}")
  else()
    message(STATUS "Using vendored nanoarrow")
    set(NANOARROW_SOURCES src/vendored/nanoarrow/nanoarrow.c)
    set(NANOARROW_INCLUDE_DIRECTORY src/vendored)
    if(NANOARROW_NAMESPACE)
      set(NANOARROW_COMPILE_DEFINITIONS
          NANOARROW_NAMESPACE=${NANOARROW_NAMESPACE})
    else()
      set(NANOARROW_COMPILE_DEFINITIONS NANOARROW_NAMESPACE=S2Geography)
    endif()
  endif()
else()
  message(STATUS "Using previously defined nanoarrow target")
  set(S2GEOGRAPHY_NANOARROW_TARGET geoarrow::geoarrow)
endif()

# --- geoarrow (vendored, but we can use an external version of it)
if(NOT TARGET geoarrow)
  message(STATUS "Using vendored geoarrow")
  set(GEOARROW_SOURCES src/vendored/geoarrow/geoarrow.c src/vendored/ryu/d2s.c
                       src/vendored/geoarrow/double_parse_fast_float.cc)
  set(GEOARROW_INCLUDE_DIRECTORY src/vendored)

  if(GEOARROW_NAMESPACE)
    set(GEOARROW_COMPILE_DEFINITIONS
        GEOARROW_USE_FAST_FLOAT=1 GEOARROW_USE_RYU=1
        GEOARROW_NAMESPACE=${GEOARROW_NAMESPACE})
  else()
    set(GEOARROW_COMPILE_DEFINITIONS
        GEOARROW_USE_FAST_FLOAT=1 GEOARROW_USE_RYU=1
        GEOARROW_NAMESPACE=S2Geography)
  endif()
else()
  message(STATUS "Using previously defined geoarrow target")
  set(S2GEOGRAPHY_GEOARROW_TARGET geoarrow::geoarrow)
endif()

# --- googletest (always bundled)

if(S2GEOGRAPHY_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE)

  # For Windows: Prevent overriding the parent project's compiler/linker
  # settings
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "Always use msvcrt.dll" FORCE)

  FetchContent_MakeAvailable(googletest)

  # do not install googletest by default when running cmake --install
  set_property(DIRECTORY ${googletest_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)

  if(S2GEOGRAPHY_CODE_COVERAGE)
    add_library(coverage_config INTERFACE)
  endif()
endif()

# Build s2geography
# -----------------

add_library(
  s2geography
  src/s2geography/accessors-geog.cc
  src/s2geography/accessors.cc
  src/s2geography/build.cc
  src/s2geography/coverings.cc
  src/s2geography/distance.cc
  src/s2geography/geoarrow-geography.cc
  src/s2geography/geoarrow.cc
  src/s2geography/geography_interface.cc
  src/s2geography/geography.cc
  src/s2geography/linear-referencing.cc
  src/s2geography/op/cell.cc
  src/s2geography/op/point.cc
  src/s2geography/predicates.cc
  src/s2geography/projections.cc
  src/s2geography/wkb.cc
  src/s2geography/wkt-reader.cc
  src/s2geography/wkt-writer.cc
  ${GEOARROW_SOURCES}
  ${NANOARROW_SOURCES})

if(NOT BUILD_SHARED_LIBS)
  set_target_properties(s2geography PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(S2GEOGRAPHY_CLANG_TIDY)
  find_program(CLANG_TIDY_PROGRAM clang-tidy)
  if(CLANG_TIDY_PROGRAM)
    set_target_properties(s2geography PROPERTIES CXX_CLANG_TIDY
                                                 "${CLANG_TIDY_PROGRAM}")
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_PROGRAM}")
  else()
    message(WARNING "clang-tidy requested but not found")
  endif()
endif()

target_include_directories(
  s2geography
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${GEOARROW_INCLUDE_DIRECTORY} ${NANOARROW_INCLUDE_DIRECTORY})

target_compile_definitions(
  s2geography
  PUBLIC S2_VERSION_MAJOR=${S2_VERSION_MAJOR}
         S2_VERSION_MINOR=${S2_VERSION_MINOR}
         S2_VERSION_PATCH=${S2_VERSION_PATCH}
  PRIVATE ${GEOARROW_COMPILE_DEFINITIONS})

if(WIN32)
  # used in s2geometry's CMakeLists.txt but not defined in target
  target_compile_definitions(s2geography PUBLIC _USE_MATH_DEFINES)
  target_compile_definitions(s2geography PUBLIC NOMINMAX)
  if(MSVC)
    target_compile_options(s2geography PUBLIC /J)
  endif()
endif()

target_link_libraries(
  s2geography
  PUBLIC s2::s2 absl::memory absl::str_format OpenSSL::SSL OpenSSL::Crypto
  PRIVATE ${S2GEOGRAPHY_NANOARROW_TARGET} ${S2GEOGRAPHY_GEOARROW_TARGET})

# Set somewhat aggressive compiler warning flags
if(S2GEOGRAPHY_EXTRA_WARNINGS)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
      s2geography
      PRIVATE $<$<CONFIG:Debug>:-Wall
              -Werror
              -Wextra
              -Wpedantic
              -Wno-type-limits
              -Wunused-result
              -Wconversion
              # S2's headers in some versions cause this to occur
              -Wno-maybe-uninitialized
              -Wno-sign-conversion>)
    target_compile_options(s2geography PRIVATE -Wno-misleading-indentation)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID
                                                        STREQUAL "Clang")
    target_compile_options(
      s2geography
      PRIVATE $<$<CONFIG:Debug>:-Wall
              -Werror
              -Wextra
              -Wpedantic
              -Wdocumentation
              -Wconversion
              -Wno-sign-conversion
              -Wno-nullability-extension>)
  endif()
endif()

if(S2GEOGRAPHY_BUILD_TESTS)

  enable_testing()

  target_compile_definitions(s2geography PUBLIC ${GEOARROW_COMPILE_DEFINITIONS})
  target_compile_definitions(s2geography
                             PUBLIC ${NANOARROW_COMPILE_DEFINITIONS})

  add_executable(sedona_udf_internal_test
                 src/s2geography/sedona_udf/sedona_udf_internal_test.cc)
  add_executable(accessors_geog_test src/s2geography/accessors-geog_test.cc)
  add_executable(build_test src/s2geography/build_test.cc)
  add_executable(distance_test src/s2geography/distance_test.cc)
  add_executable(geoarrow_test src/s2geography/geoarrow_test.cc)
  add_executable(geography_test src/s2geography/geography_test.cc)
  add_executable(linear_referencing_test
                 src/s2geography/linear-referencing_test.cc)
  add_executable(op_cell_test src/s2geography/op/cell_test.cc)
  add_executable(predicates_test src/s2geography/predicates_test.cc)
  add_executable(wkt_writer_test src/s2geography/wkt-writer_test.cc)
  add_executable(wkb_test src/s2geography/wkb_test.cc)
  add_executable(geoarrow_geography_test
                 src/s2geography/geoarrow-geography_test.cc)

  if(S2GEOGRAPHY_CODE_COVERAGE)
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
    target_link_libraries(s2geography PUBLIC coverage_config)
  endif()

  target_link_libraries(
    sedona_udf_internal_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET}
    GTest::gtest_main GTest::gmock)
  target_link_libraries(
    accessors_geog_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET}
    GTest::gtest_main GTest::gmock)
  target_link_libraries(build_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET}
                        GTest::gtest_main GTest::gmock)
  target_link_libraries(
    distance_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET} GTest::gtest_main
    GTest::gmock)
  target_link_libraries(
    geoarrow_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET} GTest::gtest_main
    GTest::gmock)
  target_link_libraries(
    geography_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET}
    GTest::gtest_main GTest::gmock)
  target_link_libraries(
    linear_referencing_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET}
    GTest::gtest_main GTest::gmock)
  target_link_libraries(op_cell_test s2geography GTest::gtest_main)
  target_link_libraries(
    predicates_test s2geography ${S2GEOGRAPHY_NANOARROW_TARGET}
    GTest::gtest_main GTest::gmock)
  target_link_libraries(wkt_writer_test s2geography GTest::gtest_main)
  target_link_libraries(wkb_test s2geography GTest::gtest_main)
  target_link_libraries(geoarrow_geography_test s2geography GTest::gtest_main)

  target_include_directories(sedona_udf_internal_test PRIVATE src/vendored)
  target_include_directories(accessors_geog_test PRIVATE src/vendored)
  target_include_directories(build_test PRIVATE src/vendored)
  target_include_directories(distance_test PRIVATE src/vendored)
  target_include_directories(geoarrow_test PRIVATE src/vendored)
  target_include_directories(linear_referencing_test PRIVATE src/vendored)
  target_include_directories(predicates_test PRIVATE src/vendored)
  target_include_directories(geoarrow_geography_test PRIVATE src/vendored)

  include(GoogleTest)
  gtest_discover_tests(sedona_udf_internal_test)
  gtest_discover_tests(accessors_geog_test)
  gtest_discover_tests(build_test)
  gtest_discover_tests(distance_test)
  gtest_discover_tests(geoarrow_test)
  gtest_discover_tests(linear_referencing_test)
  gtest_discover_tests(op_cell_test)
  gtest_discover_tests(predicates_test)
  gtest_discover_tests(geography_test)
  gtest_discover_tests(wkt_writer_test)
  gtest_discover_tests(wkb_test)
  gtest_discover_tests(geoarrow_geography_test)
endif()

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  add_executable(example-simple examples/example-simple/example-simple.cc)
  target_link_libraries(example-simple PUBLIC s2geography s2::s2)
endif()

# Install s2geography
# -------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKECONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    CACHE STRING "install path for s2geography cmake configuration files")

message("-- Install prefix: " ${CMAKE_INSTALL_PREFIX})
message("  -> libs: " ${CMAKE_INSTALL_LIBDIR})
message("  -> includes: " ${CMAKE_INSTALL_INCLUDEDIR})
message("  -> cmake config: " ${CMAKECONFIG_INSTALL_DIR})

install(
  TARGETS s2geography
  EXPORT ${PROJECT_NAME}-targets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(
  DIRECTORY src/s2geography
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING
  PATTERN "*.h")
install(FILES src/s2geography.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(S2GEOGRAPHY_BUILD_TESTS AND S2GEOGRAPHY_CODE_COVERAGE)
  install(
    TARGETS coverage_config
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  install(TARGETS example-simple DESTINATION examples)
endif()

# --- Package config (makes the package importable using find_package in another
# project)

configure_package_config_file(
  cmake/${PROJECT_NAME}Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR})

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKECONFIG_INSTALL_DIR})

install(
  EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}Targets.cmake
  DESTINATION ${CMAKECONFIG_INSTALL_DIR})
