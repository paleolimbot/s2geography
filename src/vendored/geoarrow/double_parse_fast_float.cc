
#include "geoarrow.h"

#if defined(GEOARROW_USE_FAST_FLOAT) && GEOARROW_USE_FAST_FLOAT

#include "fast_float/fast_float.h"

// When building a DuckDB extension, this is duckdb_fast_float
#if !defined(GEOARROW_FAST_FLOAT_NAMESPACE)
#define GEOARROW_FAST_FLOAT_NAMESPACE fast_float
#endif

extern "C" GeoArrowErrorCode GeoArrowFromChars(const char* first, const char* last,
                                               double* out) {
  auto answer = GEOARROW_FAST_FLOAT_NAMESPACE::from_chars(first, last, *out);
  if (answer.ec != std::errc()) {
    return EINVAL;
  } else {
    return GEOARROW_OK;
  }
}

#endif
