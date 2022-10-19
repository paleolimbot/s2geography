
cmake_minimum_required(VERSION 3.11)
include(FetchContent)
message(STATUS "Building using CMake version: ${CMAKE_VERSION}")

project(S2Geography)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard to build with")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/third_party/cmake")

option(S2GEOGRAPHY_BUILD_TESTS "Build tests" OFF)
option(S2GEOGRAPHY_CODE_COVERAGE "Enable coverage reporting" OFF)
option(S2GEOGRAPHY_BUILD_EXAMPLES "Build s2geography examples" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Dependencies
# ------------

# s2geometry

if(DEFINED ENV{CONDA_PREFIX})
  set(S2GEOGRAPHY_S2_SOURCE_DEFAULT "CONDA")
else()
  set(S2GEOGRAPHY_S2_SOURCE_DEFAULT "AUTO")
endif()

set(S2GEOGRAPHY_S2_SOURCE
  "${S2GEOGRAPHY_S2_SOURCE_DEFAULT}"
  CACHE STRING "Method to use for acquiring the s2geometry dependency")

message(STATUS "Using ${S2GEOGRAPHY_S2_SOURCE} approach to find s2geometry")

macro(build_s2)
  message(STATUS "Building s2geometry from source")

  FetchContent_Declare(
    s2
    GIT_REPOSITORY https://github.com/google/s2geometry.git
    GIT_TAG tags/v0.10.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(s2)

  set_property(TARGET s2 PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD})
endmacro()

if(${S2GEOGRAPHY_S2_SOURCE} STREQUAL "CONDA")
  set(S2_ROOT "$ENV{CONDA_PREFIX}")
  set(S2_SOURCE "SYSTEM")
elseif(${S2GEOGRAPHY_S2_SOURCE} STREQUAL "BREW")
  # required for Homebrew installed s2geometry headers to find OpenSSL headers
  find_package(OpenSSL)
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(S2_SOURCE "SYSTEM")
else()
  set(S2_SOURCE ${S2GEOGRAPHY_S2_SOURCE})
endif()

if(${S2_SOURCE} STREQUAL "AUTO")
  find_package(s2 QUIET)
  if(NOT ${s2_FOUND})
    build_s2()
  endif()
elseif(${S2_SOURCE} STREQUAL "BUNDLED")
  build_s2()
elseif(${S2_SOURCE} STREQUAL "SYSTEM")
  find_package(s2 REQUIRED)
  if(NOT ${s2_FOUND})
    message(FATAL_ERROR "Couldn't find s2geometry")
  endif()
endif()

# Abseil (bundled build not supported)

find_package(absl REQUIRED)

# GTest (always bundled)

if(S2GEOGRAPHY_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG tags/release-1.12.0
    GIT_SHALLOW TRUE
    )
  FetchContent_MakeAvailable(googletest)

  # do not install googletest by default when running cmake --install
  set_property(DIRECTORY ${googletest_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)

  if(S2GEOGRAPHY_CODE_COVERAGE)
    add_library(coverage_config INTERFACE)
  endif()
endif()

# Build s2geography
# -----------------

include_directories(src)

add_library(s2geography
    src/s2geography/accessors-geog.cc
    src/s2geography/coverings.cc
    src/s2geography/linear-referencing.cc
    src/s2geography/accessors.cc
    src/s2geography/distance.cc
    src/s2geography/predicates.cc
    src/s2geography/build.cc
    src/s2geography/geography.cc
    src/s2geography/wkt-reader.cc
    src/s2geography/wkt-writer.cc)

set_target_properties(s2geography PROPERTIES
    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

target_link_libraries(s2geography s2 absl::memory)

if(S2GEOGRAPHY_BUILD_TESTS)
  enable_testing()

  add_executable(distance_test src/s2geography/distance_test.cc)

  if (S2GEOGRAPHY_CODE_COVERAGE)
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
    target_link_libraries(s2geography coverage_config)
  endif()

  target_link_libraries(distance_test s2geography GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(distance_test)
endif()

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  add_executable(example-simple examples/example-simple/example-simple.cc)
  target_link_libraries(example-simple PUBLIC s2geography s2)
endif()

# Install s2geography
# -------------------

install(TARGETS s2geography DESTINATION lib)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.h")

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  install(TARGETS example-simple DESTINATION examples)
endif()

