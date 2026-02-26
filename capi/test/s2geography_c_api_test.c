#include "s2geography_c.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_NOT_NULL(p) \
  do { \
    if (!(p)) { \
      fprintf(stderr, "FAIL %s:%d: %s is NULL\n", __FILE__, __LINE__, #p); \
      abort(); \
    } \
  } while (0)

#define ASSERT_NULL(p) \
  do { \
    if ((p)) { \
      fprintf(stderr, "FAIL %s:%d: %s is not NULL\n", __FILE__, __LINE__, #p); \
      abort(); \
    } \
  } while (0)

#define ASSERT_EQ_INT(a, b) \
  do { \
    int _a = (a), _b = (b); \
    if (_a != _b) { \
      fprintf(stderr, "FAIL %s:%d: %s == %d, expected %d\n", \
              __FILE__, __LINE__, #a, _a, _b); \
      abort(); \
    } \
  } while (0)

#define ASSERT_GT_DBL(a, b) \
  do { \
    double _a = (a), _b = (b); \
    if (!(_a > _b)) { \
      fprintf(stderr, "FAIL %s:%d: %s == %g, expected > %g\n", \
              __FILE__, __LINE__, #a, _a, _b); \
      abort(); \
    } \
  } while (0)

#define ASSERT_NEAR(a, b, tol) \
  do { \
    double _a = (a), _b = (b), _t = (tol); \
    if (fabs(_a - _b) > _t) { \
      fprintf(stderr, "FAIL %s:%d: %s == %g, expected %g (tol %g)\n", \
              __FILE__, __LINE__, #a, _a, _b, _t); \
      abort(); \
    } \
  } while (0)

#define RUN_TEST(fn) \
  do { \
    printf("  %-50s", #fn); \
    fn(); \
    printf("PASS\n"); \
    tests_passed++; \
  } while (0)

static int tests_passed = 0;

/* ============================================================
 * Construction unit tests
 * ============================================================ */

static void test_make_point_lnglat(void) {
  S2GeogGeography* pt = s2geog_make_point_lnglat(30.0, 10.0);
  ASSERT_NOT_NULL(pt);
  ASSERT_EQ_INT(s2geog_geography_kind(pt), 1);  /* POINT */
  ASSERT_EQ_INT(s2geog_geography_dimension(pt), 0);

  double x = 0, y = 0;
  ASSERT_EQ_INT(s2geog_x(pt, &x), 0);
  ASSERT_EQ_INT(s2geog_y(pt, &y), 0);
  ASSERT_NEAR(x, 30.0, 1e-6);
  ASSERT_NEAR(y, 10.0, 1e-6);

  int np = 0;
  ASSERT_EQ_INT(s2geog_num_points(pt, &np), 0);
  ASSERT_EQ_INT(np, 1);

  s2geog_geography_destroy(pt);
}

static void test_make_point_xyz(void) {
  /* Convert known lnglat to xyz, construct, verify round-trip */
  double lnglat[2] = {30.0, 10.0};
  double xyz[3];
  s2geog_op_point_to_point(lnglat, xyz);

  S2GeogGeography* pt = s2geog_make_point_xyz(xyz[0], xyz[1], xyz[2]);
  ASSERT_NOT_NULL(pt);
  ASSERT_EQ_INT(s2geog_geography_kind(pt), 1);

  double x = 0, y = 0;
  ASSERT_EQ_INT(s2geog_x(pt, &x), 0);
  ASSERT_EQ_INT(s2geog_y(pt, &y), 0);
  ASSERT_NEAR(x, 30.0, 1e-6);
  ASSERT_NEAR(y, 10.0, 1e-6);

  s2geog_geography_destroy(pt);
}

static void test_make_multipoint_lnglat(void) {
  double coords[] = {0.0, 0.0, 10.0, 10.0, 20.0, 20.0};
  S2GeogGeography* mp = s2geog_make_multipoint_lnglat(coords, 3);
  ASSERT_NOT_NULL(mp);
  ASSERT_EQ_INT(s2geog_geography_kind(mp), 1);  /* POINT (multi) */
  ASSERT_EQ_INT(s2geog_geography_dimension(mp), 0);

  int np = 0;
  ASSERT_EQ_INT(s2geog_num_points(mp, &np), 0);
  ASSERT_EQ_INT(np, 3);

  s2geog_geography_destroy(mp);
}

static void test_make_multipoint_xyz(void) {
  double lnglat0[2] = {0.0, 0.0};
  double lnglat1[2] = {10.0, 10.0};
  double xyz[6];
  s2geog_op_point_to_point(lnglat0, &xyz[0]);
  s2geog_op_point_to_point(lnglat1, &xyz[3]);

  S2GeogGeography* mp = s2geog_make_multipoint_xyz(xyz, 2);
  ASSERT_NOT_NULL(mp);

  int np = 0;
  ASSERT_EQ_INT(s2geog_num_points(mp, &np), 0);
  ASSERT_EQ_INT(np, 2);

  s2geog_geography_destroy(mp);
}

static void test_make_polyline_lnglat(void) {
  double coords[] = {0.0, 0.0, 1.0, 0.0, 1.0, 1.0};
  S2GeogGeography* line = s2geog_make_polyline_lnglat(coords, 3);
  ASSERT_NOT_NULL(line);
  ASSERT_EQ_INT(s2geog_geography_kind(line), 2);  /* POLYLINE */
  ASSERT_EQ_INT(s2geog_geography_dimension(line), 1);

  double length = 0;
  ASSERT_EQ_INT(s2geog_length(line, &length), 0);
  ASSERT_GT_DBL(length, 0.0);

  s2geog_geography_destroy(line);
}

static void test_make_polyline_xyz(void) {
  double lnglat0[2] = {0.0, 0.0};
  double lnglat1[2] = {1.0, 0.0};
  double xyz[6];
  s2geog_op_point_to_point(lnglat0, &xyz[0]);
  s2geog_op_point_to_point(lnglat1, &xyz[3]);

  S2GeogGeography* line = s2geog_make_polyline_xyz(xyz, 2);
  ASSERT_NOT_NULL(line);
  ASSERT_EQ_INT(s2geog_geography_kind(line), 2);

  double length = 0;
  ASSERT_EQ_INT(s2geog_length(line, &length), 0);
  ASSERT_GT_DBL(length, 0.0);

  s2geog_geography_destroy(line);
}

static void test_make_polygon_lnglat(void) {
  /* Square: (0,0)-(10,0)-(10,10)-(0,10) */
  double coords[] = {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0};
  int64_t ring_offsets[] = {0, 4};
  S2GeogGeography* poly = s2geog_make_polygon_lnglat(coords, ring_offsets, 1);
  ASSERT_NOT_NULL(poly);
  ASSERT_EQ_INT(s2geog_geography_kind(poly), 3);  /* POLYGON */
  ASSERT_EQ_INT(s2geog_geography_dimension(poly), 2);

  double area = 0;
  ASSERT_EQ_INT(s2geog_area(poly, &area), 0);
  ASSERT_GT_DBL(area, 0.0);

  s2geog_geography_destroy(poly);
}

static void test_make_polygon_xyz(void) {
  double lnglats[4][2] = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};
  double xyz[12];
  int i;
  for (i = 0; i < 4; i++) {
    s2geog_op_point_to_point(lnglats[i], &xyz[3 * i]);
  }
  int64_t ring_offsets[] = {0, 4};
  S2GeogGeography* poly = s2geog_make_polygon_xyz(xyz, ring_offsets, 1);
  ASSERT_NOT_NULL(poly);
  ASSERT_EQ_INT(s2geog_geography_kind(poly), 3);

  double area = 0;
  ASSERT_EQ_INT(s2geog_area(poly, &area), 0);
  ASSERT_GT_DBL(area, 0.0);

  s2geog_geography_destroy(poly);
}

static void test_make_polygon_with_hole(void) {
  /* Outer: (0,0)-(20,0)-(20,20)-(0,20), Hole: (5,5)-(15,5)-(15,15)-(5,15) */
  double coords[] = {
    0.0, 0.0, 20.0, 0.0, 20.0, 20.0, 0.0, 20.0,
    5.0, 5.0, 15.0, 5.0, 15.0, 15.0, 5.0, 15.0
  };
  int64_t ring_offsets[] = {0, 4, 8};

  /* First: area of solid polygon (no hole) */
  int64_t solid_offsets[] = {0, 4};
  S2GeogGeography* solid = s2geog_make_polygon_lnglat(coords, solid_offsets, 1);
  ASSERT_NOT_NULL(solid);
  double solid_area = 0;
  ASSERT_EQ_INT(s2geog_area(solid, &solid_area), 0);

  /* Now polygon with hole */
  S2GeogGeography* poly = s2geog_make_polygon_lnglat(coords, ring_offsets, 2);
  ASSERT_NOT_NULL(poly);
  double area = 0;
  ASSERT_EQ_INT(s2geog_area(poly, &area), 0);
  ASSERT_GT_DBL(area, 0.0);
  ASSERT_GT_DBL(solid_area, area);  /* hole makes it smaller */

  s2geog_geography_destroy(poly);
  s2geog_geography_destroy(solid);
}

static void test_make_collection(void) {
  S2GeogGeography* pt = s2geog_make_point_lnglat(1.0, 2.0);
  double line_coords[] = {0.0, 0.0, 1.0, 0.0};
  S2GeogGeography* line = s2geog_make_polyline_lnglat(line_coords, 2);
  ASSERT_NOT_NULL(pt);
  ASSERT_NOT_NULL(line);

  S2GeogGeography* children[2];
  children[0] = pt;
  children[1] = line;
  S2GeogGeography* coll = s2geog_make_collection(children, 2);
  ASSERT_NOT_NULL(coll);
  ASSERT_EQ_INT(s2geog_geography_kind(coll), 4);  /* GEOGRAPHY_COLLECTION */

  int is_coll = 0;
  ASSERT_EQ_INT(s2geog_is_collection(coll, &is_coll), 0);
  ASSERT_EQ_INT(is_coll, 1);

  /* Collection owns children — only destroy collection */
  s2geog_geography_destroy(coll);
}

/* ============================================================
 * Integration test: all operations from raw coords
 * ============================================================ */

static void test_integration_all_ops_from_raw_coords(void) {
  /* --- 1. Construct geometries from raw lng/lat --- */

  /* Point inside the polygon */
  S2GeogGeography* pt_inside = s2geog_make_point_lnglat(5.0, 5.0);
  ASSERT_NOT_NULL(pt_inside);

  /* Point outside */
  S2GeogGeography* pt_outside = s2geog_make_point_lnglat(50.0, 50.0);
  ASSERT_NOT_NULL(pt_outside);

  /* Polyline */
  double line_coords[] = {0.0, 0.0, 0.0, 10.0};
  S2GeogGeography* polyline = s2geog_make_polyline_lnglat(line_coords, 2);
  ASSERT_NOT_NULL(polyline);

  /* Polygon A: (0,0)-(10,0)-(10,10)-(0,10) */
  double poly_a_coords[] = {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0};
  int64_t poly_a_offsets[] = {0, 4};
  S2GeogGeography* poly_a = s2geog_make_polygon_lnglat(
      poly_a_coords, poly_a_offsets, 1);
  ASSERT_NOT_NULL(poly_a);

  /* Polygon B: overlapping (5,5)-(15,5)-(15,15)-(5,15) */
  double poly_b_coords[] = {5.0, 5.0, 15.0, 5.0, 15.0, 15.0, 5.0, 15.0};
  int64_t poly_b_offsets[] = {0, 4};
  S2GeogGeography* poly_b = s2geog_make_polygon_lnglat(
      poly_b_coords, poly_b_offsets, 1);
  ASSERT_NOT_NULL(poly_b);

  /* --- 2. Scalar accessors --- */
  {
    double area = 0, perimeter = 0, length = 0, x = 0, y = 0;
    int np = 0, is_empty_val = 0, is_coll = 0;

    ASSERT_EQ_INT(s2geog_area(poly_a, &area), 0);
    ASSERT_GT_DBL(area, 0.0);

    ASSERT_EQ_INT(s2geog_perimeter(poly_a, &perimeter), 0);
    ASSERT_GT_DBL(perimeter, 0.0);

    ASSERT_EQ_INT(s2geog_length(polyline, &length), 0);
    ASSERT_GT_DBL(length, 0.0);

    ASSERT_EQ_INT(s2geog_x(pt_inside, &x), 0);
    ASSERT_NEAR(x, 5.0, 1e-6);

    ASSERT_EQ_INT(s2geog_y(pt_inside, &y), 0);
    ASSERT_NEAR(y, 5.0, 1e-6);

    ASSERT_EQ_INT(s2geog_num_points(pt_inside, &np), 0);
    ASSERT_EQ_INT(np, 1);

    ASSERT_EQ_INT(s2geog_geography_is_empty(poly_a, &is_empty_val), 0);
    ASSERT_EQ_INT(is_empty_val, 0);

    ASSERT_EQ_INT(s2geog_is_collection(poly_a, &is_coll), 0);
    ASSERT_EQ_INT(is_coll, 0);
  }

  /* --- 3. ShapeIndex + Predicates --- */
  S2GeogShapeIndex* idx_pt_in = s2geog_shape_index_new(pt_inside);
  S2GeogShapeIndex* idx_pt_out = s2geog_shape_index_new(pt_outside);
  S2GeogShapeIndex* idx_a = s2geog_shape_index_new(poly_a);
  S2GeogShapeIndex* idx_b = s2geog_shape_index_new(poly_b);
  ASSERT_NOT_NULL(idx_pt_in);
  ASSERT_NOT_NULL(idx_pt_out);
  ASSERT_NOT_NULL(idx_a);
  ASSERT_NOT_NULL(idx_b);

  {
    int result = -1;
    /* poly_a intersects pt_inside */
    ASSERT_EQ_INT(s2geog_intersects(idx_a, idx_pt_in, &result), 0);
    ASSERT_EQ_INT(result, 1);

    /* poly_a does not intersect pt_outside */
    ASSERT_EQ_INT(s2geog_intersects(idx_a, idx_pt_out, &result), 0);
    ASSERT_EQ_INT(result, 0);

    /* poly_a contains pt_inside */
    ASSERT_EQ_INT(s2geog_contains(idx_a, idx_pt_in, &result), 0);
    ASSERT_EQ_INT(result, 1);

    /* poly_a does not contain pt_outside */
    ASSERT_EQ_INT(s2geog_contains(idx_a, idx_pt_out, &result), 0);
    ASSERT_EQ_INT(result, 0);

    /* poly_a equals itself */
    ASSERT_EQ_INT(s2geog_equals(idx_a, idx_a, &result), 0);
    ASSERT_EQ_INT(result, 1);

    /* poly_a and poly_b overlap (intersect but not equal) */
    ASSERT_EQ_INT(s2geog_intersects(idx_a, idx_b, &result), 0);
    ASSERT_EQ_INT(result, 1);
    ASSERT_EQ_INT(s2geog_equals(idx_a, idx_b, &result), 0);
    ASSERT_EQ_INT(result, 0);
  }

  /* --- 4. Distance --- */
  {
    double dist = -1, max_dist = -1;
    ASSERT_EQ_INT(s2geog_distance(idx_a, idx_pt_out, &dist), 0);
    ASSERT_GT_DBL(dist, 0.0);

    ASSERT_EQ_INT(s2geog_max_distance(idx_a, idx_pt_out, &max_dist), 0);
    ASSERT_GT_DBL(max_dist, dist);

    /* Closest point */
    S2GeogGeography* cp = s2geog_closest_point(idx_a, idx_pt_out);
    ASSERT_NOT_NULL(cp);
    ASSERT_EQ_INT(s2geog_geography_kind(cp), 1);
    s2geog_geography_destroy(cp);

    /* Minimum clearance line */
    S2GeogGeography* mcl = s2geog_minimum_clearance_line_between(
        idx_a, idx_pt_out);
    ASSERT_NOT_NULL(mcl);
    ASSERT_EQ_INT(s2geog_geography_kind(mcl), 2);  /* polyline */
    s2geog_geography_destroy(mcl);
  }

  /* --- 5. Geometry-returning operations --- */
  {
    S2GeogGeography* centroid = s2geog_centroid(poly_a);
    ASSERT_NOT_NULL(centroid);
    ASSERT_EQ_INT(s2geog_geography_kind(centroid), 1);
    s2geog_geography_destroy(centroid);

    S2GeogGeography* boundary = s2geog_boundary(poly_a);
    ASSERT_NOT_NULL(boundary);
    s2geog_geography_destroy(boundary);

    S2GeogGeography* hull = s2geog_convex_hull(poly_a);
    ASSERT_NOT_NULL(hull);
    ASSERT_EQ_INT(s2geog_geography_kind(hull), 3);
    s2geog_geography_destroy(hull);
  }

  /* --- 6. Boolean operations --- */
  {
    S2GeogGeography* inter = s2geog_intersection(idx_a, idx_b);
    ASSERT_NOT_NULL(inter);

    S2GeogGeography* un = s2geog_union(idx_a, idx_b);
    ASSERT_NOT_NULL(un);

    S2GeogGeography* diff = s2geog_difference(idx_a, idx_b);
    ASSERT_NOT_NULL(diff);

    S2GeogGeography* sym = s2geog_sym_difference(idx_a, idx_b);
    ASSERT_NOT_NULL(sym);

    S2GeogGeography* uu = s2geog_unary_union(idx_a);
    ASSERT_NOT_NULL(uu);

    /* Verify: area(A) + area(B) - area(intersection) ~= area(union) */
    double a_a, a_b, a_inter, a_union;
    s2geog_area(poly_a, &a_a);
    s2geog_area(poly_b, &a_b);
    s2geog_area(inter, &a_inter);
    s2geog_area(un, &a_union);
    ASSERT_NEAR(a_a + a_b - a_inter, a_union, a_union * 0.01);

    s2geog_geography_destroy(uu);
    s2geog_geography_destroy(sym);
    s2geog_geography_destroy(diff);
    s2geog_geography_destroy(un);
    s2geog_geography_destroy(inter);
  }

  /* --- 7. Rebuild --- */
  {
    S2GeogGeography* rebuilt = s2geog_rebuild(poly_a);
    ASSERT_NOT_NULL(rebuilt);
    s2geog_geography_destroy(rebuilt);
  }

  /* --- 8. Coverings --- */
  {
    uint64_t* cell_ids = NULL;
    int64_t n = 0;
    ASSERT_EQ_INT(s2geog_covering(poly_a, 8, &cell_ids, &n), 0);
    ASSERT_GT_DBL((double)n, 0.0);
    ASSERT_NOT_NULL(cell_ids);
    s2geog_cell_ids_free(cell_ids);

    cell_ids = NULL;
    n = 0;
    ASSERT_EQ_INT(s2geog_interior_covering(poly_a, 8, &cell_ids, &n), 0);
    /* interior covering may be empty for small regions, just check no error */
    s2geog_cell_ids_free(cell_ids);
  }

  /* --- 9. Linear referencing --- */
  {
    double frac = -1;
    ASSERT_EQ_INT(s2geog_project_normalized(polyline, pt_inside, &frac), 0);
    /* pt_inside is (5,5); polyline is (0,0)-(0,10), so project ~0.5 */
    ASSERT_GT_DBL(frac, 0.0);

    S2GeogGeography* interp = s2geog_interpolate_normalized(polyline, 0.5);
    ASSERT_NOT_NULL(interp);
    ASSERT_EQ_INT(s2geog_geography_kind(interp), 1);
    s2geog_geography_destroy(interp);
  }

  /* --- 10. Validation --- */
  {
    char buf[256];
    int has_error = -1;
    ASSERT_EQ_INT(
        s2geog_find_validation_error(poly_a, buf, sizeof(buf), &has_error), 0);
    ASSERT_EQ_INT(has_error, 0);  /* valid polygon */
  }

  /* --- 11. GeographyIndex --- */
  {
    S2GeogGeographyIndex* gindex = s2geog_geography_index_new();
    ASSERT_NOT_NULL(gindex);

    ASSERT_EQ_INT(s2geog_geography_index_add(gindex, poly_a, 0), 0);
    ASSERT_EQ_INT(s2geog_geography_index_add(gindex, poly_b, 1), 0);

    int32_t* results = NULL;
    int64_t n_results = 0;
    ASSERT_EQ_INT(
        s2geog_geography_index_query(gindex, pt_inside, &results, &n_results),
        0);
    ASSERT_GT_DBL((double)n_results, 0.0);
    /* pt_inside (5,5) should hit poly_a (index 0) */
    {
      int found_0 = 0;
      int64_t i;
      for (i = 0; i < n_results; i++) {
        if (results[i] == 0) found_0 = 1;
      }
      assert(found_0);
    }
    s2geog_int32_free(results);
    s2geog_geography_index_destroy(gindex);
  }

  /* --- 12. Aggregators --- */
  {
    /* Centroid aggregator */
    S2GeogCentroidAggregator* cagg = s2geog_centroid_aggregator_new();
    ASSERT_NOT_NULL(cagg);
    ASSERT_EQ_INT(s2geog_centroid_aggregator_add(cagg, pt_inside), 0);
    ASSERT_EQ_INT(s2geog_centroid_aggregator_add(cagg, pt_outside), 0);
    S2GeogGeography* agg_centroid = s2geog_centroid_aggregator_finalize(cagg);
    ASSERT_NOT_NULL(agg_centroid);
    ASSERT_EQ_INT(s2geog_geography_kind(agg_centroid), 1);
    s2geog_geography_destroy(agg_centroid);
    s2geog_centroid_aggregator_destroy(cagg);

    /* Union aggregator */
    S2GeogUnionAggregator* uagg = s2geog_union_aggregator_new();
    ASSERT_NOT_NULL(uagg);
    ASSERT_EQ_INT(s2geog_union_aggregator_add(uagg, poly_a), 0);
    ASSERT_EQ_INT(s2geog_union_aggregator_add(uagg, poly_b), 0);
    S2GeogGeography* agg_union = s2geog_union_aggregator_finalize(uagg);
    ASSERT_NOT_NULL(agg_union);
    s2geog_geography_destroy(agg_union);
    s2geog_union_aggregator_destroy(uagg);
  }

  /* --- 13. WKT serialization of raw-constructed geometry --- */
  {
    S2GeogWKTWriter* writer = s2geog_wkt_writer_new(6);
    ASSERT_NOT_NULL(writer);
    char* wkt = s2geog_wkt_writer_write(writer, pt_inside);
    ASSERT_NOT_NULL(wkt);
    assert(strncmp(wkt, "POINT", 5) == 0);
    s2geog_string_free(wkt);

    wkt = s2geog_wkt_writer_write(writer, poly_a);
    ASSERT_NOT_NULL(wkt);
    assert(strncmp(wkt, "POLYGON", 7) == 0);
    s2geog_string_free(wkt);

    s2geog_wkt_writer_destroy(writer);
  }

  /* --- Cleanup --- */
  s2geog_shape_index_destroy(idx_b);
  s2geog_shape_index_destroy(idx_a);
  s2geog_shape_index_destroy(idx_pt_out);
  s2geog_shape_index_destroy(idx_pt_in);
  s2geog_geography_destroy(poly_b);
  s2geog_geography_destroy(poly_a);
  s2geog_geography_destroy(polyline);
  s2geog_geography_destroy(pt_outside);
  s2geog_geography_destroy(pt_inside);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void) {
  printf("s2geography_c API test (pure C)\n");
  printf("================================\n");

  /* Construction unit tests */
  RUN_TEST(test_make_point_lnglat);
  RUN_TEST(test_make_point_xyz);
  RUN_TEST(test_make_multipoint_lnglat);
  RUN_TEST(test_make_multipoint_xyz);
  RUN_TEST(test_make_polyline_lnglat);
  RUN_TEST(test_make_polyline_xyz);
  RUN_TEST(test_make_polygon_lnglat);
  RUN_TEST(test_make_polygon_xyz);
  RUN_TEST(test_make_polygon_with_hole);
  RUN_TEST(test_make_collection);

  /* Integration test */
  RUN_TEST(test_integration_all_ops_from_raw_coords);

  printf("================================\n");
  printf("All %d tests passed.\n", tests_passed);
  return 0;
}
