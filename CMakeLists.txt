
cmake_minimum_required(VERSION 3.11)
include(FetchContent)
message(STATUS "Building using CMake version: ${CMAKE_VERSION}")

project(S2Geography)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_ENABLE_INSTALL ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/s2geometry/third_party/cmake")
add_subdirectory(s2geometry)
add_subdirectory(abseil-cpp)

include_directories(src ${OPENSSL_INCLUDE_DIR})
include_directories(src ${ABSL_INCLUDE_DIR})
add_library(s2geography
    src/s2geography/accessors-geog.cc
    src/s2geography/coverings.cc
    src/s2geography/linear-referencing.cc
    src/s2geography/accessors.cc
    src/s2geography/distance.cc
    src/s2geography/predicates.cc
    src/s2geography/build.cc
    src/s2geography/geography.cc
    src/s2geography/wkt-reader.cc)

set_target_properties(s2geography PROPERTIES
    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

target_link_libraries(s2geography s2)

install(TARGETS s2geography DESTINATION lib)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.h")
