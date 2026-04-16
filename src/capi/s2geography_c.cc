
#include "s2geography_c.h"

#include <array>
#include <cstring>
#include <string>

#include "absl/base/config.h"
#include "geoarrow/geoarrow.h"
#include "nanoarrow/nanoarrow.h"
#include "openssl/opensslv.h"
#include "s2geography/accessors-geog.h"
#include "s2geography/accessors.h"
#include "s2geography/build.h"
#include "s2geography/coverings.h"
#include "s2geography/distance.h"
#include "s2geography/geoarrow-geography.h"
#include "s2geography/linear-referencing.h"
#include "s2geography/operation.h"
#include "s2geography/predicates.h"
#include "s2geography/sedona_udf/sedona_extension.h"

// Helper macros

#define S2GEOGRAPHY_SET_ERROR(err, value)          \
  if ((err) != nullptr) {                          \
    ((struct S2GeogError*)err)->message = (value); \
  }

#define S2GEOGRAPHY_C_BEGIN(err)  \
  S2GEOGRAPHY_SET_ERROR(err, ""); \
  try {
#define S2GEOGRAPHY_C_END(err)                   \
  }                                              \
  catch (std::bad_alloc & e) {                   \
    return ENOMEM;                               \
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
  struct GeoArrowWKTReader wkt_reader;
  struct GeoArrowError error;
  bool wkb_reader_initialized;
  bool wkt_reader_initialized;

  S2GeogFactory()
      : wkb_reader_initialized(false), wkt_reader_initialized(false) {
    error.message[0] = '\0';
  }

  ~S2GeogFactory() {
    if (wkb_reader_initialized) {
      GeoArrowWKBReaderReset(&wkb_reader);
    }
    if (wkt_reader_initialized) {
      GeoArrowWKTReaderReset(&wkt_reader);
    }
  }

  GeoArrowErrorCode EnsureWkbReader() {
    if (!wkb_reader_initialized) {
      GeoArrowErrorCode ec = GeoArrowWKBReaderInit(&wkb_reader);
      if (ec == GEOARROW_OK) {
        wkb_reader_initialized = true;
      }
      return ec;
    }
    return GEOARROW_OK;
  }

  GeoArrowErrorCode EnsureWktReader() {
    if (!wkt_reader_initialized) {
      GeoArrowErrorCode ec = GeoArrowWKTReaderInit(&wkt_reader);
      if (ec == GEOARROW_OK) {
        wkt_reader_initialized = true;
      }
      return ec;
    }
    return GEOARROW_OK;
  }

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

struct S2GeogOp {
  std::unique_ptr<s2geography::Operation> op;

  // Non-copyable
  S2GeogOp(const S2GeogOp&) = delete;
  S2GeogOp& operator=(const S2GeogOp&) = delete;

  S2GeogOp() = default;
  ~S2GeogOp() = default;
};

// Error handling functions

S2GeogErrorCode S2GeogErrorCreate(struct S2GeogError** err) {
  S2GEOGRAPHY_C_BEGIN(nullptr);
  S2GEOGRAPHY_DCHECK(err != nullptr);
  *err = new S2GeogError();
  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(nullptr)
}

const char* S2GeogErrorGetMessage(const struct S2GeogError* err) {
  if (err == nullptr) {
    return "";
  }
  return err->message.c_str();
}

void S2GeogErrorDestroy(struct S2GeogError* err) {
  S2GEOGRAPHY_DCHECK(err != nullptr);
  delete err;
}

// Cell ID function

uint64_t S2GeogLngLatToCellId(const struct S2GeogVertex* v) {
  S2GEOGRAPHY_DCHECK(v != nullptr);

  if (std::isnan(v->v[0]) || std::isnan(v->v[1])) {
    return S2CellId::Sentinel().id();
  } else {
    return S2CellId(
               S2LatLng::FromDegrees(v->v[1], v->v[0]).Normalized().ToPoint())
        .id();
  }
}

// Kernel functions

using KernelInitFunc = void (*)(struct SedonaCScalarKernel*);

static const std::array<KernelInitFunc, 27> kSedonaKernels = {{
    s2geography::sedona_udf::AreaKernel,
    s2geography::sedona_udf::CentroidKernel,
    s2geography::sedona_udf::ClosestPointKernel,
    [](SedonaCScalarKernel* k) { s2geography::sedona_udf::ContainsKernel(k); },
    s2geography::sedona_udf::ConvexHullKernel,
    s2geography::sedona_udf::DifferenceKernel,
    [](SedonaCScalarKernel* k) { s2geography::sedona_udf::DistanceKernel(k); },
    [](SedonaCScalarKernel* k) { s2geography::sedona_udf::EqualsKernel(k); },
    s2geography::sedona_udf::IntersectionKernel,
    [](SedonaCScalarKernel* k) {
      s2geography::sedona_udf::IntersectsKernel(k);
    },
    s2geography::sedona_udf::LengthKernel,
    s2geography::sedona_udf::LineInterpolatePointKernel,
    s2geography::sedona_udf::LineLocatePointKernel,
    [](SedonaCScalarKernel* k) {
      s2geography::sedona_udf::MaxDistanceKernel(k);
    },
    s2geography::sedona_udf::PerimeterKernel,
    [](SedonaCScalarKernel* k) {
      s2geography::sedona_udf::ShortestLineKernel(k);
    },
    s2geography::sedona_udf::SymDifferenceKernel,
    s2geography::sedona_udf::UnionKernel,
    s2geography::sedona_udf::ReducePrecisionKernel,
    s2geography::sedona_udf::SimplifyKernel,
    s2geography::sedona_udf::BufferKernel,
    s2geography::sedona_udf::BufferQuadSegsKernel,
    s2geography::sedona_udf::BufferParamsKernel,
    [](SedonaCScalarKernel* k) {
      s2geography::sedona_udf::DistanceWithinKernel(k);
    },
    s2geography::sedona_udf::CellIdFromPointKernel,
    s2geography::sedona_udf::CoveringCellIdsKernel,
    [](SedonaCScalarKernel* k) {
      s2geography::sedona_udf::LongestLineKernel(k);
    },
}};

size_t S2GeogNumKernels(void) { return kSedonaKernels.size(); }

int S2GeogInitKernels(void* kernels_array, size_t kernels_array_size_bytes,
                      int format) {
  if (format != S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF) {
    return ENOTSUP;
  }

  if (kernels_array_size_bytes !=
      (sizeof(SedonaCScalarKernel) * kSedonaKernels.size())) {
    return EINVAL;
  }

  auto* kernel_ptr =
      reinterpret_cast<struct SedonaCScalarKernel*>(kernels_array);

  for (auto init_func : kSedonaKernels) {
    init_func(kernel_ptr++);
  }

  return 0;
}

// Geography functions

S2GeogErrorCode S2GeogCreate(struct S2Geog** geog) {
  S2GEOGRAPHY_C_BEGIN(nullptr);
  S2GEOGRAPHY_DCHECK(geog != nullptr);
  *geog = new S2Geog();
  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(nullptr)
}

void S2GeogDestroy(struct S2Geog* geog) {
  S2GEOGRAPHY_DCHECK(geog != nullptr);
  delete geog;
}

// Factory functions

S2GeogErrorCode S2GeogFactoryCreate(struct S2GeogFactory** geog_factory) {
  S2GEOGRAPHY_C_BEGIN(nullptr);
  S2GEOGRAPHY_DCHECK(geog_factory != nullptr);
  *geog_factory = new S2GeogFactory();
  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(nullptr)
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

  // Lazily initialize the WKB reader
  GeoArrowErrorCode ec = geog_factory->EnsureWkbReader();
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, "error initializing WKB reader");
    return ec;
  }

  struct GeoArrowBufferView src;
  src.data = buf;
  src.size_bytes = static_cast<int64_t>(buf_size);

  struct GeoArrowGeometryView parsed;
  ec = GeoArrowWKBReaderRead(&geog_factory->wkb_reader, src, &parsed,
                             &geog_factory->error);
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, geog_factory->error.message);
    return ec;
  }

  ec = GeoArrowGeometryShallowCopy(parsed, &out->geom);
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, "error copying geometry nodes");
    return ec;
  }

  out->geog.Init(GeoArrowGeometryAsView(&out->geom));

  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(err);
}

S2GeogErrorCode S2GeogFactoryInitFromWkt(struct S2GeogFactory* geog_factory,
                                         const char* buf, size_t buf_size,
                                         struct S2Geog* out,
                                         struct S2GeogError* err) {
  S2GEOGRAPHY_C_BEGIN(err);

  S2GEOGRAPHY_DCHECK(geog_factory != nullptr);
  S2GEOGRAPHY_DCHECK(out != nullptr);
  S2GEOGRAPHY_DCHECK(buf != nullptr || buf_size == 0);

  // Reset the parse error
  geog_factory->error.message[0] = '\0';

  // Lazily initialize the WKT reader
  GeoArrowErrorCode ec = geog_factory->EnsureWktReader();
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, "error initializing WKT reader");
    return ec;
  }

  // Reset the output geometry to receive the parsed result
  // Ideally we could rewind this to keep the internal coord buffer
  GeoArrowGeometryReset(&out->geom);
  ec = GeoArrowGeometryInit(&out->geom);
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, "error initializing GeoArrowGeometry");
    return ec;
  }

  struct GeoArrowStringView src;
  src.data = buf;
  src.size_bytes = static_cast<int64_t>(buf_size);

  // Initialize a visitor that builds into the output geometry
  struct GeoArrowVisitor v{};
  GeoArrowGeometryInitVisitor(&out->geom, &v);

  ec = GeoArrowWKTReaderVisit(&geog_factory->wkt_reader, src, &v);
  if (ec != GEOARROW_OK) {
    S2GEOGRAPHY_SET_ERROR(err, "error parsing WKT");
    return ec;
  }

  out->geog.Init(GeoArrowGeometryAsView(&out->geom));

  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(err);
}

void S2GeogFactoryDestroy(struct S2GeogFactory* geog_factory) {
  S2GEOGRAPHY_DCHECK(geog_factory != nullptr);
  delete geog_factory;
}

// Rectangle bounder functions

S2GeogErrorCode S2GeogRectBounderCreate(
    struct S2GeogRectBounder** rect_bounder) {
  S2GEOGRAPHY_C_BEGIN(nullptr);
  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  *rect_bounder = new S2GeogRectBounder();
  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(nullptr)
}

void S2GeogRectBounderClear(struct S2GeogRectBounder* rect_bounder) {
  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  rect_bounder->bounder.Clear();
}

S2GeogErrorCode S2GeogRectBounderBound(struct S2GeogRectBounder* rect_bounder,
                                       const struct S2Geog* geog,
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
  return rect_bounder->bounder.is_empty();
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
  S2GEOGRAPHY_DCHECK(rect_bounder != nullptr);
  delete rect_bounder;
}

// Operator functions

S2GeogErrorCode S2GeogOpCreate(struct S2GeogOp** op, int op_id) {
  S2GEOGRAPHY_C_BEGIN(nullptr);
  std::unique_ptr<s2geography::Operation> inner;
  switch (op_id) {
    case S2GEOGRAPHY_OP_INTERSECTS:
      inner = s2geography::Intersects();
      break;
    case S2GEOGRAPHY_OP_CONTAINS:
      inner = s2geography::Contains();
      break;
    case S2GEOGRAPHY_OP_WITHIN:
      inner = s2geography::Within();
      break;
    case S2GEOGRAPHY_OP_EQUALS:
      inner = s2geography::Equals();
      break;
    default:
      return ENOTSUP;
  }

  *op = new S2GeogOp{std::move(inner)};
  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(nullptr);
}

/// \brief Evaluate an operation with two geographies as input
S2GeogErrorCode S2GeogOpEvalGeogGeog(struct S2GeogOp* op, const S2Geog* lhs,
                                     const S2Geog* rhs,
                                     struct S2GeogError* err) {
  S2GEOGRAPHY_C_BEGIN(err);
  S2GEOGRAPHY_DCHECK(op != nullptr);
  S2GEOGRAPHY_DCHECK(lhs != nullptr);
  S2GEOGRAPHY_DCHECK(rhs != nullptr);

  op->op->ExecGeogGeog(lhs->geog, rhs->geog);
  return S2GEOGRAPHY_OK;
  S2GEOGRAPHY_C_END(err);
}

/// \brief Get integer or boolean output for this operation
int64_t S2GeogOpGetInt(struct S2GeogOp* op) {
  S2GEOGRAPHY_DCHECK(op != nullptr);
  S2GEOGRAPHY_DCHECK(op->op != nullptr);
  return op->op->GetInt();
}

/// \brief Destroy a op object
void S2GeogOpDestroy(struct S2GeogOp* op) {
  S2GEOGRAPHY_DCHECK(op != nullptr);
  delete op;
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
