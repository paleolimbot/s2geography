
#include "s2geography_c.h"

#include <openssl/opensslv.h>
#include <s2geography/accessors-geog.h>
#include <s2geography/accessors.h>
#include <s2geography/build.h>
#include <s2geography/coverings.h>
#include <s2geography/distance.h>
#include <s2geography/linear-referencing.h>
#include <s2geography/predicates.h>
#include <s2geography/sedona_udf/sedona_extension.h>

#include <cstring>
#include <string>

#include "absl/base/config.h"
#include "geoarrow/geoarrow.h"
#include "nanoarrow/nanoarrow.h"
#include "s2geography/geoarrow-geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

// Helper macros

#define S2GEOGRAPHY_SET_ERROR(err, value) \
  if ((err) != nullptr) {                 \
    err->message = (value);               \
  }

#define S2GEOGRAPHY_C_BEGIN(err)  \
  S2GEOGRAPHY_SET_ERROR(err, ""); \
  try {
#define S2GEOGRAPHY_C_END(err)                   \
  }                                              \
  catch (std::exception & e) {                   \
    S2GEOGRAPHY_SET_ERROR(err, e.what());        \
    return EINVAL;                               \
  }                                              \
  catch (...) {                                  \
    S2GEOGRAPHY_SET_ERROR(err, "unknown error"); \
    return EINVAL;                               \
  }

// Struct definitions

struct S2GeogError {
  std::string message;
};

struct S2Geog {
  // The main object that is being wrapped
  s2geography::GeoArrowGeography geog;

  // Owns the GeoArrowGeometryView and possibly the coordinates
  struct GeoArrowGeometry geom;

  S2Geog() { GeoArrowGeometryInit(&geom); }
  ~S2Geog() { GeoArrowGeometryReset(&geom); }

  // Non-copyable
  S2Geog(const S2Geog&) = delete;
  S2Geog& operator=(const S2Geog&) = delete;
};

struct S2GeogFactory {
  struct GeoArrowWKBReader wkb_reader;
  struct GeoArrowError error;

  S2GeogFactory() {
    GeoArrowWKBReaderInit(&wkb_reader);
    error.message[0] = '\0';
  }
  ~S2GeogFactory() { GeoArrowWKBReaderReset(&wkb_reader); }

  // Non-copyable
  S2GeogFactory(const S2GeogFactory&) = delete;
  S2GeogFactory& operator=(const S2GeogFactory&) = delete;
};

struct S2GeogRectBounder {
  s2geography::LatLngRectBounder bounder;

  // Non-copyable
  S2GeogRectBounder(const S2GeogRectBounder&) = delete;
  S2GeogRectBounder& operator=(const S2GeogRectBounder&) = delete;

  S2GeogRectBounder() = default;
  ~S2GeogRectBounder() = default;
};

// Error handling functions

S2GeogErrorCode S2GeogErrorCreate(struct S2GeogError** err) {
  if (err == nullptr) {
    return -1;
  }
  *err = new S2GeogError();
  return 0;
}

const char* S2GeogErrorGetMessage(struct S2GeogError* err) {
  if (err == nullptr) {
    return "";
  }
  return err->message.c_str();
}

void S2GeogErrorDestroy(struct S2GeogError* err) { delete err; }

// Cell ID function

uint64_t S2GeogLngLatToCellId(struct S2GeogVertex* v) {
  if (std::isnan(v->v[0]) || std::isnan(v->v[1])) {
    return S2CellId::Sentinel().id();
  } else {
    return S2CellId(
               S2LatLng::FromDegrees(v->v[1], v->v[0]).Normalized().ToPoint())
        .id();
  }
}

// Kernel functions

size_t S2GeogNumKernels(void) { return 27; }

int S2GeogInitKernels(void* kernels_array, size_t kernels_array_size_bytes,
                      int format) {
  if (format != S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF) {
    return ENOTSUP;
  }

  if (kernels_array_size_bytes !=
      (sizeof(SedonaCScalarKernel) * S2GeogNumKernels())) {
    return EINVAL;
  }

  auto* kernel_ptr =
      reinterpret_cast<struct SedonaCScalarKernel*>(kernels_array);

  s2geography::sedona_udf::AreaKernel(kernel_ptr++);
  s2geography::sedona_udf::CentroidKernel(kernel_ptr++);
  s2geography::sedona_udf::ClosestPointKernel(kernel_ptr++);
  s2geography::sedona_udf::ContainsKernel(kernel_ptr++);
  s2geography::sedona_udf::ConvexHullKernel(kernel_ptr++);
  s2geography::sedona_udf::DifferenceKernel(kernel_ptr++);
  s2geography::sedona_udf::DistanceKernel(kernel_ptr++);
  s2geography::sedona_udf::EqualsKernel(kernel_ptr++);
  s2geography::sedona_udf::IntersectionKernel(kernel_ptr++);
  s2geography::sedona_udf::IntersectsKernel(kernel_ptr++);
  s2geography::sedona_udf::LengthKernel(kernel_ptr++);
  s2geography::sedona_udf::LineInterpolatePointKernel(kernel_ptr++);
  s2geography::sedona_udf::LineLocatePointKernel(kernel_ptr++);
  s2geography::sedona_udf::MaxDistanceKernel(kernel_ptr++);
  s2geography::sedona_udf::PerimeterKernel(kernel_ptr++);
  s2geography::sedona_udf::ShortestLineKernel(kernel_ptr++);
  s2geography::sedona_udf::SymDifferenceKernel(kernel_ptr++);
  s2geography::sedona_udf::UnionKernel(kernel_ptr++);
  s2geography::sedona_udf::ReducePrecisionKernel(kernel_ptr++);
  s2geography::sedona_udf::SimplifyKernel(kernel_ptr++);
  s2geography::sedona_udf::BufferKernel(kernel_ptr++);
  s2geography::sedona_udf::BufferQuadSegsKernel(kernel_ptr++);
  s2geography::sedona_udf::BufferParamsKernel(kernel_ptr++);
  s2geography::sedona_udf::DistanceWithinKernel(kernel_ptr++);
  s2geography::sedona_udf::CellIdFromPointKernel(kernel_ptr++);
  s2geography::sedona_udf::CoveringCellIdsKernel(kernel_ptr++);
  s2geography::sedona_udf::LongestLineKernel(kernel_ptr++);

  return 0;
}

// Geography functions

S2GeogErrorCode S2GeogCreate(struct S2Geog** geog) {
  if (geog == nullptr) {
    return EINVAL;
  }
  *geog = new S2Geog();
  return 0;
}

void S2GeogDestroy(struct S2Geog* geog) { delete geog; }

// Factory functions

S2GeogErrorCode S2GeogFactoryCreate(struct S2GeogFactory** geog_factory) {
  if (geog_factory == nullptr) {
    return EINVAL;
  }

  *geog_factory = new S2GeogFactory();
  return 0;
}

S2GeogErrorCode S2GeogFactoryInitFromWkbNonOwning(
    struct S2GeogFactory* geog_factory, const uint8_t* buf, size_t buf_size,
    struct S2Geog* out, struct S2GeogError* err) {
  S2GEOGRAPHY_C_BEGIN(err);

  S2GEOGRAPHY_DCHECK(geog_factory != nullptr);
  S2GEOGRAPHY_DCHECK(out != nullptr);
  S2GEOGRAPHY_DCHECK(buf != nullptr || buf_size == 0);

  // Reset the parse error
  geog_factory->error.message[0] = '\0';

  struct GeoArrowBufferView src;
  src.data = buf;
  src.size_bytes = static_cast<int64_t>(buf_size);

  struct GeoArrowGeometryView parsed;
  GeoArrowErrorCode ec = S2GeographyGeoArrowWKBReaderRead(
      &geog_factory->wkb_reader, src, &parsed, &geog_factory->error);
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, geog_factory->error.message);
    return ec;
  }

  ec = GeoArrowGeometryShallowCopy(parsed, &out->geom);
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, "error copying geometry nodes");
  }

  out->geog.Init(GeoArrowGeometryAsView(&out->geom));

  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(err);
}

void S2GeogFactoryDestroy(struct S2GeogFactory* geog_factory) {
  delete geog_factory;
}

// Rectangle bounder functions

S2GeogErrorCode S2GeogRectBounderCreate(
    struct S2GeogRectBounder** rect_bounder) {
  if (rect_bounder == nullptr) {
    return EINVAL;
  }
  *rect_bounder = new S2GeogRectBounder();
  return S2GEOGRAPHY_OK;
}

void S2GeogRectBounderClear(struct S2GeogRectBounder* rect_bounder) {
  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  rect_bounder->bounder.Clear();
}

S2GeogErrorCode S2GeogRectBounderBound(struct S2GeogRectBounder* rect_bounder,
                                       struct S2Geog* geog,
                                       struct S2GeogError* err) {
  S2GEOGRAPHY_C_BEGIN(err);

  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  S2GEOGRAPHY_DCHECK(geog != nullptr);

  rect_bounder->bounder.Update(geog->geog);

  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(err);
}

uint8_t S2GeogRectBounderIsEmpty(struct S2GeogRectBounder* rect_bounder) {
  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  return rect_bounder->bounder.Finish().is_empty() ? 1 : 0;
}

S2GeogErrorCode S2GeogRectBounderFinish(struct S2GeogRectBounder* rect_bounder,
                                        struct S2GeogVertex* lo,
                                        struct S2GeogVertex* hi,
                                        struct S2GeogError* err) {
  S2GEOGRAPHY_C_BEGIN(err);

  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  S2GEOGRAPHY_DCHECK(lo != nullptr);
  S2GEOGRAPHY_DCHECK(hi != nullptr);

  S2LatLngRect rect = rect_bounder->bounder.Finish();

  lo->v[0] = rect.lng_lo().degrees();
  lo->v[1] = rect.lat_lo().degrees();
  lo->v[2] = 0;
  lo->v[3] = 0;

  hi->v[0] = rect.lng_hi().degrees();
  hi->v[1] = rect.lat_hi().degrees();
  hi->v[2] = 0;
  hi->v[3] = 0;

  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(err);
}

void S2GeogRectBounderDestroy(struct S2GeogRectBounder* rect_bounder) {
  delete rect_bounder;
}

// Version functions

const char* S2GeogNanoarrowVersion(void) {
  static std::string version = std::string() +
                               std::to_string(NANOARROW_VERSION_MAJOR) + "." +
                               std::to_string(NANOARROW_VERSION_MINOR) + "." +
                               std::to_string(NANOARROW_VERSION_PATCH);
  return version.c_str();
}

const char* S2GeogGeoArrowVersion(void) {
  static std::string version = std::string() +
                               std::to_string(GEOARROW_VERSION_MAJOR) + "." +
                               std::to_string(GEOARROW_VERSION_MINOR) + "." +
                               std::to_string(GEOARROW_VERSION_PATCH);
  return version.c_str();
}

const char* S2GeogOpenSSLVersion(void) {
  static std::string version = std::string() +
                               std::to_string(OPENSSL_VERSION_MAJOR) + "." +
                               std::to_string(OPENSSL_VERSION_MINOR) + "." +
                               std::to_string(OPENSSL_VERSION_PATCH);
  return version.c_str();
}

const char* S2GeogS2GeometryVersion(void) {
  static std::string version =
      std::string() + std::to_string(S2_VERSION_MAJOR) + "." +
      std::to_string(S2_VERSION_MINOR) + "." + std::to_string(S2_VERSION_PATCH);
  return version.c_str();
}

const char* S2GeogAbseilVersion(void) {
#if defined(ABSL_LTS_RELEASE_VERSION)
  static std::string version = std::string() +
                               std::to_string(ABSL_LTS_RELEASE_VERSION) + "." +
                               std::to_string(ABSL_LTS_RELEASE_PATCH_LEVEL);
  return version.c_str();
#else
  return "<live at head>";
#endif
}
