
``` r
library(cpp11)

# assumes that we're in a checkout of the repo and that the default build
# preset has been built (i.e., installed to <checkout dir>/dist).
s2geography_home <- file.path(getwd(), "../../dist")

# this should really be built into the build/install process because at
# this point we've already found and linked to OpenSSL for building s2
if (identical(unname(Sys.info()["sysname"]), "Darwin")) {
  openssl_home <- system("brew --prefix openssl@1.1", intern = TRUE)
  openssl_cflags <- sprintf("-I%s/include", openssl_home)
  openssl_libs <- sprintf("-L%s/lib -lssl -lcrypto", openssl_home)
} else {
  stop("Platform not (yet) supported")
}

Sys.setenv(
  PKG_CXXFLAGS = paste0(openssl_cflags, " -I", s2geography_home, "/include "),
  PKG_LIBS = paste0(openssl_libs, " -L", s2geography_home, "/lib -ls2geography -ls2 -labsl_base -labsl_city -labsl_demangle_internal -labsl_flags_reflection -labsl_graphcycles_internal -labsl_hash -labsl_hashtablez_sampler -labsl_int128 -labsl_low_level_hash -labsl_malloc_internal -labsl_raw_hash_set -labsl_raw_logging_internal -labsl_spinlock_wait -labsl_stacktrace -labsl_str_format_internal -labsl_strings -labsl_symbolize -labsl_synchronization -labsl_throw_delegate -labsl_time_zone -labsl_time")
)

knitr::opts_chunk$set(
  collapse = TRUE,
  comment = "#>",
  fig.path = "man/figures/README-",
  out.width = "100%"
)
```

``` cpp
#include "cpp11.hpp"
#include "s2geography.h"

using namespace s2geography;

[[cpp11::register]]
void test_s2geography() {
  //WKTReader reader;
  //std::unique_ptr<S2Geography> point1 = reader.read_feature("POINT (-64 45)");
  //std::unique_ptr<S2Geography> point2 = reader.read_feature("POINT (-64 45)");
  
  PointGeography point1 = S2LatLng::FromDegrees(45, -64).ToPoint();
  PointGeography point2 = S2LatLng::FromDegrees(45, 0).ToPoint();

  ShapeIndexGeography point1_index(point1);
  ShapeIndexGeography point2_index(point2);

  double dist = s2_distance(point1_index, point2_index);

  Rprintf("distance result is %g", dist);
}
```

``` r
test_s2geography()
#> distance result is 0.768167
```