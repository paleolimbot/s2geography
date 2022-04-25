
# s2geography

[![Examples](https://github.com/paleolimbot/s2geography/actions/workflows/run-examples.yaml/badge.svg)](https://github.com/paleolimbot/s2geography/actions/workflows/run-examples.yaml)

Google's [s2geometry](https://github.com/google/s2geometry) is a spherical geometry engine providing accurate and performant geometry operations for geometries on the sphere. This library provides a compatability layer on top of s2geometry for those more familiar with [simple features](https://en.wikipedia.org/wiki/Simple_Features), [GEOS](https://libgeos.org), and/or the [GEOS C API](https://libgeos.org/doxygen/geos__c_8h.html).

The s2geography library was refactored out of the [s2 package for R](https://github.com/r-spatial/s2), which has served as the backend for geometries with geographic coordinates in the popular [sf package for R](https://github.com/r-spatial/sf) since version 1.0.0. The library is currently under construction as it adapts to suit the needs of more than just a single R package. Suggestions to modify, replace, or completely rewrite this library are welcome!

## Example

A quick example (see also the `examples/` directory):

```cpp
#include <stdio.h>

#include <iostream>

#include "s2geography.h"

using namespace s2geography;

int main(int argc, char *argv[]) {
  WKTReader reader;
  std::unique_ptr<Geography> geog1 = reader.read_feature("POINT (-64 45)");
  std::unique_ptr<Geography> geog2 = reader.read_feature(
      "GEOMETRYCOLLECTION (POINT (30 10), LINESTRING (30 10, 10 30, 40 40), "
      "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)))");

  ShapeIndexGeography geog1_index(*geog1);
  ShapeIndexGeography geog2_index(*geog2);

  double dist = s2_distance(geog1_index, geog2_index);

  printf("distance result is %g\n", dist);

  WKTWriter writer;
  std::cout << "geog1: " << writer.write_feature(*geog1) << "\n";
  std::cout << "geog2: " << writer.write_feature(*geog2) << "\n";
}
```

## Overview

The basic unit in s2geography is the `Geography` class. The three main subclasses of this wrap `std::vector<S2Point>`, `std::vector<std::unique_ptr<S2Polyline>>`,, `std::unique_ptr<S2Polygon>`, and `std::vector<std::unique_ptr<Geography>>`; however, the `Geography` class is parameterized as zero or more `S2Shape` objects that also define an `S2Region`. This allows a flexible storage model (although only the four main subclasses have been tested).

Many operations in S2 require a `S2ShapeIndex` as input. This concept is similar to the GEOS prepared geometry and maps to the `ShapeIndexGeography` in this library. For indexing a vector of features, use the `GeographyIndex` (similar to the GEOS STRTree object).

The s2geography library sits on top of the s2geometry library, and you can and should use s2 directly!

## Installation

s2geography depends on s2geometry, which depends on [Abseil](https://github.com/abseil/abseil-cpp) and OpenSSL. You will need to install Abseil from source and install it to the same place you install s2geography (e.g., the homebrew/distributed versions are unlikely to work). Configure with `cmake <src dir> -Dabsl_DIR=.../cmake/absl`, where `.../cmake/absl` contains the `abslConfig.cmake` file. You may also need to specify the location of OpenSSL using `-DOPENSSL_ROOT_DIR=/path/to/openssl@1.1`. The s2 library is fetched and built using CMake's FetchContent module, so you don't need to clone it separately.

The project is structured such that the VSCode `cmake` integration is triggered when the folder is open (if the default build doesn't work, consider adding `CMakeUserPresets.json` to configure things like the install directory, absl_DIR, or the location of OpenSSL).

For example, the GitHub Actions Ubuntu runner is configured like this:

```bash
# clone this repo
git clone https://github.com/paleolimbot/s2geography.git

# build abseil-cpp in the build/ directory, install to dist/
mkdir s2geography/build && cd s2geography/build
git clone https://github.com/abseil/abseil-cpp.git
cmake abseil-cpp -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_CXX_STANDARD=11 -DABSL_ENABLE_INSTALL=ON
cmake --build abseil-cpp
cmake --install abseil-cpp --prefix ../dist

# build s2geography (also fetches and builds s2)
cmake .. -Dabsl_DIR=`pwd`/../dist/lib/x86_64-linux-gnu/cmake/absl -DBUILD_EXAMPLES=ON
cmake --build .
cmake --install . --prefix ../dist
```

Locally (M1 mac), I have to add `-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1` to `cmake ..` when building s2geography.

## TODO

- [ ] Test using GTest. The original version in the s2 package for R has excellent test coverage, but the tests are written in R and should probably be ported here (original test folder: https://github.com/r-spatial/s2/tree/main/tests/testthat).
- [ ] Improve the CMake to work for more use-cases than locally + GitHub actions
