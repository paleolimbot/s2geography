#include "s2geography_c.h"

#include <cstring>
#include <string>

#include <memory>
#include <unordered_set>
#include <vector>

#include "s2geography.h"

#include "s2geography/op/cell.h"
#include "s2geography/op/point.h"

using namespace s2geography;

// ============================================================
// Error handling
// ============================================================

static thread_local std::string g_last_error;

const char* s2geog_last_error(void) {
  return g_last_error.empty() ? nullptr : g_last_error.c_str();
}

#define S2GEOG_TRY try {
#define S2GEOG_CATCH_INT                          \
  }                                               \
  catch (const std::exception& e) {               \
    g_last_error = e.what();                       \
    return 1;                                      \
  }                                               \
  return 0;

#define S2GEOG_CATCH_PTR                          \
  }                                               \
  catch (const std::exception& e) {               \
    g_last_error = e.what();                       \
    return nullptr;                                \
  }

// ============================================================
// Wrap/unwrap helpers
// ============================================================

static inline Geography* unwrap(S2GeogGeography* p) {
  return reinterpret_cast<Geography*>(p);
}
static inline const Geography* unwrap(const S2GeogGeography* p) {
  return reinterpret_cast<const Geography*>(p);
}
static inline S2GeogGeography* wrap_geog(Geography* p) {
  return reinterpret_cast<S2GeogGeography*>(p);
}
static inline S2GeogGeography* wrap_geog(std::unique_ptr<Geography> p) {
  return reinterpret_cast<S2GeogGeography*>(p.release());
}

static inline ShapeIndexGeography* unwrap_idx(S2GeogShapeIndex* p) {
  return reinterpret_cast<ShapeIndexGeography*>(p);
}
static inline const ShapeIndexGeography* unwrap_idx(const S2GeogShapeIndex* p) {
  return reinterpret_cast<const ShapeIndexGeography*>(p);
}
static inline S2GeogShapeIndex* wrap_idx(ShapeIndexGeography* p) {
  return reinterpret_cast<S2GeogShapeIndex*>(p);
}

// ============================================================
// Geography lifecycle
// ============================================================

void s2geog_geography_destroy(S2GeogGeography* geog) {
  delete unwrap(geog);
}

int s2geog_geography_kind(const S2GeogGeography* geog) {
  return static_cast<int>(unwrap(geog)->kind());
}

int s2geog_geography_dimension(const S2GeogGeography* geog) {
  return unwrap(geog)->dimension();
}

int s2geog_geography_num_shapes(const S2GeogGeography* geog) {
  return unwrap(geog)->num_shapes();
}

int s2geog_geography_is_empty(const S2GeogGeography* geog, int* out) {
  S2GEOG_TRY
  *out = s2_is_empty(*unwrap(geog)) ? 1 : 0;
  S2GEOG_CATCH_INT
}

// ============================================================
// Geometry construction from raw coordinates
// ============================================================

static inline S2Point lnglat_to_s2point(double lng, double lat) {
  return S2LatLng::FromDegrees(lat, lng).ToPoint();
}

S2GeogGeography* s2geog_make_point_lnglat(double lng, double lat) {
  S2GEOG_TRY
  return wrap_geog(new PointGeography(lnglat_to_s2point(lng, lat)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_point_xyz(double x, double y, double z) {
  S2GEOG_TRY
  return wrap_geog(new PointGeography(S2Point(x, y, z)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_multipoint_lnglat(const double* lnglat,
                                                 int64_t n) {
  S2GEOG_TRY
  std::vector<S2Point> points(n);
  for (int64_t i = 0; i < n; i++) {
    points[i] = lnglat_to_s2point(lnglat[2 * i], lnglat[2 * i + 1]);
  }
  return wrap_geog(new PointGeography(std::move(points)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_multipoint_xyz(const double* xyz, int64_t n) {
  S2GEOG_TRY
  std::vector<S2Point> points(n);
  for (int64_t i = 0; i < n; i++) {
    points[i] = S2Point(xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]);
  }
  return wrap_geog(new PointGeography(std::move(points)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_polyline_lnglat(const double* lnglat, int64_t n) {
  S2GEOG_TRY
  std::vector<S2Point> vertices(n);
  for (int64_t i = 0; i < n; i++) {
    vertices[i] = lnglat_to_s2point(lnglat[2 * i], lnglat[2 * i + 1]);
  }
  auto polyline = std::make_unique<S2Polyline>(std::move(vertices),
                                                S2Debug::DISABLE);
  return wrap_geog(new PolylineGeography(std::move(polyline)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_polyline_xyz(const double* xyz, int64_t n) {
  S2GEOG_TRY
  std::vector<S2Point> vertices(n);
  for (int64_t i = 0; i < n; i++) {
    vertices[i] = S2Point(xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]);
  }
  auto polyline = std::make_unique<S2Polyline>(std::move(vertices),
                                                S2Debug::DISABLE);
  return wrap_geog(new PolylineGeography(std::move(polyline)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_polygon_lnglat(const double* lnglat,
                                              const int64_t* ring_offsets,
                                              int64_t n_rings) {
  S2GEOG_TRY
  std::vector<std::unique_ptr<S2Loop>> loops;
  for (int64_t r = 0; r < n_rings; r++) {
    int64_t start = ring_offsets[r];
    int64_t end = ring_offsets[r + 1];
    std::vector<S2Point> pts(end - start);
    for (int64_t i = start; i < end; i++) {
      pts[i - start] = lnglat_to_s2point(lnglat[2 * i], lnglat[2 * i + 1]);
    }
    auto loop = std::make_unique<S2Loop>();
    loop->set_s2debug_override(S2Debug::DISABLE);
    loop->Init(pts);
    loop->Normalize();
    loops.push_back(std::move(loop));
  }
  auto polygon = std::make_unique<S2Polygon>();
  polygon->set_s2debug_override(S2Debug::DISABLE);
  polygon->InitNested(std::move(loops));
  return wrap_geog(new PolygonGeography(std::move(polygon)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_polygon_xyz(const double* xyz,
                                          const int64_t* ring_offsets,
                                          int64_t n_rings) {
  S2GEOG_TRY
  std::vector<std::unique_ptr<S2Loop>> loops;
  for (int64_t r = 0; r < n_rings; r++) {
    int64_t start = ring_offsets[r];
    int64_t end = ring_offsets[r + 1];
    std::vector<S2Point> pts(end - start);
    for (int64_t i = start; i < end; i++) {
      pts[i - start] = S2Point(xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]);
    }
    auto loop = std::make_unique<S2Loop>();
    loop->set_s2debug_override(S2Debug::DISABLE);
    loop->Init(pts);
    loop->Normalize();
    loops.push_back(std::move(loop));
  }
  auto polygon = std::make_unique<S2Polygon>();
  polygon->set_s2debug_override(S2Debug::DISABLE);
  polygon->InitNested(std::move(loops));
  return wrap_geog(new PolygonGeography(std::move(polygon)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_make_collection(S2GeogGeography** geogs, int64_t n) {
  S2GEOG_TRY
  std::vector<std::unique_ptr<Geography>> features(n);
  for (int64_t i = 0; i < n; i++) {
    features[i].reset(unwrap(geogs[i]));
  }
  return wrap_geog(new GeographyCollection(std::move(features)));
  S2GEOG_CATCH_PTR
}

// ============================================================
// WKT IO
// ============================================================

S2GeogWKTReader* s2geog_wkt_reader_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogWKTReader*>(new WKTReader());
  S2GEOG_CATCH_PTR
}

void s2geog_wkt_reader_destroy(S2GeogWKTReader* reader) {
  delete reinterpret_cast<WKTReader*>(reader);
}

S2GeogGeography* s2geog_wkt_reader_read(S2GeogWKTReader* reader,
                                         const char* wkt, int64_t size) {
  S2GEOG_TRY
  auto* r = reinterpret_cast<WKTReader*>(reader);
  std::unique_ptr<Geography> geog;
  if (size < 0) {
    geog = r->read_feature(wkt);
  } else {
    geog = r->read_feature(wkt, size);
  }
  return wrap_geog(std::move(geog));
  S2GEOG_CATCH_PTR
}

S2GeogWKTWriter* s2geog_wkt_writer_new(int precision) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogWKTWriter*>(new WKTWriter(precision));
  S2GEOG_CATCH_PTR
}

void s2geog_wkt_writer_destroy(S2GeogWKTWriter* writer) {
  delete reinterpret_cast<WKTWriter*>(writer);
}

char* s2geog_wkt_writer_write(S2GeogWKTWriter* writer,
                               const S2GeogGeography* geog) {
  S2GEOG_TRY
  auto* w = reinterpret_cast<WKTWriter*>(writer);
  std::string result = w->write_feature(*unwrap(geog));
  char* out = static_cast<char*>(malloc(result.size() + 1));
  memcpy(out, result.c_str(), result.size() + 1);
  return out;
  S2GEOG_CATCH_PTR
}

void s2geog_string_free(char* str) {
  free(str);
}

// ============================================================
// Accessors
// ============================================================

int s2geog_area(const S2GeogGeography* geog, double* out) {
  S2GEOG_TRY
  *out = s2_area(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_length(const S2GeogGeography* geog, double* out) {
  S2GEOG_TRY
  *out = s2_length(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_perimeter(const S2GeogGeography* geog, double* out) {
  S2GEOG_TRY
  *out = s2_perimeter(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_x(const S2GeogGeography* geog, double* out) {
  S2GEOG_TRY
  *out = s2_x(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_y(const S2GeogGeography* geog, double* out) {
  S2GEOG_TRY
  *out = s2_y(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_num_points(const S2GeogGeography* geog, int* out) {
  S2GEOG_TRY
  *out = s2_num_points(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_is_collection(const S2GeogGeography* geog, int* out) {
  S2GEOG_TRY
  *out = s2_is_collection(*unwrap(geog)) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_find_validation_error(const S2GeogGeography* geog, char* buf,
                                  int64_t buf_size, int* out) {
  S2GEOG_TRY
  S2Error error;
  bool found = s2_find_validation_error(*unwrap(geog), &error);
  *out = found ? 1 : 0;
  if (found && buf && buf_size > 0) {
    std::string msg = error.text();
    int64_t copy_len =
        static_cast<int64_t>(msg.size()) < buf_size - 1 ? msg.size() : buf_size - 1;
    memcpy(buf, msg.c_str(), copy_len);
    buf[copy_len] = '\0';
  }
  S2GEOG_CATCH_INT
}

// ============================================================
// ShapeIndex
// ============================================================

S2GeogShapeIndex* s2geog_shape_index_new(const S2GeogGeography* geog) {
  S2GEOG_TRY
  auto* idx = new ShapeIndexGeography(*unwrap(geog));
  return wrap_idx(idx);
  S2GEOG_CATCH_PTR
}

void s2geog_shape_index_destroy(S2GeogShapeIndex* idx) {
  delete unwrap_idx(idx);
}

// ============================================================
// Predicates
// ============================================================

int s2geog_intersects(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                      int* out) {
  S2GEOG_TRY
  S2BooleanOperation::Options opts;
  *out = s2_intersects(*unwrap_idx(a), *unwrap_idx(b), opts) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_equals(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                  int* out) {
  S2GEOG_TRY
  S2BooleanOperation::Options opts;
  *out = s2_equals(*unwrap_idx(a), *unwrap_idx(b), opts) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_contains(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                    int* out) {
  S2GEOG_TRY
  S2BooleanOperation::Options opts;
  *out = s2_contains(*unwrap_idx(a), *unwrap_idx(b), opts) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_touches(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                   int* out) {
  S2GEOG_TRY
  S2BooleanOperation::Options opts;
  *out = s2_touches(*unwrap_idx(a), *unwrap_idx(b), opts) ? 1 : 0;
  S2GEOG_CATCH_INT
}

// ============================================================
// Distance
// ============================================================

int s2geog_distance(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                    double* out) {
  S2GEOG_TRY
  *out = s2_distance(*unwrap_idx(a), *unwrap_idx(b));
  S2GEOG_CATCH_INT
}

int s2geog_max_distance(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                        double* out) {
  S2GEOG_TRY
  *out = s2_max_distance(*unwrap_idx(a), *unwrap_idx(b));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_closest_point(const S2GeogShapeIndex* a,
                                       const S2GeogShapeIndex* b) {
  S2GEOG_TRY
  S2Point pt = s2_closest_point(*unwrap_idx(a), *unwrap_idx(b));
  return wrap_geog(new PointGeography(pt));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_minimum_clearance_line_between(
    const S2GeogShapeIndex* a, const S2GeogShapeIndex* b) {
  S2GEOG_TRY
  auto pair = s2_minimum_clearance_line_between(*unwrap_idx(a), *unwrap_idx(b));
  return wrap_geog(new PolylineGeography(std::make_unique<S2Polyline>(
      std::vector<S2Point>{pair.first, pair.second}, S2Debug::DISABLE)));
  S2GEOG_CATCH_PTR
}

// ============================================================
// Geometry-returning operations
// ============================================================

S2GeogGeography* s2geog_centroid(const S2GeogGeography* geog) {
  S2GEOG_TRY
  S2Point pt = s2_centroid(*unwrap(geog));
  return wrap_geog(new PointGeography(pt));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_boundary(const S2GeogGeography* geog) {
  S2GEOG_TRY
  return wrap_geog(s2_boundary(*unwrap(geog)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_convex_hull(const S2GeogGeography* geog) {
  S2GEOG_TRY
  return wrap_geog(s2_convex_hull(*unwrap(geog)));
  S2GEOG_CATCH_PTR
}

// ============================================================
// Boolean operations
// ============================================================

S2GeogGeography* s2geog_intersection(const S2GeogShapeIndex* a,
                                      const S2GeogShapeIndex* b) {
  S2GEOG_TRY
  GlobalOptions opts;
  return wrap_geog(s2_boolean_operation(
      *unwrap_idx(a), *unwrap_idx(b),
      S2BooleanOperation::OpType::INTERSECTION, opts));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_union(const S2GeogShapeIndex* a,
                               const S2GeogShapeIndex* b) {
  S2GEOG_TRY
  GlobalOptions opts;
  return wrap_geog(s2_boolean_operation(
      *unwrap_idx(a), *unwrap_idx(b),
      S2BooleanOperation::OpType::UNION, opts));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_difference(const S2GeogShapeIndex* a,
                                    const S2GeogShapeIndex* b) {
  S2GEOG_TRY
  GlobalOptions opts;
  return wrap_geog(s2_boolean_operation(
      *unwrap_idx(a), *unwrap_idx(b),
      S2BooleanOperation::OpType::DIFFERENCE, opts));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_sym_difference(const S2GeogShapeIndex* a,
                                        const S2GeogShapeIndex* b) {
  S2GEOG_TRY
  GlobalOptions opts;
  return wrap_geog(s2_boolean_operation(
      *unwrap_idx(a), *unwrap_idx(b),
      S2BooleanOperation::OpType::SYMMETRIC_DIFFERENCE, opts));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_unary_union(const S2GeogShapeIndex* geog) {
  S2GEOG_TRY
  GlobalOptions opts;
  return wrap_geog(s2_unary_union(*unwrap_idx(geog), opts));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_rebuild(const S2GeogGeography* geog) {
  S2GEOG_TRY
  GlobalOptions opts;
  return wrap_geog(s2_rebuild(*unwrap(geog), opts));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_build_point(const S2GeogGeography* geog) {
  S2GEOG_TRY
  return wrap_geog(s2_build_point(*unwrap(geog)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_build_polyline(const S2GeogGeography* geog) {
  S2GEOG_TRY
  return wrap_geog(s2_build_polyline(*unwrap(geog)));
  S2GEOG_CATCH_PTR
}

S2GeogGeography* s2geog_build_polygon(const S2GeogGeography* geog) {
  S2GEOG_TRY
  return wrap_geog(s2_build_polygon(*unwrap(geog)));
  S2GEOG_CATCH_PTR
}

// ============================================================
// WKB IO
// ============================================================

S2GeogWKBReader* s2geog_wkb_reader_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogWKBReader*>(new WKBReader());
  S2GEOG_CATCH_PTR
}

void s2geog_wkb_reader_destroy(S2GeogWKBReader* reader) {
  delete reinterpret_cast<WKBReader*>(reader);
}

S2GeogGeography* s2geog_wkb_reader_read(S2GeogWKBReader* reader,
                                          const uint8_t* bytes, int64_t size) {
  S2GEOG_TRY
  auto* r = reinterpret_cast<WKBReader*>(reader);
  return wrap_geog(r->ReadFeature(bytes, size));
  S2GEOG_CATCH_PTR
}

S2GeogWKBWriter* s2geog_wkb_writer_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogWKBWriter*>(new WKBWriter());
  S2GEOG_CATCH_PTR
}

void s2geog_wkb_writer_destroy(S2GeogWKBWriter* writer) {
  delete reinterpret_cast<WKBWriter*>(writer);
}

int s2geog_wkb_writer_write(S2GeogWKBWriter* writer,
                             const S2GeogGeography* geog, uint8_t** out,
                             int64_t* out_size) {
  S2GEOG_TRY
  auto* w = reinterpret_cast<WKBWriter*>(writer);
  std::string result = w->WriteFeature(*unwrap(geog));
  *out_size = static_cast<int64_t>(result.size());
  *out = static_cast<uint8_t*>(malloc(result.size()));
  memcpy(*out, result.data(), result.size());
  S2GEOG_CATCH_INT
}

void s2geog_bytes_free(uint8_t* bytes) {
  free(bytes);
}

// ============================================================
// Coverings
// ============================================================

int s2geog_covering(const S2GeogGeography* geog, int max_cells,
                    uint64_t** cell_ids_out, int64_t* n_out) {
  S2GEOG_TRY
  S2RegionCoverer::Options opts;
  opts.set_max_cells(max_cells);
  S2RegionCoverer coverer(opts);
  std::vector<S2CellId> covering;
  s2_covering(*unwrap(geog), &covering, coverer);
  *n_out = static_cast<int64_t>(covering.size());
  *cell_ids_out = static_cast<uint64_t*>(malloc(covering.size() * sizeof(uint64_t)));
  for (size_t i = 0; i < covering.size(); i++) {
    (*cell_ids_out)[i] = covering[i].id();
  }
  S2GEOG_CATCH_INT
}

int s2geog_interior_covering(const S2GeogGeography* geog, int max_cells,
                              uint64_t** cell_ids_out, int64_t* n_out) {
  S2GEOG_TRY
  S2RegionCoverer::Options opts;
  opts.set_max_cells(max_cells);
  S2RegionCoverer coverer(opts);
  std::vector<S2CellId> covering;
  s2_interior_covering(*unwrap(geog), &covering, coverer);
  *n_out = static_cast<int64_t>(covering.size());
  *cell_ids_out = static_cast<uint64_t*>(malloc(covering.size() * sizeof(uint64_t)));
  for (size_t i = 0; i < covering.size(); i++) {
    (*cell_ids_out)[i] = covering[i].id();
  }
  S2GEOG_CATCH_INT
}

void s2geog_cell_ids_free(uint64_t* cell_ids) {
  free(cell_ids);
}

// ============================================================
// Linear referencing
// ============================================================

int s2geog_project_normalized(const S2GeogGeography* geog1,
                               const S2GeogGeography* geog2, double* out) {
  S2GEOG_TRY
  *out = s2_project_normalized(*unwrap(geog1), *unwrap(geog2));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_interpolate_normalized(const S2GeogGeography* geog,
                                                double distance_norm) {
  S2GEOG_TRY
  S2Point pt = s2_interpolate_normalized(*unwrap(geog), distance_norm);
  return wrap_geog(new PointGeography(pt));
  S2GEOG_CATCH_PTR
}

// ============================================================
// Op/Point
// ============================================================

void s2geog_op_point_to_lnglat(const double point[3], double lnglat_out[2]) {
  op::point::Point p = {point[0], point[1], point[2]};
  op::point::LngLat ll = op::Execute<op::point::ToLngLat>(p);
  lnglat_out[0] = ll[0];
  lnglat_out[1] = ll[1];
}

void s2geog_op_point_to_point(const double lnglat[2], double point_out[3]) {
  op::point::LngLat ll = {lnglat[0], lnglat[1]};
  op::point::Point p = op::Execute<op::point::ToPoint>(ll);
  point_out[0] = p[0];
  point_out[1] = p[1];
  point_out[2] = p[2];
}

// ============================================================
// Op/Cell
// ============================================================

int s2geog_op_cell_from_token(const char* token, uint64_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::FromToken>(std::string_view(token));
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_from_debug_string(const char* debug_str, uint64_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::FromDebugString>(std::string_view(debug_str));
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_from_point(const double point[3], uint64_t* out) {
  S2GEOG_TRY
  op::point::Point p = {point[0], point[1], point[2]};
  *out = op::Execute<op::cell::FromPoint>(p);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_to_point(uint64_t cell_id, double point_out[3]) {
  S2GEOG_TRY
  op::point::Point p = op::Execute<op::cell::ToPoint>(cell_id);
  point_out[0] = p[0]; point_out[1] = p[1]; point_out[2] = p[2];
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_to_token(uint64_t cell_id, char* buf, int64_t buf_size) {
  S2GEOG_TRY
  std::string tok = op::ExecuteString<op::cell::ToToken>(cell_id);
  int64_t copy_len = static_cast<int64_t>(tok.size()) < buf_size - 1
                         ? tok.size() : buf_size - 1;
  memcpy(buf, tok.c_str(), copy_len);
  buf[copy_len] = '\0';
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_to_debug_string(uint64_t cell_id, char* buf,
                                    int64_t buf_size) {
  S2GEOG_TRY
  std::string s = op::ExecuteString<op::cell::ToDebugString>(cell_id);
  int64_t copy_len = static_cast<int64_t>(s.size()) < buf_size - 1
                         ? s.size() : buf_size - 1;
  memcpy(buf, s.c_str(), copy_len);
  buf[copy_len] = '\0';
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_is_valid(uint64_t cell_id, int* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::IsValid>(cell_id) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_center(uint64_t cell_id, double point_out[3]) {
  S2GEOG_TRY
  op::point::Point p = op::Execute<op::cell::CellCenter>(cell_id);
  point_out[0] = p[0]; point_out[1] = p[1]; point_out[2] = p[2];
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_vertex(uint64_t cell_id, int8_t vertex_id,
                           double point_out[3]) {
  S2GEOG_TRY
  op::point::Point p = op::Execute<op::cell::CellVertex>(cell_id, vertex_id);
  point_out[0] = p[0]; point_out[1] = p[1]; point_out[2] = p[2];
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_level(uint64_t cell_id, int8_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::Level>(cell_id);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_area(uint64_t cell_id, double* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::Area>(cell_id);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_area_approx(uint64_t cell_id, double* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::AreaApprox>(cell_id);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_parent(uint64_t cell_id, int8_t level, uint64_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::Parent>(cell_id, level);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_child(uint64_t cell_id, int8_t k, uint64_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::Child>(cell_id, k);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_edge_neighbor(uint64_t cell_id, int8_t k, uint64_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::EdgeNeighbor>(cell_id, k);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_contains(uint64_t cell_id, uint64_t cell_id_test,
                             int* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::Contains>(cell_id, cell_id_test) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_may_intersect(uint64_t cell_id, uint64_t cell_id_test,
                                  int* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::MayIntersect>(cell_id, cell_id_test) ? 1 : 0;
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_distance(uint64_t cell_id, uint64_t cell_id_test,
                             double* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::Distance>(cell_id, cell_id_test);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_max_distance(uint64_t cell_id, uint64_t cell_id_test,
                                 double* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::MaxDistance>(cell_id, cell_id_test);
  S2GEOG_CATCH_INT
}

int s2geog_op_cell_common_ancestor_level(uint64_t cell_id,
                                          uint64_t cell_id_test,
                                          int8_t* out) {
  S2GEOG_TRY
  *out = op::Execute<op::cell::CommonAncestorLevel>(cell_id, cell_id_test);
  S2GEOG_CATCH_INT
}

// ============================================================
// Aggregators
// ============================================================

S2GeogCentroidAggregator* s2geog_centroid_aggregator_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogCentroidAggregator*>(new CentroidAggregator());
  S2GEOG_CATCH_PTR
}

void s2geog_centroid_aggregator_destroy(S2GeogCentroidAggregator* agg) {
  delete reinterpret_cast<CentroidAggregator*>(agg);
}

int s2geog_centroid_aggregator_add(S2GeogCentroidAggregator* agg,
                                    const S2GeogGeography* geog) {
  S2GEOG_TRY
  reinterpret_cast<CentroidAggregator*>(agg)->Add(*unwrap(geog));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_centroid_aggregator_finalize(
    S2GeogCentroidAggregator* agg) {
  S2GEOG_TRY
  S2Point pt = reinterpret_cast<CentroidAggregator*>(agg)->Finalize();
  return wrap_geog(new PointGeography(pt));
  S2GEOG_CATCH_PTR
}

S2GeogConvexHullAggregator* s2geog_convex_hull_aggregator_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogConvexHullAggregator*>(
      new S2ConvexHullAggregator());
  S2GEOG_CATCH_PTR
}

void s2geog_convex_hull_aggregator_destroy(S2GeogConvexHullAggregator* agg) {
  delete reinterpret_cast<S2ConvexHullAggregator*>(agg);
}

int s2geog_convex_hull_aggregator_add(S2GeogConvexHullAggregator* agg,
                                       const S2GeogGeography* geog) {
  S2GEOG_TRY
  reinterpret_cast<S2ConvexHullAggregator*>(agg)->Add(*unwrap(geog));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_convex_hull_aggregator_finalize(
    S2GeogConvexHullAggregator* agg) {
  S2GEOG_TRY
  return wrap_geog(
      reinterpret_cast<S2ConvexHullAggregator*>(agg)->Finalize());
  S2GEOG_CATCH_PTR
}

S2GeogRebuildAggregator* s2geog_rebuild_aggregator_new(void) {
  S2GEOG_TRY
  GlobalOptions opts;
  return reinterpret_cast<S2GeogRebuildAggregator*>(
      new RebuildAggregator(opts));
  S2GEOG_CATCH_PTR
}

void s2geog_rebuild_aggregator_destroy(S2GeogRebuildAggregator* agg) {
  delete reinterpret_cast<RebuildAggregator*>(agg);
}

int s2geog_rebuild_aggregator_add(S2GeogRebuildAggregator* agg,
                                   const S2GeogGeography* geog) {
  S2GEOG_TRY
  reinterpret_cast<RebuildAggregator*>(agg)->Add(*unwrap(geog));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_rebuild_aggregator_finalize(
    S2GeogRebuildAggregator* agg) {
  S2GEOG_TRY
  return wrap_geog(
      reinterpret_cast<RebuildAggregator*>(agg)->Finalize());
  S2GEOG_CATCH_PTR
}

S2GeogCoverageUnionAggregator* s2geog_coverage_union_aggregator_new(void) {
  S2GEOG_TRY
  GlobalOptions opts;
  return reinterpret_cast<S2GeogCoverageUnionAggregator*>(
      new S2CoverageUnionAggregator(opts));
  S2GEOG_CATCH_PTR
}

void s2geog_coverage_union_aggregator_destroy(
    S2GeogCoverageUnionAggregator* agg) {
  delete reinterpret_cast<S2CoverageUnionAggregator*>(agg);
}

int s2geog_coverage_union_aggregator_add(S2GeogCoverageUnionAggregator* agg,
                                          const S2GeogGeography* geog) {
  S2GEOG_TRY
  reinterpret_cast<S2CoverageUnionAggregator*>(agg)->Add(*unwrap(geog));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_coverage_union_aggregator_finalize(
    S2GeogCoverageUnionAggregator* agg) {
  S2GEOG_TRY
  return wrap_geog(
      reinterpret_cast<S2CoverageUnionAggregator*>(agg)->Finalize());
  S2GEOG_CATCH_PTR
}

S2GeogUnionAggregator* s2geog_union_aggregator_new(void) {
  S2GEOG_TRY
  GlobalOptions opts;
  return reinterpret_cast<S2GeogUnionAggregator*>(new S2UnionAggregator(opts));
  S2GEOG_CATCH_PTR
}

void s2geog_union_aggregator_destroy(S2GeogUnionAggregator* agg) {
  delete reinterpret_cast<S2UnionAggregator*>(agg);
}

int s2geog_union_aggregator_add(S2GeogUnionAggregator* agg,
                                 const S2GeogGeography* geog) {
  S2GEOG_TRY
  reinterpret_cast<S2UnionAggregator*>(agg)->Add(*unwrap(geog));
  S2GEOG_CATCH_INT
}

S2GeogGeography* s2geog_union_aggregator_finalize(
    S2GeogUnionAggregator* agg) {
  S2GEOG_TRY
  return wrap_geog(
      reinterpret_cast<S2UnionAggregator*>(agg)->Finalize());
  S2GEOG_CATCH_PTR
}

// ============================================================
// GeographyIndex
// ============================================================

S2GeogGeographyIndex* s2geog_geography_index_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogGeographyIndex*>(new GeographyIndex());
  S2GEOG_CATCH_PTR
}

void s2geog_geography_index_destroy(S2GeogGeographyIndex* index) {
  delete reinterpret_cast<GeographyIndex*>(index);
}

int s2geog_geography_index_add(S2GeogGeographyIndex* index,
                                const S2GeogGeography* geog, int value) {
  S2GEOG_TRY
  reinterpret_cast<GeographyIndex*>(index)->Add(*unwrap(geog), value);
  S2GEOG_CATCH_INT
}

int s2geog_geography_index_query(S2GeogGeographyIndex* index,
                                  const S2GeogGeography* geog,
                                  int32_t** results_out, int64_t* n_out) {
  S2GEOG_TRY
  auto* idx = reinterpret_cast<GeographyIndex*>(index);
  GeographyIndex::Iterator it(idx);
  std::vector<S2CellId> covering;
  unwrap(geog)->GetCellUnionBound(&covering);
  std::unordered_set<int> indices;
  it.Query(covering, &indices);

  *n_out = static_cast<int64_t>(indices.size());
  *results_out = static_cast<int32_t*>(malloc(indices.size() * sizeof(int32_t)));
  int64_t i = 0;
  for (int val : indices) {
    (*results_out)[i++] = val;
  }
  S2GEOG_CATCH_INT
}

void s2geog_int32_free(int32_t* ptr) {
  free(ptr);
}

// ============================================================
// ArrowUDF lifecycle
// ============================================================

static inline arrow_udf::ArrowUDF* unwrap_udf(S2GeogArrowUDF* p) {
  return reinterpret_cast<arrow_udf::ArrowUDF*>(p);
}

void s2geog_arrow_udf_destroy(S2GeogArrowUDF* udf) {
  delete unwrap_udf(udf);
}

int s2geog_arrow_udf_init(S2GeogArrowUDF* udf, struct ArrowSchema* arg_schema,
                           const char* options, struct ArrowSchema* out) {
  S2GEOG_TRY
  int rc = unwrap_udf(udf)->Init(arg_schema, options, out);
  if (rc != 0) {
    const char* err = unwrap_udf(udf)->GetLastError();
    g_last_error = err ? err : "ArrowUDF::Init failed";
    return rc;
  }
  S2GEOG_CATCH_INT
}

int s2geog_arrow_udf_execute(S2GeogArrowUDF* udf, struct ArrowArray** args,
                              int64_t n_args, struct ArrowArray* out) {
  S2GEOG_TRY
  int rc = unwrap_udf(udf)->Execute(args, n_args, out);
  if (rc != 0) {
    const char* err = unwrap_udf(udf)->GetLastError();
    g_last_error = err ? err : "ArrowUDF::Execute failed";
    return rc;
  }
  S2GEOG_CATCH_INT
}

const char* s2geog_arrow_udf_get_last_error(S2GeogArrowUDF* udf) {
  return unwrap_udf(udf)->GetLastError();
}

// ============================================================
// ArrowUDF factory functions
// ============================================================

#define S2GEOG_UDF_FACTORY(name, ns_func)                              \
  S2GeogArrowUDF* s2geog_arrow_udf_##name(void) {                     \
    S2GEOG_TRY                                                         \
    auto udf = arrow_udf::ns_func();                                   \
    return reinterpret_cast<S2GeogArrowUDF*>(udf.release());           \
    S2GEOG_CATCH_PTR                                                   \
  }

S2GEOG_UDF_FACTORY(distance, Distance)
S2GEOG_UDF_FACTORY(max_distance, MaxDistance)
S2GEOG_UDF_FACTORY(shortest_line, ShortestLine)
S2GEOG_UDF_FACTORY(closest_point, ClosestPoint)
S2GEOG_UDF_FACTORY(intersects, Intersects)
S2GEOG_UDF_FACTORY(contains, Contains)
S2GEOG_UDF_FACTORY(equals, Equals)
S2GEOG_UDF_FACTORY(length, Length)
S2GEOG_UDF_FACTORY(area, Area)
S2GEOG_UDF_FACTORY(perimeter, Perimeter)
S2GEOG_UDF_FACTORY(centroid, Centroid)
S2GEOG_UDF_FACTORY(convex_hull, ConvexHull)
S2GEOG_UDF_FACTORY(point_on_surface, PointOnSurface)
S2GEOG_UDF_FACTORY(difference, Difference)
S2GEOG_UDF_FACTORY(sym_difference, SymDifference)
S2GEOG_UDF_FACTORY(intersection, Intersection)
S2GEOG_UDF_FACTORY(udf_union, Union)
S2GEOG_UDF_FACTORY(line_interpolate_point, LineInterpolatePoint)
S2GEOG_UDF_FACTORY(line_locate_point, LineLocatePoint)

#undef S2GEOG_UDF_FACTORY

// ============================================================
// GeoArrow IO
// ============================================================

void s2geog_geography_array_free(S2GeogGeography** arr, int64_t n) {
  if (arr) {
    for (int64_t i = 0; i < n; i++) {
      s2geog_geography_destroy(arr[i]);
    }
    free(arr);
  }
}

S2GeogGeoArrowReader* s2geog_geoarrow_reader_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogGeoArrowReader*>(new geoarrow::Reader());
  S2GEOG_CATCH_PTR
}

void s2geog_geoarrow_reader_destroy(S2GeogGeoArrowReader* reader) {
  delete reinterpret_cast<geoarrow::Reader*>(reader);
}

int s2geog_geoarrow_reader_init(S2GeogGeoArrowReader* reader,
                                 const struct ArrowSchema* schema) {
  S2GEOG_TRY
  reinterpret_cast<geoarrow::Reader*>(reader)->Init(schema);
  S2GEOG_CATCH_INT
}

int s2geog_geoarrow_reader_read(S2GeogGeoArrowReader* reader,
                                 const struct ArrowArray* array, int64_t offset,
                                 int64_t length, S2GeogGeography*** out,
                                 int64_t* n_out) {
  S2GEOG_TRY
  std::vector<std::unique_ptr<Geography>> geogs;
  reinterpret_cast<geoarrow::Reader*>(reader)->ReadGeography(array, offset,
                                                              length, &geogs);
  int64_t n = static_cast<int64_t>(geogs.size());
  S2GeogGeography** arr =
      static_cast<S2GeogGeography**>(malloc(sizeof(S2GeogGeography*) * n));
  for (int64_t i = 0; i < n; i++) {
    arr[i] = wrap_geog(std::move(geogs[i]));
  }
  *out = arr;
  *n_out = n;
  S2GEOG_CATCH_INT
}

S2GeogGeoArrowWriter* s2geog_geoarrow_writer_new(void) {
  S2GEOG_TRY
  return reinterpret_cast<S2GeogGeoArrowWriter*>(new geoarrow::Writer());
  S2GEOG_CATCH_PTR
}

void s2geog_geoarrow_writer_destroy(S2GeogGeoArrowWriter* writer) {
  delete reinterpret_cast<geoarrow::Writer*>(writer);
}

int s2geog_geoarrow_writer_init(S2GeogGeoArrowWriter* writer,
                                 const struct ArrowSchema* schema) {
  S2GEOG_TRY
  reinterpret_cast<geoarrow::Writer*>(writer)->Init(schema);
  S2GEOG_CATCH_INT
}

int s2geog_geoarrow_writer_write_geography(S2GeogGeoArrowWriter* writer,
                                            const S2GeogGeography* geog) {
  S2GEOG_TRY
  reinterpret_cast<geoarrow::Writer*>(writer)->WriteGeography(*unwrap(geog));
  S2GEOG_CATCH_INT
}

int s2geog_geoarrow_writer_write_null(S2GeogGeoArrowWriter* writer) {
  S2GEOG_TRY
  reinterpret_cast<geoarrow::Writer*>(writer)->WriteNull();
  S2GEOG_CATCH_INT
}

int s2geog_geoarrow_writer_finish(S2GeogGeoArrowWriter* writer,
                                   struct ArrowArray* out) {
  S2GEOG_TRY
  reinterpret_cast<geoarrow::Writer*>(writer)->Finish(out);
  S2GEOG_CATCH_INT
}

const char* s2geog_geoarrow_version(void) {
  return geoarrow::version();
}

// ============================================================
// Projections
// ============================================================

struct S2GeogProjectionImpl {
  std::shared_ptr<S2::Projection> proj;
};

S2GeogProjection* s2geog_projection_lnglat(void) {
  S2GEOG_TRY
  auto* impl = new S2GeogProjectionImpl{lnglat()};
  return reinterpret_cast<S2GeogProjection*>(impl);
  S2GEOG_CATCH_PTR
}

S2GeogProjection* s2geog_projection_pseudo_mercator(void) {
  S2GEOG_TRY
  auto* impl = new S2GeogProjectionImpl{pseudo_mercator()};
  return reinterpret_cast<S2GeogProjection*>(impl);
  S2GEOG_CATCH_PTR
}

void s2geog_projection_destroy(S2GeogProjection* proj) {
  delete reinterpret_cast<S2GeogProjectionImpl*>(proj);
}
