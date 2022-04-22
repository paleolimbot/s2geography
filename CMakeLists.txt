
cmake_minimum_required(VERSION 3.11)
include(FetchContent)
message(STATUS "Building using CMake version: ${CMAKE_VERSION}")

project(S2Geography)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ABSL_PROPAGATE_CXX_STD ON)

# Download s2 and abseil
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG tags/20211102.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(absl)

FetchContent_Declare(
  s2
  GIT_REPOSITORY https://github.com/google/s2geometry.git
  GIT_TAG tags/v0.10.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(s2)

include_directories(src ${OPENSSL_INCLUDE_DIR})
add_library(s2geography
    src/s2geography/accessors-geog.cpp
    src/s2geography/coverings.cpp
    src/s2geography/linear-referencing.cpp
    src/s2geography/accessors.cpp
    src/s2geography/distance.cpp
    src/s2geography/predicates.cpp
    src/s2geography/build.cpp
    src/s2geography/geography.cpp)

target_link_libraries(s2geography s2)

install(TARGETS s2geography DESTINATION lib)
install(
    FILES
    src/s2geography/accessors-geog.hpp
    src/s2geography/constructor.hpp
    src/s2geography/index.hpp
    src/s2geography/accessors.hpp
    src/s2geography/coverings.hpp
    src/s2geography/linear-referencing.hpp
    src/s2geography/aggregator.hpp
    src/s2geography/distance.hpp
    src/s2geography/predicates.hpp
    src/s2geography/build.hpp
    src/s2geography/geography.hpp
    src/s2geography.hpp
    DESTINATION include)
