
cmake_minimum_required(VERSION 3.11)
include(FetchContent)
message(STATUS "Building using CMake version: ${CMAKE_VERSION}")

project(S2Geography)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/third_party/cmake")

option(S2GEOGRAPHY_FETCH_S2GEOMETRY "Download and build s2geometry from source" OFF)

option(S2GEOGRAPHY_BUILD_TESTS "Build tests" OFF)

option(S2GEOGRAPHY_CODE_COVERAGE "Enable coverage reporting" OFF)
add_library(coverage_config INTERFACE)

option(S2GEOGRAPHY_BUILD_EXAMPLES "Build s2geography examples" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

if(S2GEOGRAPHY_FETCH_S2GEOMETRY)
  FetchContent_Declare(
    s2
    GIT_REPOSITORY https://github.com/google/s2geometry.git
    GIT_TAG tags/v0.10.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(s2)

  set_property(TARGET s2 PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD})
else()
  find_package(s2 REQUIRED)
endif()

find_package(absl REQUIRED)

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

if(BUILD_EXAMPLES)
  add_executable(example-simple examples/example-simple/example-simple.cc)
  target_link_libraries(example-simple PUBLIC s2geography s2)
endif()

install(TARGETS s2geography DESTINATION lib)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.h")

if(S2GEOGRAPHY_BUILD_EXAMPLES)
  install(TARGETS example-simple DESTINATION examples)
endif()

if(S2GEOGRAPHY_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.11.0.zip
  )
  FetchContent_MakeAvailable(googletest)
  enable_testing()

  add_executable(distance_test src/s2geography/distance_test.cc)

  if (S2GEOGRAPHY_CODE_COVERAGE)
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
    target_link_libraries(s2geography coverage_config)
  endif()

  target_link_libraries(distance_test s2geography gtest_main)

  include(GoogleTest)
  gtest_discover_tests(distance_test)

endif()
