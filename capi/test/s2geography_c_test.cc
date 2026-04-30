#include "s2geography_c.h"

#include <cmath>
#include <cstring>

#include <gtest/gtest.h>

TEST(ErrorHandling, NoErrorInitially) {
  EXPECT_EQ(s2geog_last_error(), nullptr);
}

TEST(GeographyLifecycle, CreateFromWktAndInspect) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  ASSERT_NE(reader, nullptr);

  S2GeogGeography* geog = s2geog_wkt_reader_read(reader, "POINT (1 2)", -1);
  ASSERT_NE(geog, nullptr);

  EXPECT_EQ(s2geog_geography_kind(geog), 1);  // POINT
  EXPECT_EQ(s2geog_geography_dimension(geog), 0);
  EXPECT_EQ(s2geog_geography_num_shapes(geog), 1);

  int empty = -1;
  EXPECT_EQ(s2geog_geography_is_empty(geog, &empty), 0);
  EXPECT_EQ(empty, 0);

  s2geog_geography_destroy(geog);
  s2geog_wkt_reader_destroy(reader);
}

TEST(GeographyLifecycle, EmptyPoint) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* geog = s2geog_wkt_reader_read(reader, "POINT EMPTY", -1);
  ASSERT_NE(geog, nullptr);

  int empty = -1;
  EXPECT_EQ(s2geog_geography_is_empty(geog, &empty), 0);
  EXPECT_EQ(empty, 1);

  s2geog_geography_destroy(geog);
  s2geog_wkt_reader_destroy(reader);
}

TEST(WktRoundTrip, PointRoundTrip) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogWKTWriter* writer = s2geog_wkt_writer_new(6);
  ASSERT_NE(writer, nullptr);

  S2GeogGeography* geog = s2geog_wkt_reader_read(reader, "POINT (1 2)", -1);
  ASSERT_NE(geog, nullptr);

  char* wkt = s2geog_wkt_writer_write(writer, geog);
  ASSERT_NE(wkt, nullptr);
  EXPECT_STREQ(wkt, "POINT (1 2)");

  s2geog_string_free(wkt);
  s2geog_geography_destroy(geog);
  s2geog_wkt_writer_destroy(writer);
  s2geog_wkt_reader_destroy(reader);
}

TEST(WktRoundTrip, PolygonRoundTrip) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogWKTWriter* writer = s2geog_wkt_writer_new(1);

  S2GeogGeography* geog = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", -1);
  ASSERT_NE(geog, nullptr);
  EXPECT_EQ(s2geog_geography_kind(geog), 3);  // POLYGON
  EXPECT_EQ(s2geog_geography_dimension(geog), 2);

  char* wkt = s2geog_wkt_writer_write(writer, geog);
  ASSERT_NE(wkt, nullptr);
  // S2 may reorder vertices; just check it starts with POLYGON
  EXPECT_TRUE(strncmp(wkt, "POLYGON", 7) == 0);

  s2geog_string_free(wkt);
  s2geog_geography_destroy(geog);
  s2geog_wkt_writer_destroy(writer);
  s2geog_wkt_reader_destroy(reader);
}

TEST(ErrorHandling, InvalidWkt) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* geog = s2geog_wkt_reader_read(reader, "NOT VALID WKT", -1);
  EXPECT_EQ(geog, nullptr);
  EXPECT_NE(s2geog_last_error(), nullptr);

  s2geog_wkt_reader_destroy(reader);
}

TEST(Accessors, AreaLengthPerimeter) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();

  S2GeogGeography* poly = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1);
  ASSERT_NE(poly, nullptr);

  double area = -1;
  EXPECT_EQ(s2geog_area(poly, &area), 0);
  EXPECT_GT(area, 0);

  double perimeter = -1;
  EXPECT_EQ(s2geog_perimeter(poly, &perimeter), 0);
  EXPECT_GT(perimeter, 0);

  S2GeogGeography* line = s2geog_wkt_reader_read(
      reader, "LINESTRING (0 0, 1 0)", -1);
  double length = -1;
  EXPECT_EQ(s2geog_length(line, &length), 0);
  EXPECT_GT(length, 0);

  s2geog_geography_destroy(line);
  s2geog_geography_destroy(poly);
  s2geog_wkt_reader_destroy(reader);
}

TEST(Accessors, PointXY) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* pt = s2geog_wkt_reader_read(reader, "POINT (30 10)", -1);

  double x = 0, y = 0;
  EXPECT_EQ(s2geog_x(pt, &x), 0);
  EXPECT_EQ(s2geog_y(pt, &y), 0);
  EXPECT_NEAR(x, 30.0, 1e-6);
  EXPECT_NEAR(y, 10.0, 1e-6);

  int np = 0;
  EXPECT_EQ(s2geog_num_points(pt, &np), 0);
  EXPECT_EQ(np, 1);

  int is_coll = -1;
  EXPECT_EQ(s2geog_is_collection(pt, &is_coll), 0);
  EXPECT_EQ(is_coll, 0);

  s2geog_geography_destroy(pt);
  s2geog_wkt_reader_destroy(reader);
}

TEST(ShapeIndex, CreateAndPredicates) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* g1 = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1);
  S2GeogGeography* g2 = s2geog_wkt_reader_read(reader, "POINT (5 5)", -1);
  S2GeogGeography* g3 = s2geog_wkt_reader_read(reader, "POINT (20 20)", -1);

  S2GeogShapeIndex* i1 = s2geog_shape_index_new(g1);
  S2GeogShapeIndex* i2 = s2geog_shape_index_new(g2);
  S2GeogShapeIndex* i3 = s2geog_shape_index_new(g3);
  ASSERT_NE(i1, nullptr);
  ASSERT_NE(i2, nullptr);
  ASSERT_NE(i3, nullptr);

  int result = -1;
  EXPECT_EQ(s2geog_intersects(i1, i2, &result), 0);
  EXPECT_EQ(result, 1);

  EXPECT_EQ(s2geog_intersects(i1, i3, &result), 0);
  EXPECT_EQ(result, 0);

  EXPECT_EQ(s2geog_contains(i1, i2, &result), 0);
  EXPECT_EQ(result, 1);

  EXPECT_EQ(s2geog_contains(i1, i3, &result), 0);
  EXPECT_EQ(result, 0);

  s2geog_shape_index_destroy(i3);
  s2geog_shape_index_destroy(i2);
  s2geog_shape_index_destroy(i1);
  s2geog_geography_destroy(g3);
  s2geog_geography_destroy(g2);
  s2geog_geography_destroy(g1);
  s2geog_wkt_reader_destroy(reader);
}

TEST(Distance, PointDistance) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* g1 = s2geog_wkt_reader_read(reader, "POINT (0 0)", -1);
  S2GeogGeography* g2 = s2geog_wkt_reader_read(reader, "POINT (1 0)", -1);

  S2GeogShapeIndex* i1 = s2geog_shape_index_new(g1);
  S2GeogShapeIndex* i2 = s2geog_shape_index_new(g2);

  double dist = -1;
  EXPECT_EQ(s2geog_distance(i1, i2, &dist), 0);
  EXPECT_GT(dist, 0);

  double max_dist = -1;
  EXPECT_EQ(s2geog_max_distance(i1, i2, &max_dist), 0);
  EXPECT_GE(max_dist, dist);

  s2geog_shape_index_destroy(i2);
  s2geog_shape_index_destroy(i1);
  s2geog_geography_destroy(g2);
  s2geog_geography_destroy(g1);
  s2geog_wkt_reader_destroy(reader);
}

TEST(GeometryOps, CentroidBoundaryConvexHull) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* poly = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1);

  S2GeogGeography* centroid = s2geog_centroid(poly);
  ASSERT_NE(centroid, nullptr);
  EXPECT_EQ(s2geog_geography_kind(centroid), 1);  // POINT

  S2GeogGeography* boundary = s2geog_boundary(poly);
  ASSERT_NE(boundary, nullptr);

  S2GeogGeography* hull = s2geog_convex_hull(poly);
  ASSERT_NE(hull, nullptr);
  EXPECT_EQ(s2geog_geography_kind(hull), 3);  // POLYGON

  s2geog_geography_destroy(hull);
  s2geog_geography_destroy(boundary);
  s2geog_geography_destroy(centroid);
  s2geog_geography_destroy(poly);
  s2geog_wkt_reader_destroy(reader);
}

TEST(BooleanOps, IntersectionUnion) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* g1 = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1);
  S2GeogGeography* g2 = s2geog_wkt_reader_read(
      reader, "POLYGON ((5 5, 15 5, 15 15, 5 15, 5 5))", -1);

  S2GeogShapeIndex* i1 = s2geog_shape_index_new(g1);
  S2GeogShapeIndex* i2 = s2geog_shape_index_new(g2);

  S2GeogGeography* inter = s2geog_intersection(i1, i2);
  ASSERT_NE(inter, nullptr);

  S2GeogGeography* un = s2geog_union(i1, i2);
  ASSERT_NE(un, nullptr);

  S2GeogGeography* diff = s2geog_difference(i1, i2);
  ASSERT_NE(diff, nullptr);

  S2GeogGeography* sym = s2geog_sym_difference(i1, i2);
  ASSERT_NE(sym, nullptr);

  double a_inter, a_union, a_g1, a_g2;
  s2geog_area(inter, &a_inter);
  s2geog_area(un, &a_union);
  s2geog_area(g1, &a_g1);
  s2geog_area(g2, &a_g2);
  EXPECT_NEAR(a_g1 + a_g2 - a_inter, a_union, a_union * 0.01);

  s2geog_geography_destroy(sym);
  s2geog_geography_destroy(diff);
  s2geog_geography_destroy(un);
  s2geog_geography_destroy(inter);
  s2geog_shape_index_destroy(i2);
  s2geog_shape_index_destroy(i1);
  s2geog_geography_destroy(g2);
  s2geog_geography_destroy(g1);
  s2geog_wkt_reader_destroy(reader);
}

TEST(WkbIO, RoundTrip) {
  S2GeogWKTReader* wkt_reader = s2geog_wkt_reader_new();
  S2GeogGeography* geog = s2geog_wkt_reader_read(wkt_reader, "POINT (1 2)", -1);

  S2GeogWKBWriter* wkb_writer = s2geog_wkb_writer_new();
  ASSERT_NE(wkb_writer, nullptr);
  uint8_t* bytes = nullptr;
  int64_t size = 0;
  EXPECT_EQ(s2geog_wkb_writer_write(wkb_writer, geog, &bytes, &size), 0);
  EXPECT_GT(size, 0);
  EXPECT_NE(bytes, nullptr);

  S2GeogWKBReader* wkb_reader = s2geog_wkb_reader_new();
  S2GeogGeography* geog2 = s2geog_wkb_reader_read(wkb_reader, bytes, size);
  ASSERT_NE(geog2, nullptr);
  EXPECT_EQ(s2geog_geography_kind(geog2), 1);  // POINT

  s2geog_bytes_free(bytes);
  s2geog_geography_destroy(geog2);
  s2geog_wkb_reader_destroy(wkb_reader);
  s2geog_geography_destroy(geog);
  s2geog_wkb_writer_destroy(wkb_writer);
  s2geog_wkt_reader_destroy(wkt_reader);
}

TEST(Coverings, BasicCovering) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* poly = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1);

  uint64_t* cell_ids = nullptr;
  int64_t n = 0;
  EXPECT_EQ(s2geog_covering(poly, 8, &cell_ids, &n), 0);
  EXPECT_GT(n, 0);
  EXPECT_NE(cell_ids, nullptr);

  s2geog_cell_ids_free(cell_ids);
  s2geog_geography_destroy(poly);
  s2geog_wkt_reader_destroy(reader);
}

TEST(LinearRef, InterpolateLocate) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeography* line = s2geog_wkt_reader_read(
      reader, "LINESTRING (0 0, 0 10)", -1);
  S2GeogGeography* pt = s2geog_wkt_reader_read(reader, "POINT (0 5)", -1);

  double frac = -1;
  EXPECT_EQ(s2geog_project_normalized(line, pt, &frac), 0);
  EXPECT_NEAR(frac, 0.5, 0.05);

  S2GeogGeography* interp = s2geog_interpolate_normalized(line, 0.5);
  ASSERT_NE(interp, nullptr);
  EXPECT_EQ(s2geog_geography_kind(interp), 1);

  s2geog_geography_destroy(interp);
  s2geog_geography_destroy(pt);
  s2geog_geography_destroy(line);
  s2geog_wkt_reader_destroy(reader);
}

TEST(OpCell, BasicOps) {
  double point[3];
  double lnglat[2] = {-73.9857, 40.7484};  // NYC
  s2geog_op_point_to_point(lnglat, point);
  uint64_t cell_id = 0;
  EXPECT_EQ(s2geog_op_cell_from_point(point, &cell_id), 0);
  EXPECT_NE(cell_id, 0u);

  int valid = 0;
  EXPECT_EQ(s2geog_op_cell_is_valid(cell_id, &valid), 0);
  EXPECT_EQ(valid, 1);

  int8_t level = -1;
  EXPECT_EQ(s2geog_op_cell_level(cell_id, &level), 0);
  EXPECT_EQ(level, 30);

  double area = 0;
  EXPECT_EQ(s2geog_op_cell_area(cell_id, &area), 0);
  EXPECT_GT(area, 0);

  char token[32];
  EXPECT_EQ(s2geog_op_cell_to_token(cell_id, token, sizeof(token)), 0);
  uint64_t cell_back = 0;
  EXPECT_EQ(s2geog_op_cell_from_token(token, &cell_back), 0);
  EXPECT_EQ(cell_id, cell_back);

  uint64_t parent = 0;
  EXPECT_EQ(s2geog_op_cell_parent(cell_id, 10, &parent), 0);
  EXPECT_NE(parent, 0u);

  int8_t parent_level = -1;
  EXPECT_EQ(s2geog_op_cell_level(parent, &parent_level), 0);
  EXPECT_EQ(parent_level, 10);
}

TEST(OpPoint, LngLatRoundTrip) {
  double lnglat_in[2] = {45.0, 30.0};
  double point[3];
  double lnglat_out[2];

  s2geog_op_point_to_point(lnglat_in, point);
  s2geog_op_point_to_lnglat(point, lnglat_out);

  EXPECT_NEAR(lnglat_out[0], 45.0, 1e-10);
  EXPECT_NEAR(lnglat_out[1], 30.0, 1e-10);
}

TEST(Aggregators, CentroidAggregator) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogCentroidAggregator* agg = s2geog_centroid_aggregator_new();
  ASSERT_NE(agg, nullptr);

  S2GeogGeography* p1 = s2geog_wkt_reader_read(reader, "POINT (0 0)", -1);
  S2GeogGeography* p2 = s2geog_wkt_reader_read(reader, "POINT (0 10)", -1);
  EXPECT_EQ(s2geog_centroid_aggregator_add(agg, p1), 0);
  EXPECT_EQ(s2geog_centroid_aggregator_add(agg, p2), 0);

  S2GeogGeography* result = s2geog_centroid_aggregator_finalize(agg);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(s2geog_geography_kind(result), 1);  // POINT

  s2geog_geography_destroy(result);
  s2geog_centroid_aggregator_destroy(agg);
  s2geog_geography_destroy(p2);
  s2geog_geography_destroy(p1);
  s2geog_wkt_reader_destroy(reader);
}

TEST(Aggregators, UnionAggregator) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogUnionAggregator* agg = s2geog_union_aggregator_new();
  ASSERT_NE(agg, nullptr);

  S2GeogGeography* g1 = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))", -1);
  S2GeogGeography* g2 = s2geog_wkt_reader_read(
      reader, "POLYGON ((3 3, 8 3, 8 8, 3 8, 3 3))", -1);
  EXPECT_EQ(s2geog_union_aggregator_add(agg, g1), 0);
  EXPECT_EQ(s2geog_union_aggregator_add(agg, g2), 0);

  S2GeogGeography* result = s2geog_union_aggregator_finalize(agg);
  ASSERT_NE(result, nullptr);

  s2geog_geography_destroy(result);
  s2geog_union_aggregator_destroy(agg);
  s2geog_geography_destroy(g2);
  s2geog_geography_destroy(g1);
  s2geog_wkt_reader_destroy(reader);
}

TEST(GeographyIndex, QueryReturnsCandidate) {
  S2GeogWKTReader* reader = s2geog_wkt_reader_new();
  S2GeogGeographyIndex* index = s2geog_geography_index_new();
  ASSERT_NE(index, nullptr);

  S2GeogGeography* g0 = s2geog_wkt_reader_read(
      reader, "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))", -1);
  S2GeogGeography* g1 = s2geog_wkt_reader_read(
      reader, "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))", -1);

  EXPECT_EQ(s2geog_geography_index_add(index, g0, 0), 0);
  EXPECT_EQ(s2geog_geography_index_add(index, g1, 1), 0);

  S2GeogGeography* query = s2geog_wkt_reader_read(reader, "POINT (2 2)", -1);
  int32_t* results = nullptr;
  int64_t n_results = 0;
  EXPECT_EQ(s2geog_geography_index_query(index, query, &results, &n_results), 0);
  EXPECT_GT(n_results, 0);
  bool found_0 = false;
  for (int64_t i = 0; i < n_results; i++) {
    if (results[i] == 0) found_0 = true;
  }
  EXPECT_TRUE(found_0);

  s2geog_int32_free(results);
  s2geog_geography_destroy(query);
  s2geog_geography_destroy(g1);
  s2geog_geography_destroy(g0);
  s2geog_geography_index_destroy(index);
  s2geog_wkt_reader_destroy(reader);
}

TEST(ArrowUDF, FactoryFunctionsReturnNonNull) {
  S2GeogArrowUDF* udf;

  udf = s2geog_arrow_udf_distance();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_max_distance();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_intersects();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_contains();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_equals();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_length();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_area();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_centroid();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_convex_hull();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_intersection();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);

  udf = s2geog_arrow_udf_udf_union();
  ASSERT_NE(udf, nullptr);
  s2geog_arrow_udf_destroy(udf);
}

TEST(GeoArrowIO, VersionNotNull) {
  const char* ver = s2geog_geoarrow_version();
  ASSERT_NE(ver, nullptr);
  EXPECT_GT(strlen(ver), 0u);
}

TEST(GeoArrowIO, ReaderWriterLifecycle) {
  S2GeogGeoArrowReader* reader = s2geog_geoarrow_reader_new();
  ASSERT_NE(reader, nullptr);
  s2geog_geoarrow_reader_destroy(reader);

  S2GeogGeoArrowWriter* writer = s2geog_geoarrow_writer_new();
  ASSERT_NE(writer, nullptr);
  s2geog_geoarrow_writer_destroy(writer);
}

TEST(GeoArrowIO, GeographyArrayFreeHandlesNull) {
  s2geog_geography_array_free(nullptr, 0);
}

TEST(Projections, LngLatCreateAndDestroy) {
  S2GeogProjection* proj = s2geog_projection_lnglat();
  ASSERT_NE(proj, nullptr);
  s2geog_projection_destroy(proj);
}

TEST(Projections, PseudoMercatorCreateAndDestroy) {
  S2GeogProjection* proj = s2geog_projection_pseudo_mercator();
  ASSERT_NE(proj, nullptr);
  s2geog_projection_destroy(proj);
}
