#ifndef S2GEOGRAPHY_C_H
#define S2GEOGRAPHY_C_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Arrow C Data Interface (forward declarations)
// ============================================================

// These match the canonical Arrow C Data Interface structs.
// If arrow_abi.h is already included, these are redundant but harmless
// because they're just forward declarations.
#ifndef ARROW_C_DATA_INTERFACE
struct ArrowSchema;
struct ArrowArray;
#endif

// ============================================================
// Error handling
// ============================================================

/// Returns the last error message, or NULL if no error.
/// The returned string is valid until the next s2geog_* call on the same thread.
const char* s2geog_last_error(void);

// ============================================================
// Opaque types
// ============================================================

typedef struct S2GeogGeography S2GeogGeography;
typedef struct S2GeogShapeIndex S2GeogShapeIndex;
typedef struct S2GeogGeographyIndex S2GeogGeographyIndex;

typedef struct S2GeogWKTReader S2GeogWKTReader;
typedef struct S2GeogWKTWriter S2GeogWKTWriter;
typedef struct S2GeogWKBReader S2GeogWKBReader;
typedef struct S2GeogWKBWriter S2GeogWKBWriter;
typedef struct S2GeogGeoArrowReader S2GeogGeoArrowReader;
typedef struct S2GeogGeoArrowWriter S2GeogGeoArrowWriter;

typedef struct S2GeogCentroidAggregator S2GeogCentroidAggregator;
typedef struct S2GeogConvexHullAggregator S2GeogConvexHullAggregator;
typedef struct S2GeogRebuildAggregator S2GeogRebuildAggregator;
typedef struct S2GeogCoverageUnionAggregator S2GeogCoverageUnionAggregator;
typedef struct S2GeogUnionAggregator S2GeogUnionAggregator;

typedef struct S2GeogArrowUDF S2GeogArrowUDF;
typedef struct S2GeogProjection S2GeogProjection;

// ============================================================
// Memory management helpers
// ============================================================

/// Free a string returned by s2geog_wkt_writer_write.
void s2geog_string_free(char* str);

/// Free a byte array returned by s2geog_wkb_writer_write.
void s2geog_bytes_free(uint8_t* bytes);

/// Free a cell ID array returned by s2geog_covering / s2geog_interior_covering.
void s2geog_cell_ids_free(uint64_t* cell_ids);

/// Free an int32 array returned by s2geog_geography_index_query.
void s2geog_int32_free(int32_t* ptr);

/// Destroy each geography in the array, then free the array itself.
void s2geog_geography_array_free(S2GeogGeography** arr, int64_t n);

// ============================================================
// Geography lifecycle
// ============================================================

/// Destroy a geography. Safe to call with NULL.
void s2geog_geography_destroy(S2GeogGeography* geog);

/// Returns the GeographyKind enum value (1=POINT, 2=POLYLINE, 3=POLYGON, etc.)
int s2geog_geography_kind(const S2GeogGeography* geog);

/// Returns the dimension (0=point, 1=line, 2=polygon, -1=mixed/empty)
int s2geog_geography_dimension(const S2GeogGeography* geog);

/// Returns the number of S2Shape objects
int s2geog_geography_num_shapes(const S2GeogGeography* geog);

/// Check if empty. Sets *out to 1 if empty, 0 otherwise.
int s2geog_geography_is_empty(const S2GeogGeography* geog, int* out);

// ============================================================
// Geometry construction from raw coordinates
// ============================================================
// All coordinate arrays are BORROWED — the caller retains ownership.
// lnglat arrays: interleaved [lng0, lat0, lng1, lat1, ...] in degrees.
// xyz arrays: interleaved [x0, y0, z0, x1, y1, z1, ...] unit-sphere coords.
// Returned geographies are owned by the caller (free with s2geog_geography_destroy).

/// Create a single-point geography from (lng, lat) in degrees.
S2GeogGeography* s2geog_make_point_lnglat(double lng, double lat);

/// Create a single-point geography from a unit-sphere XYZ vector.
S2GeogGeography* s2geog_make_point_xyz(double x, double y, double z);

/// Create a multi-point geography from n interleaved (lng, lat) pairs.
S2GeogGeography* s2geog_make_multipoint_lnglat(const double* lnglat, int64_t n);

/// Create a multi-point geography from n interleaved (x, y, z) triples.
S2GeogGeography* s2geog_make_multipoint_xyz(const double* xyz, int64_t n);

/// Create a polyline geography from n interleaved (lng, lat) pairs.
S2GeogGeography* s2geog_make_polyline_lnglat(const double* lnglat, int64_t n);

/// Create a polyline geography from n interleaved (x, y, z) triples.
S2GeogGeography* s2geog_make_polyline_xyz(const double* xyz, int64_t n);

/// Create a polygon geography from interleaved (lng, lat) pairs.
/// ring_offsets: array of n_rings+1 offsets into the coordinate array
///   (e.g., [0, 4, 7] means ring 0 has coords[0..3], ring 1 has coords[4..6]).
/// First ring is the outer shell; subsequent rings are holes.
S2GeogGeography* s2geog_make_polygon_lnglat(const double* lnglat,
                                             const int64_t* ring_offsets,
                                             int64_t n_rings);

/// Create a polygon geography from interleaved (x, y, z) triples.
/// ring_offsets: same semantics as _lnglat variant.
S2GeogGeography* s2geog_make_polygon_xyz(const double* xyz,
                                          const int64_t* ring_offsets,
                                          int64_t n_rings);

/// Create a geography collection from an array of existing geographies.
/// The collection takes OWNERSHIP of each geog in the array.
/// After this call, the caller must NOT destroy the individual geographies —
/// only destroy the returned collection.
/// The geogs array pointer itself remains caller-owned.
S2GeogGeography* s2geog_make_collection(S2GeogGeography** geogs, int64_t n);

// ============================================================
// WKT IO
// ============================================================

S2GeogWKTReader* s2geog_wkt_reader_new(void);
void s2geog_wkt_reader_destroy(S2GeogWKTReader* reader);

/// Read a geography from WKT. Pass size=-1 to auto-detect length via strlen.
/// Returns NULL on error (check s2geog_last_error()).
S2GeogGeography* s2geog_wkt_reader_read(S2GeogWKTReader* reader,
                                         const char* wkt, int64_t size);

/// Create a WKT writer with the given decimal precision.
S2GeogWKTWriter* s2geog_wkt_writer_new(int precision);
void s2geog_wkt_writer_destroy(S2GeogWKTWriter* writer);

/// Write geography to WKT. Caller must free result with s2geog_string_free().
char* s2geog_wkt_writer_write(S2GeogWKTWriter* writer,
                               const S2GeogGeography* geog);

// ============================================================
// WKB IO
// ============================================================

S2GeogWKBReader* s2geog_wkb_reader_new(void);
void s2geog_wkb_reader_destroy(S2GeogWKBReader* reader);

/// Read geography from WKB bytes. Returns NULL on error.
S2GeogGeography* s2geog_wkb_reader_read(S2GeogWKBReader* reader,
                                          const uint8_t* bytes, int64_t size);

S2GeogWKBWriter* s2geog_wkb_writer_new(void);
void s2geog_wkb_writer_destroy(S2GeogWKBWriter* writer);

/// Write geography to WKB. Caller must free *out with s2geog_bytes_free().
int s2geog_wkb_writer_write(S2GeogWKBWriter* writer,
                             const S2GeogGeography* geog, uint8_t** out,
                             int64_t* out_size);

// ============================================================
// ShapeIndex (prepared geometry)
// ============================================================

/// Create a ShapeIndex from a geography (builds the index).
/// Returns NULL on error.
S2GeogShapeIndex* s2geog_shape_index_new(const S2GeogGeography* geog);
void s2geog_shape_index_destroy(S2GeogShapeIndex* idx);

// ============================================================
// Scalar accessors
// ============================================================

int s2geog_area(const S2GeogGeography* geog, double* out);
int s2geog_length(const S2GeogGeography* geog, double* out);
int s2geog_perimeter(const S2GeogGeography* geog, double* out);
int s2geog_x(const S2GeogGeography* geog, double* out);
int s2geog_y(const S2GeogGeography* geog, double* out);
int s2geog_num_points(const S2GeogGeography* geog, int* out);
int s2geog_is_collection(const S2GeogGeography* geog, int* out);
int s2geog_find_validation_error(const S2GeogGeography* geog, char* buf,
                                  int64_t buf_size, int* out);

// ============================================================
// Predicates (operate on ShapeIndex)
// ============================================================

int s2geog_intersects(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                      int* out);
int s2geog_equals(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                  int* out);
int s2geog_contains(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                    int* out);
int s2geog_touches(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                   int* out);

// ============================================================
// Distance (operate on ShapeIndex)
// ============================================================

int s2geog_distance(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                    double* out);
int s2geog_max_distance(const S2GeogShapeIndex* a, const S2GeogShapeIndex* b,
                        double* out);

/// Returns a PointGeography. Caller owns the result.
S2GeogGeography* s2geog_closest_point(const S2GeogShapeIndex* a,
                                       const S2GeogShapeIndex* b);

/// Returns a PolylineGeography (2-point line). Caller owns the result.
S2GeogGeography* s2geog_minimum_clearance_line_between(
    const S2GeogShapeIndex* a, const S2GeogShapeIndex* b);

// ============================================================
// Geometry-returning operations
// ============================================================

/// Returns a PointGeography representing the centroid. Caller owns result.
S2GeogGeography* s2geog_centroid(const S2GeogGeography* geog);
S2GeogGeography* s2geog_boundary(const S2GeogGeography* geog);
S2GeogGeography* s2geog_convex_hull(const S2GeogGeography* geog);

// ============================================================
// Boolean operations (operate on ShapeIndex)
// ============================================================

S2GeogGeography* s2geog_intersection(const S2GeogShapeIndex* a,
                                      const S2GeogShapeIndex* b);
S2GeogGeography* s2geog_union(const S2GeogShapeIndex* a,
                               const S2GeogShapeIndex* b);
S2GeogGeography* s2geog_difference(const S2GeogShapeIndex* a,
                                    const S2GeogShapeIndex* b);
S2GeogGeography* s2geog_sym_difference(const S2GeogShapeIndex* a,
                                        const S2GeogShapeIndex* b);
S2GeogGeography* s2geog_unary_union(const S2GeogShapeIndex* geog);

/// Rebuild geometry using S2Builder with default options.
S2GeogGeography* s2geog_rebuild(const S2GeogGeography* geog);
S2GeogGeography* s2geog_build_point(const S2GeogGeography* geog);
S2GeogGeography* s2geog_build_polyline(const S2GeogGeography* geog);
S2GeogGeography* s2geog_build_polygon(const S2GeogGeography* geog);

// ============================================================
// Coverings
// ============================================================

/// Compute a covering. Caller must free *cell_ids_out with s2geog_cell_ids_free().
int s2geog_covering(const S2GeogGeography* geog, int max_cells,
                    uint64_t** cell_ids_out, int64_t* n_out);
int s2geog_interior_covering(const S2GeogGeography* geog, int max_cells,
                              uint64_t** cell_ids_out, int64_t* n_out);

// ============================================================
// Linear referencing
// ============================================================

int s2geog_project_normalized(const S2GeogGeography* geog1,
                               const S2GeogGeography* geog2, double* out);
S2GeogGeography* s2geog_interpolate_normalized(const S2GeogGeography* geog,
                                                double distance_norm);

// ============================================================
// Op/Point
// ============================================================

/// Convert XYZ unit vector to (lng, lat) degrees.
void s2geog_op_point_to_lnglat(const double point[3], double lnglat_out[2]);

/// Convert (lng, lat) degrees to XYZ unit vector.
void s2geog_op_point_to_point(const double lnglat[2], double point_out[3]);

// ============================================================
// Op/Cell
// ============================================================

int s2geog_op_cell_from_token(const char* token, uint64_t* out);
int s2geog_op_cell_from_debug_string(const char* debug_str, uint64_t* out);
int s2geog_op_cell_from_point(const double point[3], uint64_t* out);
int s2geog_op_cell_to_point(uint64_t cell_id, double point_out[3]);
int s2geog_op_cell_to_token(uint64_t cell_id, char* buf, int64_t buf_size);
int s2geog_op_cell_to_debug_string(uint64_t cell_id, char* buf,
                                    int64_t buf_size);
int s2geog_op_cell_is_valid(uint64_t cell_id, int* out);
int s2geog_op_cell_center(uint64_t cell_id, double point_out[3]);
int s2geog_op_cell_vertex(uint64_t cell_id, int8_t vertex_id,
                           double point_out[3]);
int s2geog_op_cell_level(uint64_t cell_id, int8_t* out);
int s2geog_op_cell_area(uint64_t cell_id, double* out);
int s2geog_op_cell_area_approx(uint64_t cell_id, double* out);
int s2geog_op_cell_parent(uint64_t cell_id, int8_t level, uint64_t* out);
int s2geog_op_cell_child(uint64_t cell_id, int8_t k, uint64_t* out);
int s2geog_op_cell_edge_neighbor(uint64_t cell_id, int8_t k, uint64_t* out);
int s2geog_op_cell_contains(uint64_t cell_id, uint64_t cell_id_test, int* out);
int s2geog_op_cell_may_intersect(uint64_t cell_id, uint64_t cell_id_test,
                                  int* out);
int s2geog_op_cell_distance(uint64_t cell_id, uint64_t cell_id_test,
                             double* out);
int s2geog_op_cell_max_distance(uint64_t cell_id, uint64_t cell_id_test,
                                 double* out);
int s2geog_op_cell_common_ancestor_level(uint64_t cell_id,
                                          uint64_t cell_id_test, int8_t* out);

// ============================================================
// Aggregators
// ============================================================

// --- CentroidAggregator ---
S2GeogCentroidAggregator* s2geog_centroid_aggregator_new(void);
void s2geog_centroid_aggregator_destroy(S2GeogCentroidAggregator* agg);
int s2geog_centroid_aggregator_add(S2GeogCentroidAggregator* agg,
                                    const S2GeogGeography* geog);
S2GeogGeography* s2geog_centroid_aggregator_finalize(
    S2GeogCentroidAggregator* agg);

// --- ConvexHullAggregator ---
S2GeogConvexHullAggregator* s2geog_convex_hull_aggregator_new(void);
void s2geog_convex_hull_aggregator_destroy(S2GeogConvexHullAggregator* agg);
int s2geog_convex_hull_aggregator_add(S2GeogConvexHullAggregator* agg,
                                       const S2GeogGeography* geog);
S2GeogGeography* s2geog_convex_hull_aggregator_finalize(
    S2GeogConvexHullAggregator* agg);

// --- RebuildAggregator ---
S2GeogRebuildAggregator* s2geog_rebuild_aggregator_new(void);
void s2geog_rebuild_aggregator_destroy(S2GeogRebuildAggregator* agg);
int s2geog_rebuild_aggregator_add(S2GeogRebuildAggregator* agg,
                                   const S2GeogGeography* geog);
S2GeogGeography* s2geog_rebuild_aggregator_finalize(
    S2GeogRebuildAggregator* agg);

// --- CoverageUnionAggregator ---
S2GeogCoverageUnionAggregator* s2geog_coverage_union_aggregator_new(void);
void s2geog_coverage_union_aggregator_destroy(
    S2GeogCoverageUnionAggregator* agg);
int s2geog_coverage_union_aggregator_add(S2GeogCoverageUnionAggregator* agg,
                                          const S2GeogGeography* geog);
S2GeogGeography* s2geog_coverage_union_aggregator_finalize(
    S2GeogCoverageUnionAggregator* agg);

// --- UnionAggregator ---
S2GeogUnionAggregator* s2geog_union_aggregator_new(void);
void s2geog_union_aggregator_destroy(S2GeogUnionAggregator* agg);
int s2geog_union_aggregator_add(S2GeogUnionAggregator* agg,
                                 const S2GeogGeography* geog);
S2GeogGeography* s2geog_union_aggregator_finalize(S2GeogUnionAggregator* agg);

// ============================================================
// GeographyIndex (spatial index like STRTree)
// ============================================================

S2GeogGeographyIndex* s2geog_geography_index_new(void);
void s2geog_geography_index_destroy(S2GeogGeographyIndex* index);

/// Add a geography to the index with an integer key.
int s2geog_geography_index_add(S2GeogGeographyIndex* index,
                                const S2GeogGeography* geog, int value);

/// Query the index for candidate matches. Caller must free *results_out
/// with s2geog_int32_free().
int s2geog_geography_index_query(S2GeogGeographyIndex* index,
                                  const S2GeogGeography* geog,
                                  int32_t** results_out, int64_t* n_out);

// ============================================================
// ArrowUDF
// ============================================================

void s2geog_arrow_udf_destroy(S2GeogArrowUDF* udf);
int s2geog_arrow_udf_init(S2GeogArrowUDF* udf, struct ArrowSchema* arg_schema,
                           const char* options, struct ArrowSchema* out);
int s2geog_arrow_udf_execute(S2GeogArrowUDF* udf, struct ArrowArray** args,
                              int64_t n_args, struct ArrowArray* out);
const char* s2geog_arrow_udf_get_last_error(S2GeogArrowUDF* udf);

// Factory functions
S2GeogArrowUDF* s2geog_arrow_udf_distance(void);
S2GeogArrowUDF* s2geog_arrow_udf_max_distance(void);
S2GeogArrowUDF* s2geog_arrow_udf_shortest_line(void);
S2GeogArrowUDF* s2geog_arrow_udf_closest_point(void);
S2GeogArrowUDF* s2geog_arrow_udf_intersects(void);
S2GeogArrowUDF* s2geog_arrow_udf_contains(void);
S2GeogArrowUDF* s2geog_arrow_udf_equals(void);
S2GeogArrowUDF* s2geog_arrow_udf_length(void);
S2GeogArrowUDF* s2geog_arrow_udf_area(void);
S2GeogArrowUDF* s2geog_arrow_udf_perimeter(void);
S2GeogArrowUDF* s2geog_arrow_udf_centroid(void);
S2GeogArrowUDF* s2geog_arrow_udf_convex_hull(void);
S2GeogArrowUDF* s2geog_arrow_udf_point_on_surface(void);
S2GeogArrowUDF* s2geog_arrow_udf_difference(void);
S2GeogArrowUDF* s2geog_arrow_udf_sym_difference(void);
S2GeogArrowUDF* s2geog_arrow_udf_intersection(void);
S2GeogArrowUDF* s2geog_arrow_udf_udf_union(void);
S2GeogArrowUDF* s2geog_arrow_udf_line_interpolate_point(void);
S2GeogArrowUDF* s2geog_arrow_udf_line_locate_point(void);

// ============================================================
// GeoArrow IO
// ============================================================

S2GeogGeoArrowReader* s2geog_geoarrow_reader_new(void);
void s2geog_geoarrow_reader_destroy(S2GeogGeoArrowReader* reader);
int s2geog_geoarrow_reader_init(S2GeogGeoArrowReader* reader,
                                 const struct ArrowSchema* schema);
int s2geog_geoarrow_reader_read(S2GeogGeoArrowReader* reader,
                                 const struct ArrowArray* array, int64_t offset,
                                 int64_t length, S2GeogGeography*** out,
                                 int64_t* n_out);

S2GeogGeoArrowWriter* s2geog_geoarrow_writer_new(void);
void s2geog_geoarrow_writer_destroy(S2GeogGeoArrowWriter* writer);
int s2geog_geoarrow_writer_init(S2GeogGeoArrowWriter* writer,
                                 const struct ArrowSchema* schema);
int s2geog_geoarrow_writer_write_geography(S2GeogGeoArrowWriter* writer,
                                            const S2GeogGeography* geog);
int s2geog_geoarrow_writer_write_null(S2GeogGeoArrowWriter* writer);
int s2geog_geoarrow_writer_finish(S2GeogGeoArrowWriter* writer,
                                   struct ArrowArray* out);

const char* s2geog_geoarrow_version(void);

// ============================================================
// Projections
// ============================================================

S2GeogProjection* s2geog_projection_lnglat(void);
S2GeogProjection* s2geog_projection_pseudo_mercator(void);
void s2geog_projection_destroy(S2GeogProjection* proj);

#ifdef __cplusplus
}
#endif

#endif  // S2GEOGRAPHY_C_H
