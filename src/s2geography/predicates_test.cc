#include "s2geography/predicates.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/geoarrow-geography.h"
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
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
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
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
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
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))", "POLYGON ((0 0, 0.4 0, 0 0.4, 0 0))",
        "POLYGON ((30 30, 32 30, 30 32, 30 30))"},
       {"LINESTRING (0.25 0.25, 0.5 0.5)"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Big polygon contains it, small polygon doesn't, distant polygon doesn't
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, false}));
}

TEST(Predicates, SedonaUdfDisjointArrayScalar) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::DisjointKernel(&kernel);
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

  // Interior point -> not disjoint (false), exterior point -> disjoint (true)
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {false, true, std::nullopt}));
}

TEST(Predicates, SedonaUdfDisjointScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::DisjointKernel(&kernel);
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
                                          {false, true, std::nullopt}));
}

TEST(Predicates, SedonaUdfWithinArrayScalar) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::WithinKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt},
                         {"POLYGON ((0 0, 2 0, 0 2, 0 0))"}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // Within is the reverse of Contains: point inside polygon -> true
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfWithinScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::WithinKernel(&kernel);
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

  // Polygon is not within point
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {false, false, std::nullopt}));
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

  // Check with all combinations of forcing an index build on scalar arguments.
  // Because all of our arguments are scalar (we test length one arrays derived
  // from the test parameter), this lets us get full test coverage of any
  // special cases designed to avoid building internal indexes of small array
  // elements.
  for (bool prepare_arg0 : {true, false}) {
    for (bool prepare_arg1 : {true, false}) {
      SCOPED_TRACE("prepare_arg0: " + std::to_string(prepare_arg0) +
                   ", prepare_arg1: " + std::to_string(prepare_arg1));
      struct SedonaCScalarKernel kernel;
      struct SedonaCScalarKernelImpl impl;
      if (p.op == "intersects") {
        s2geography::sedona_udf::IntersectsKernel(&kernel, prepare_arg0,
                                                  prepare_arg1);
      } else if (p.op == "disjoint") {
        s2geography::sedona_udf::DisjointKernel(&kernel, prepare_arg0,
                                                prepare_arg1);
      } else if (p.op == "contains") {
        s2geography::sedona_udf::ContainsKernel(&kernel, prepare_arg0,
                                                prepare_arg1);
      } else if (p.op == "within") {
        s2geography::sedona_udf::WithinKernel(&kernel, prepare_arg0,
                                              prepare_arg1);
      } else if (p.op == "equals") {
        s2geography::sedona_udf::EqualsKernel(&kernel, prepare_arg0,
                                              prepare_arg1);
      } else {
        FAIL() << "Unknown predicate: " << p.op;
      }

      ASSERT_NO_FATAL_FAILURE(TestInitKernel(&kernel, &impl,
                                             {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                                             NANOARROW_TYPE_BOOL));

      nanoarrow::UniqueArray out_array;
      ASSERT_NO_FATAL_FAILURE(
          TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                            {{p.lhs}, {p.rhs}}, {}, out_array.get()));
      impl.release(&impl);
      kernel.release(&kernel);

      ASSERT_NO_FATAL_FAILURE(
          TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL, {p.expected}));
    }
  }
}

TEST_P(PredicatesScalarScalarTest, PredicateOperation) {
  const auto& p = GetParam();

  // Skip null tests for Operation (it doesn't handle nulls directly)
  if (!p.lhs.has_value() || !p.rhs.has_value()) {
    return;
  }

  // Create the appropriate predicate operation
  std::unique_ptr<s2geography::Operation> predicate;
  if (p.op == "intersects") {
    predicate = s2geography::Intersects();
  } else if (p.op == "disjoint") {
    predicate = s2geography::Disjoint();
  } else if (p.op == "contains") {
    predicate = s2geography::Contains();
  } else if (p.op == "within") {
    predicate = s2geography::Within();
  } else if (p.op == "equals") {
    predicate = s2geography::Equals();
  } else {
    FAIL() << "Unknown predicate: " << p.op;
  }

  // Check with all combinations of forcing an index build on arguments.
  // This ensures we test both prepared (indexed) and unprepared (fresh)
  // geographies.
  for (bool prepare_arg0 : {true, false}) {
    for (bool prepare_arg1 : {true, false}) {
      SCOPED_TRACE("prepare_arg0: " + std::to_string(prepare_arg0) +
                   ", prepare_arg1: " + std::to_string(prepare_arg1));

      // Create fresh geographies for each iteration to ensure unindexed state
      // when not preparing
      auto lhs_geom = TestGeometry::FromWKT(*p.lhs);
      auto rhs_geom = TestGeometry::FromWKT(*p.rhs);

      s2geography::GeoArrowGeography lhs_geog;
      s2geography::GeoArrowGeography rhs_geog;
      lhs_geog.Init(lhs_geom.geom());
      rhs_geog.Init(rhs_geom.geom());

      // Force index build if preparing, otherwise verify geography is unindexed
      if (prepare_arg0) {
        lhs_geog.ForceBuildIndex();
      } else {
        ASSERT_TRUE(lhs_geog.is_unindexed())
            << "lhs geography should be unindexed when not preparing";
      }

      if (prepare_arg1) {
        rhs_geog.ForceBuildIndex();
      } else {
        ASSERT_TRUE(rhs_geog.is_unindexed())
            << "rhs geography should be unindexed when not preparing";
      }

      predicate->ExecGeogGeog(lhs_geog, rhs_geog);
      bool result = predicate->GetInt() != 0;
      ASSERT_TRUE(p.expected.has_value())
          << "Expected value should not be null";
      EXPECT_EQ(result, *p.expected);
    }
  }
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
        ScalarScalarParam{"polygon_intersects_interior_polygon",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
                          true},

        // Two distant small polygons (brute force: all three checks fail)
        ScalarScalarParam{"polygon_not_intersects_distant_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "intersects",
                          "POLYGON ((30 30, 31 30, 30 31, 30 30))", false},

        // Linestring x linestring crossing (brute force edge crossing, no
        // polygons)
        ScalarScalarParam{"linestring_intersects_linestring_crossing",
                          "LINESTRING (0 0, 1 1)", "intersects",
                          "LINESTRING (0 1, 1 0)", true},
        // Linestring x linestring shared vertex (brute force CrossingSign == 0)
        ScalarScalarParam{"linestring_intersects_linestring_shared_vertex",
                          "LINESTRING (0 0, 1 0)", "intersects",
                          "LINESTRING (1 0, 2 0)", true},
        // Linestring x linestring disjoint (brute force all checks fail, no
        // polygons)
        ScalarScalarParam{"linestring_not_intersects_linestring",
                          "LINESTRING (0 0, 1 0)", "intersects",
                          "LINESTRING (30 30, 31 30)", false},

        // Multipoint x multipoint (brute force point-vertex match)
        ScalarScalarParam{"multipoint_intersects_multipoint_shared",
                          "MULTIPOINT (0 0, 1 1)", "intersects",
                          "MULTIPOINT (1 1, 2 2)", true},
        // Multipoint x multipoint disjoint
        ScalarScalarParam{"multipoint_not_intersects_multipoint",
                          "MULTIPOINT (0 0, 1 1)", "intersects",
                          "MULTIPOINT (3 3, 4 4)", false},
        // Multipoint x linestring (point at line vertex)
        ScalarScalarParam{"multipoint_intersects_linestring_vertex",
                          "MULTIPOINT (0 0, 5 5)", "intersects",
                          "LINESTRING (0 0, 1 0)", true},
        // Linestring x multipoint (point at line vertex, reversed)
        ScalarScalarParam{"linestring_intersects_multipoint_vertex",
                          "LINESTRING (0 0, 1 0)", "intersects",
                          "MULTIPOINT (0 0, 5 5)", true},
        // Multipoint x linestring disjoint
        ScalarScalarParam{"multipoint_not_intersects_linestring",
                          "MULTIPOINT (5 5, 6 6)", "intersects",
                          "LINESTRING (0 0, 1 0)", false},
        // Multipoint x polygon (point inside polygon, brute force)
        ScalarScalarParam{"multipoint_intersects_polygon_interior",
                          "MULTIPOINT (0.25 0.25, 5 5)", "intersects",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},
        // Multipoint x polygon disjoint
        ScalarScalarParam{"multipoint_not_intersects_polygon",
                          "MULTIPOINT (5 5, 6 6)", "intersects",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", false},

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

        // Linestring does not contain point (no polygons on container ->
        // brute force condition not met, falls through to index path)
        ScalarScalarParam{"linestring_not_contains_point",
                          "LINESTRING (0 0, 1 0)", "contains", "POINT (0.5 0)",
                          false},
        // Linestring does not contain linestring
        ScalarScalarParam{"linestring_not_contains_linestring",
                          "LINESTRING (0 0, 2 0)", "contains",
                          "LINESTRING (0 0, 1 0)", false},

        // Polygon does not contain overlapping polygon (brute force: vertices
        // inside but edges cross)
        ScalarScalarParam{"polygon_not_contains_overlapping_polygon",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POLYGON ((0.1 0.1, 3 0.1, 0.1 3, 0.1 0.1))", false},
        // Polygon does not contain distant polygon (brute force: vertex
        // outside early exit)
        ScalarScalarParam{"polygon_not_contains_distant_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "contains",
                          "POLYGON ((30 30, 31 30, 30 31, 30 30))", false},

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
                          "POLYGON ((0 0, 2 0, 0 2, 0 1, 0 0))", false},

        // GEOMETRYCOLLECTION tests
        // Intersects: GEOMETRYCOLLECTION with mixed types (point + linestring +
        // polygon) vs point inside. Exercises brute force handling of all
        // geometry types.
        ScalarScalarParam{
            "gc_intersects_point",
            "GEOMETRYCOLLECTION (POINT (5 5), LINESTRING (0 0, 1 0), "
            "POLYGON ((0 0, 2 0, 0 2, 0 0)))",
            "intersects", "POINT (0.25 0.25)", true},
        // Intersects: point in GEOMETRYCOLLECTION matches standalone point
        ScalarScalarParam{
            "point_intersects_gc_point", "POINT (5 5)", "intersects",
            "GEOMETRYCOLLECTION (POINT (5 5), LINESTRING (10 10, 11 10))",
            true},

        ScalarScalarParam{
            // Intersects: linestring crosses polygon in GEOMETRYCOLLECTION
            "linestring_intersects_gc_polygon", "LINESTRING (0.25 0.25, 3 3)",
            "intersects",
            "GEOMETRYCOLLECTION (POINT (30 30), POLYGON ((0 0, 2 "
            "0, 0 2, 0 0)))",
            true},
        // Intersects: GEOMETRYCOLLECTION vs GEOMETRYCOLLECTION (brute force
        // path, both have mixed types)
        ScalarScalarParam{
            "gc_intersects_gc",
            "GEOMETRYCOLLECTION (POINT (0.5 0.5), LINESTRING (0 0, 1 0))",
            "intersects",
            "GEOMETRYCOLLECTION (POINT (30 30), LINESTRING (0 0, 0 1))", true},
        // Intersects: disjoint GEOMETRYCOLLECTIONs
        ScalarScalarParam{
            "gc_not_intersects_gc",
            "GEOMETRYCOLLECTION (POINT (0 0), LINESTRING (1 1, 2 2))",
            "intersects",
            "GEOMETRYCOLLECTION (POINT (30 30), LINESTRING (40 40, 41 41))",
            false},

        // Contains: polygon in GEOMETRYCOLLECTION contains point (exercises
        // brute force with dimension() == 2 from the polygon component)
        ScalarScalarParam{
            "gc_contains_point",
            "GEOMETRYCOLLECTION (POINT (30 30), LINESTRING (40 40, 41 40), "
            "POLYGON ((0 0, 2 0, 0 2, 0 0)))",
            "contains", "POINT (0.25 0.25)", true},
        // Contains: polygon in GEOMETRYCOLLECTION contains interior linestring
        ScalarScalarParam{"gc_contains_linestring",
                          "GEOMETRYCOLLECTION (POINT (30 30), POLYGON ((0 0, 2 "
                          "0, 0 2, 0 0)))",
                          "contains", "LINESTRING (0.25 0.25, 0.5 0.5)", true},
        // Contains: polygon contains GEOMETRYCOLLECTION with point + linestring
        // (exercises SemiBruteForceIndexedContains with mixed containee)
        ScalarScalarParam{
            "polygon_contains_gc", "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
            "GEOMETRYCOLLECTION (POINT (0.25 0.25), LINESTRING (0.3 0.3, 0.4 "
            "0.4))",
            true},
        // Contains: polygon does not contain GEOMETRYCOLLECTION (point outside)
        ScalarScalarParam{
            "polygon_not_contains_gc_point_outside",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
            "GEOMETRYCOLLECTION (POINT (30 30), LINESTRING (0.3 0.3, 0.4 0.4))",
            false},
        // Contains: GEOMETRYCOLLECTION does not contain polygon that crosses
        // boundary
        ScalarScalarParam{"gc_not_contains_crossing_polygon",
                          "GEOMETRYCOLLECTION (POINT (30 30), POLYGON ((0 0, 2 "
                          "0, 0 2, 0 0)))",
                          "contains",
                          "POLYGON ((0.1 0.1, 3 0.1, 0.1 3, 0.1 0.1))", false},

        // Equals: GEOMETRYCOLLECTION equals itself (identity fast path)
        ScalarScalarParam{
            "gc_equals_gc_identical",
            "GEOMETRYCOLLECTION (POINT (0 0), LINESTRING (1 1, 2 2))", "equals",
            "GEOMETRYCOLLECTION (POINT (0 0), LINESTRING (1 1, 2 2))", true},
        // Equals: different GEOMETRYCOLLECTIONs
        ScalarScalarParam{
            "gc_not_equals_gc",
            "GEOMETRYCOLLECTION (POINT (0 0), LINESTRING (1 1, 2 2))", "equals",
            "GEOMETRYCOLLECTION (POINT (0 0), LINESTRING (1 1, 3 3))", false},
        // Equals: GEOMETRYCOLLECTION vs simple geometry
        ScalarScalarParam{"gc_not_equals_point",
                          "GEOMETRYCOLLECTION (POINT (0 0))", "equals",
                          "POINT (0 0)", true},
        // Disjoint
        ScalarScalarParam{"polygon_disjoint_distant_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "disjoint",
                          "POINT (-30 -30)", true},
        ScalarScalarParam{"polygon_not_disjoint_interior_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "disjoint",
                          "POINT (0.25 0.25)", false}

        ),
    [](const ::testing::TestParamInfo<ScalarScalarParam>& info) {
      return info.param.name;
    });
