#include "s2geography/predicates.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

TEST(Predicates, SedonaUdfIntersectsScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
                         {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfIntersectsArrayScalar) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt},
                         {"POLYGON ((0 0, 1 0, 0 1, 0 0))"}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfEqualsScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::EqualsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
                         {"POLYGON ((1 0, 0 1, 0 0, 1 0))",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfContainsScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ContainsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
                         {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

// Semi-brute force tests: scalar side is indexed, array items are fresh.
// These exercise the SemiBruteForce paths for non-point geometries.

TEST(Predicates, SedonaUdfIntersectsScalarPolygonArrayLinestring) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
       {"LINESTRING (0.25 0.25, 0.5 0.5)", "LINESTRING (0.25 0.25, 3 3)",
        "LINESTRING (3 3, 4 4)"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Interior linestring, crossing linestring, exterior linestring
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, true, false}));
}

TEST(Predicates, SedonaUdfIntersectsArrayLinestringScalarPolygon) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"LINESTRING (0.25 0.25, 0.5 0.5)", "LINESTRING (0.25 0.25, 3 3)",
        "LINESTRING (3 3, 4 4)"},
       {"POLYGON ((0 0, 2 0, 0 2, 0 0))"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, true, false}));
}

TEST(Predicates, SedonaUdfIntersectsScalarPolygonArrayPolygon) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
       {"POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
        "POLYGON ((0 0, 1 0, 0 1, 0 0))",
        "POLYGON ((30 30, 32 30, 30 32, 30 30))"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Interior polygon, shared-boundary polygon, distant polygon
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, true, false}));
}

TEST(Predicates, SedonaUdfContainsScalarPolygonArrayLinestring) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ContainsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
       {"LINESTRING (0.25 0.25, 0.5 0.5)", "LINESTRING (0.25 0.25, 3 3)",
        "LINESTRING (3 3, 4 4)"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Interior linestring, crossing linestring, exterior linestring
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, false}));
}

TEST(Predicates, SedonaUdfContainsScalarPolygonArrayPolygon) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ContainsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
       {"POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
        "POLYGON ((0 0, 0.5 0, 0 0.5, 0 0))",
        "POLYGON ((30 30, 32 30, 30 32, 30 30))"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Interior sub-polygon, shared-boundary polygon (not contained), distant
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, false}));
}

TEST(Predicates, SedonaUdfContainsArrayPolygonScalarLinestring) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ContainsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))",
        "POLYGON ((0 0, 0.4 0, 0 0.4, 0 0))",
        "POLYGON ((30 30, 32 30, 30 32, 30 30))"},
       {"LINESTRING (0.25 0.25, 0.5 0.5)"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Big polygon contains it, small polygon doesn't, distant polygon doesn't
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, false}));
}

struct ScalarScalarParam {
  std::string name;
  std::optional<std::string> lhs;
  std::string op;
  std::optional<std::string> rhs;
  std::optional<bool> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const ScalarScalarParam& p) {
    os << (p.lhs ? *p.lhs : "null") << " " << p.op << " "
       << (p.rhs ? *p.rhs : "null") << " -> ";
    if (p.expected) {
      os << (*p.expected ? "true" : "false");
    } else {
      os << "null";
    }
    return os;
  }
};

class PredicatesScalarScalarTest
    : public ::testing::TestWithParam<ScalarScalarParam> {};

TEST_P(PredicatesScalarScalarTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  if (p.op == "intersects") {
    s2geography::sedona_udf::IntersectsKernel(&kernel);
  } else if (p.op == "contains") {
    s2geography::sedona_udf::ContainsKernel(&kernel);
  } else if (p.op == "equals") {
    s2geography::sedona_udf::EqualsKernel(&kernel);
  } else {
    FAIL() << "Unknown predicate: " << p.op;
  }

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{p.lhs}, {p.rhs}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL, {p.expected}));
}

INSTANTIATE_TEST_SUITE_P(
    Predicates, PredicatesScalarScalarTest,
    ::testing::Values(
        // Intersects
        // Nulls
        ScalarScalarParam{"null_intersects", std::nullopt, "intersects",
                          "POINT EMPTY", std::nullopt},
        ScalarScalarParam{"intersects_null", "POINT EMPTY", "intersects",
                          std::nullopt, std::nullopt},
        ScalarScalarParam{"null_intersects_null", std::nullopt, "intersects",
                          std::nullopt, std::nullopt},

        // Intersects cases that take one of the faster paths
        // Empties
        ScalarScalarParam{"intersects_empty", "POINT (0 0)", "intersects",
                          "POINT EMPTY", false},
        ScalarScalarParam{"empty_intersects", "POINT EMPTY", "intersects",
                          "POINT (0 0)", false},

        // Point x polygon
        ScalarScalarParam{"polygon_intersects_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (0.25 0.25)", true},
        // Polygon x point
        ScalarScalarParam{"point_intersects_polygon", "POINT (0.25 0.25)",
                          "intersects", "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},
        // Point definitely not in polygon (outside the covering)
        ScalarScalarParam{"polygon_not_intersects_distant_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (-30 -30)", false},
        // Point definitely not in polygon (probably inside the covering)
        ScalarScalarParam{"polygon_not_intersects_close_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (1.01 1.01)", false},

        // Polygon x boundary point
        ScalarScalarParam{"boundary_point_intersects_polygon", "POINT (0 0)",
                          "intersects", "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},
        // Boundary point x polygon
        ScalarScalarParam{"polygon_intersects_boundary_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (0 0)", true},

        // Polygon in polygon
        ScalarScalarParam{"polygon_intersects_polygon",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", true},

        // Polygon x linestring (linestring fully inside)
        ScalarScalarParam{"polygon_intersects_interior_linestring",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "LINESTRING (0.25 0.25, 0.5 0.5)", true},
        // Linestring x polygon (linestring fully inside)
        ScalarScalarParam{"interior_linestring_intersects_polygon",
                          "LINESTRING (0.25 0.25, 0.5 0.5)", "intersects",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},

        // Polygon x linestring (linestring partially crosses boundary)
        ScalarScalarParam{"polygon_intersects_crossing_linestring",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "LINESTRING (0.25 0.25, 3 3)", true},
        // Linestring x polygon (linestring partially crosses boundary)
        ScalarScalarParam{"crossing_linestring_intersects_polygon",
                          "LINESTRING (0.25 0.25, 3 3)", "intersects",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},

        // Polygon x linestring (linestring fully outside)
        ScalarScalarParam{"polygon_not_intersects_exterior_linestring",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "LINESTRING (3 3, 4 4)", false},

        // Interior polygon fully inside (no shared vertices)
        ScalarScalarParam{
            "polygon_intersects_interior_polygon",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
            "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))", true},

        // Contains
        // Nulls
        ScalarScalarParam{"null_contains", std::nullopt, "contains",
                          "POINT EMPTY", std::nullopt},
        ScalarScalarParam{"contains_null", "POINT EMPTY", "contains",
                          std::nullopt, std::nullopt},
        ScalarScalarParam{"null_contains_null", std::nullopt, "contains",
                          std::nullopt, std::nullopt},

        // Contains cases that take one of the faster paths
        // Empties
        ScalarScalarParam{"contains_empty", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                          "contains", "POINT EMPTY", false},
        ScalarScalarParam{"empty_contains", "POINT EMPTY", "contains",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", false},

        // Polygon contains interior point
        ScalarScalarParam{"polygon_contains_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POINT (0.25 0.25)", true},
        // Point does not contain anything
        ScalarScalarParam{"point_not_contains_polygon", "POINT (0.25 0.25)",
                          "contains", "POLYGON ((0 0, 2 0, 0 2, 0 0))", false},
        // Point definitely not in polygon (outside the covering)
        ScalarScalarParam{"polygon_not_contains_distant_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POINT (-30 -30)", false},
        // Point definitely not in polygon (probably inside the covering)
        ScalarScalarParam{"polygon_not_contains_close_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POINT (1.01 1.01)", false},
        // Polygon does not contain boundary point
        ScalarScalarParam{"polygon_not_contains_boundary_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POINT (0 0)", false},

        // Polygon contains interior sub-polygon
        ScalarScalarParam{
            "polygon_contains_polygon", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            "contains", "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))", true},

        // Polygon does not contain interior sub-polygon with shared boundary
        ScalarScalarParam{"polygon_does_not_contain_polygon_boundary",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POLYGON ((0 0, 0.5 0, 0 0.5, 0 0))", false},

        // Interior polygon does not contain Polygon
        ScalarScalarParam{"polygon_does_not_contain_polygon",
                          "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
                          "contains", "POLYGON ((0 0, 2 0, 0 2, 0 0))", false},

        // Polygon contains interior linestring
        ScalarScalarParam{"polygon_contains_interior_linestring",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "LINESTRING (0.25 0.25, 0.5 0.5)", true},
        // Polygon does not contain linestring that crosses boundary
        ScalarScalarParam{"polygon_not_contains_crossing_linestring",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "LINESTRING (0.25 0.25, 3 3)", false},
        // Polygon does not contain exterior linestring
        ScalarScalarParam{"polygon_not_contains_exterior_linestring",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "LINESTRING (3 3, 4 4)", false},

        // Equals
        // Nulls
        ScalarScalarParam{"null_equals", std::nullopt, "equals", "POINT EMPTY",
                          std::nullopt},
        ScalarScalarParam{"equals_null", "POINT EMPTY", "equals", std::nullopt,
                          std::nullopt},
        ScalarScalarParam{"null_equals_null", std::nullopt, "equals",
                          std::nullopt, std::nullopt},

        // Empties
        ScalarScalarParam{"equals_empty", "POINT (0 0)", "equals",
                          "POINT EMPTY", false},
        ScalarScalarParam{"empty_equals", "POINT EMPTY", "equals",
                          "POINT (0 0)", false},
        ScalarScalarParam{"empty_point_equals_empty_point", "POINT EMPTY",
                          "equals", "POINT EMPTY", true},
        ScalarScalarParam{"empty_point_equals_empty_linestring", "POINT EMPTY",
                          "equals", "LINESTRING EMPTY", true},

        // Fast path for identical values
        ScalarScalarParam{"polygon_equals_identical_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", true},

        // Rotated vertices
        ScalarScalarParam{"polygon_equals_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((1 0, 0 1, 0 0, 1 0))", true},

        // Potentially intersecting but not equal
        ScalarScalarParam{"polygon_not_equals_close_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", false},
        // Not at all intersecting and not equal
        ScalarScalarParam{"polygon_not_equals_distant_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((30 30, 32 30, 30 32, 30 30))", false},

        // Different number of chains
        ScalarScalarParam{"polygon_not_equals_chains_ne",
                          "MULTIPOLYGON (((0 0, 1 0, 0 1, 0 0)), ((10 10, 11 "
                          "10, 10 11, 10 10)))",
                          "equals", "POLYGON ((0 0, 2 0, 0 2, 0 0))", false},
        // Different number of edges
        ScalarScalarParam{"polygon_not_equals_edges_ne",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((0 0, 2 0, 0 2, 0 1, 0 0))", false}

        ),
    [](const ::testing::TestParamInfo<ScalarScalarParam>& info) {
      return info.param.name;
    });
