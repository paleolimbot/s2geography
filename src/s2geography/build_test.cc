#include "s2geography/build.h"

#include <gtest/gtest.h>

#include <string>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/s2geography_gtest_util.h"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

namespace s2geography {

void TestRebuild(const std::string& wkt, std::string expected = "",
                 const GlobalOptions& options = GlobalOptions()) {
  if (expected.empty()) {
    expected = wkt;
  }

  WKTReader reader;
  auto geog = reader.read_feature(wkt);
  ShapeIndexGeography index_geog(*geog);
  EXPECT_THAT(*s2_rebuild(index_geog, options), WktEquals6(expected));
}

TEST(Build, Rebuild) {
  ASSERT_NO_FATAL_FAILURE(TestRebuild("POINT (-64 45)"));
  ASSERT_NO_FATAL_FAILURE(TestRebuild("LINESTRING (-64 45, 0 1)"));
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild("POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"));

  // Ring orientation should be fixed with the default options
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild("POLYGON ((0 0, 0 10, 10 10, 10 0, 0 0))",
                  "POLYGON ((10 0, 10 10, 0 10, 0 0, 10 0))"));

  // Should be able to pass options
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild("MULTIPOINT ((-64 45), (-64 45))", "POINT (-64 45)"));

  GlobalOptions options;
  options.point_layer.set_duplicate_edges(
      S2Builder::GraphOptions::DuplicateEdges::KEEP);
  ASSERT_NO_FATAL_FAILURE(TestRebuild("MULTIPOINT ((-64 45), (-64 45))",
                                      "MULTIPOINT ((-64 45), (-64 45))",
                                      options));
}

TEST(Build, RebuildLayerActionPoint) {
  GlobalOptions options;
  options.point_layer_action = GlobalOptions::OUTPUT_ACTION_INCLUDE;
  options.polyline_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  options.polygon_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;

  std::string collection(
      "GEOMETRYCOLLECTION (POINT (-64 45), LINESTRING(-64 45, 0 1), POLYGON "
      "((0 0, 1 0, 0 1, 0 0)))");

  options.point_layer_action = GlobalOptions::OUTPUT_ACTION_INCLUDE;
  ASSERT_NO_FATAL_FAILURE(TestRebuild(collection, "POINT (-64 45)", options));

  options.point_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild(collection, "GEOMETRYCOLLECTION EMPTY", options));

  options.point_layer_action = GlobalOptions::OUTPUT_ACTION_ERROR;
  EXPECT_THROW(TestRebuild(collection, "GEOMETRYCOLLECTION EMPTY", options),
               Exception);
}

TEST(Build, RebuildLayerActionPolyline) {
  GlobalOptions options;
  options.point_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  options.polyline_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  options.polygon_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;

  std::string collection(
      "GEOMETRYCOLLECTION (POINT (-64 45), LINESTRING(-64 45, 0 1), POLYGON "
      "((0 0, 1 0, 0 1, 0 0)))");

  options.polyline_layer_action = GlobalOptions::OUTPUT_ACTION_INCLUDE;
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild(collection, "LINESTRING (-64 45, 0 1)", options));

  options.polyline_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild(collection, "GEOMETRYCOLLECTION EMPTY", options));

  options.polyline_layer_action = GlobalOptions::OUTPUT_ACTION_ERROR;
  EXPECT_THROW(TestRebuild(collection, "GEOMETRYCOLLECTION EMPTY", options),
               Exception);
}

TEST(Build, RebuildLayerActionPolygon) {
  GlobalOptions options;
  options.point_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  options.polyline_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  options.polygon_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;

  std::string collection(
      "GEOMETRYCOLLECTION (POINT (-64 45), LINESTRING(-64 45, 0 1), POLYGON "
      "((0 0, 1 0, 0 1, 0 0)))");

  options.polygon_layer_action = GlobalOptions::OUTPUT_ACTION_INCLUDE;
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild(collection, "POLYGON ((0 0, 1 0, 0 1, 0 0))", options));

  options.polygon_layer_action = GlobalOptions::OUTPUT_ACTION_IGNORE;
  ASSERT_NO_FATAL_FAILURE(
      TestRebuild(collection, "GEOMETRYCOLLECTION EMPTY", options));

  options.polygon_layer_action = GlobalOptions::OUTPUT_ACTION_ERROR;
  EXPECT_THROW(TestRebuild(collection, "GEOMETRYCOLLECTION EMPTY", options),
               Exception);
}

void TestUnaryUnionRoundtrip(const std::string& wkt_filter) {
  std::vector<std::string> test_wkt = TestWKT(wkt_filter);
  ASSERT_GE(test_wkt.size(), 0);

  WKTReader reader;
  for (const auto& wkt : test_wkt) {
    SCOPED_TRACE(wkt);
    auto geog = reader.read_feature(wkt);
    ShapeIndexGeography index(*geog);
    auto geog_unary = s2_unary_union(index, GlobalOptions());
    ASSERT_EQ(geog_unary->num_shapes(), geog->num_shapes());
    if (geog_unary->num_shapes() == 0) {
      ASSERT_EQ(geog_unary->kind(), GeographyKind::GEOGRAPHY_COLLECTION);
      return;
    }

    ASSERT_EQ(geog_unary->kind(), geog->kind());
    EXPECT_EQ(s2_dimension(*geog_unary), s2_dimension(*geog));
    EXPECT_EQ(s2_length(*geog_unary), s2_length(*geog));
    EXPECT_EQ(s2_area(*geog_unary), s2_area(*geog));
  }
}

TEST(Build, UnaryUnionRoundtrip) {
  ASSERT_NO_FATAL_FAILURE(TestUnaryUnionRoundtrip("POINT"));
  ASSERT_NO_FATAL_FAILURE(TestUnaryUnionRoundtrip("MULTIPOINT"));
  ASSERT_NO_FATAL_FAILURE(TestUnaryUnionRoundtrip("LINESTRING"));
  ASSERT_NO_FATAL_FAILURE(TestUnaryUnionRoundtrip("MULTILINESTRING"));
  ASSERT_NO_FATAL_FAILURE(TestUnaryUnionRoundtrip("POLYGON"));
  ASSERT_NO_FATAL_FAILURE(TestUnaryUnionRoundtrip("MULTIPOLYGON"));
}

TEST(Build, SedonaUdfIntersection) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectionKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POINT (0 0)", "POINT (0 1)", std::nullopt}, {"POINT (0 0)"}}, {},
      out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 0)", "POINT EMPTY", std::nullopt}));
}

TEST(Build, SedonaUdfUnion) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::UnionKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POINT (0 0)", "POINT (0 1)", std::nullopt}, {"POINT (0 0)"}}, {},
      out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(),
      {"POINT (0 0)", "MULTIPOINT ((0 1), (0 0))", std::nullopt}));
}

struct BinaryOpParam {
  std::string name;
  std::optional<std::string> input_wkt_a;
  std::optional<std::string> input_wkt_b;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os, const BinaryOpParam& p) {
    os << (p.input_wkt_a ? *p.input_wkt_a : "null") << " | "
       << (p.input_wkt_b ? *p.input_wkt_b : "null") << " -> "
       << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class UnionTest : public ::testing::TestWithParam<BinaryOpParam> {};

TEST_P(UnionTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::UnionKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{p.input_wkt_a}, {p.input_wkt_b}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, UnionTest,
    ::testing::Values(
        // Null inputs
        BinaryOpParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        BinaryOpParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        BinaryOpParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Empty inputs
        BinaryOpParam{"both_empty", "POINT EMPTY", "POINT EMPTY",
                      "POINT EMPTY"},
        BinaryOpParam{"empty_a_point", "POINT EMPTY", "POINT (0 0)",
                      "POINT (0 0)"},
        BinaryOpParam{"empty_b_point", "POINT (0 0)", "POINT EMPTY",
                      "POINT (0 0)"},
        BinaryOpParam{"empty_a_polygon", "POLYGON EMPTY",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        BinaryOpParam{
            "empty_b_polygon", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON EMPTY", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Point + Point
        BinaryOpParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                      "POINT (0 0)"},
        BinaryOpParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                      "MULTIPOINT ((0 0), (0 1))"},

        // Multipoint + Point
        BinaryOpParam{"multipoint_point", "MULTIPOINT ((0 0), (1 1))",
                      "POINT (2 2)", "MULTIPOINT ((0 0), (1 1), (2 2))"},
        BinaryOpParam{"multipoint_point_overlap", "MULTIPOINT ((0 0), (1 1))",
                      "POINT (0 0)", "MULTIPOINT ((0 0), (1 1))"},

        // Point + Point: very close disjoint (triggers non-early-return path)
        BinaryOpParam{"point_very_close", "POINT (0 0)", "POINT (0 0.001)",
                      "MULTIPOINT ((0 0), (0 0.001))"},
        // Point + Point: very far disjoint (triggers early-return for
        // definitely not intersecting)
        BinaryOpParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                      "MULTIPOINT ((0 0), (180 0))"},

        // Linestring + Linestring
        BinaryOpParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 10, 10 10)",
                      "MULTILINESTRING ((0 0, 10 0), (0 10, 10 10))"},
        BinaryOpParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 0, 10 0)", "LINESTRING (0 0, 10 0)"},
        // Linestring + Linestring: very close disjoint
        BinaryOpParam{"linestring_very_close", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 0.001, 10 0.001)",
                      "MULTILINESTRING ((0 0, 10 0), (0 0.001, 10 0.001))"},
        // Linestring + Linestring: very far disjoint (near-antipodal)
        BinaryOpParam{"linestring_very_far", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (170 0, 180 0)",
                      "MULTILINESTRING ((0 0, 10 0), (170 0, 180 0))"},

        // Polygon + Polygon: disjoint
        BinaryOpParam{"polygon_disjoint", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                      "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                      "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                      "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Polygon + Polygon: identical
        BinaryOpParam{"polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon + Polygon: very close disjoint
        BinaryOpParam{"polygon_very_close",
                      "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                      "POLYGON ((5.001 0, 10 0, 10 5, 5.001 5, 5.001 0))",
                      "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                      "((5.001 0, 10 0, 10 5, 5.001 5, 5.001 0)))"},
        // Polygon + Polygon: very far disjoint (near-antipodal)
        BinaryOpParam{"polygon_very_far", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                      "POLYGON ((170 -5, 175 -5, 175 0, 170 0, 170 -5))",
                      "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                      "((170 -5, 175 -5, 175 0, 170 0, 170 -5)))"},

        // Point + Linestring: mixed dimension
        BinaryOpParam{
            "point_linestring", "POINT (5 5)", "LINESTRING (0 0, 10 0)",
            "GEOMETRYCOLLECTION (POINT (5 5), LINESTRING (0 0, 10 0))"},
        // Point + Polygon: mixed dimension
        BinaryOpParam{"point_polygon", "POINT (5 5)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Point outside polygon
        BinaryOpParam{"point_outside_polygon", "POINT (20 20)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "GEOMETRYCOLLECTION (POINT (20 20), "
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0)))"},
        // Linestring + Polygon
        BinaryOpParam{"linestring_polygon", "LINESTRING (0 0, 10 0)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Linestring outside polygon
        BinaryOpParam{"linestring_outside_polygon", "LINESTRING (20 0, 30 0)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "GEOMETRYCOLLECTION (LINESTRING (20 0, 30 0), "
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0)))"}

        ),
    [](const ::testing::TestParamInfo<BinaryOpParam>& info) {
      return info.param.name;
    });

class IntersectionTest : public ::testing::TestWithParam<BinaryOpParam> {};

TEST_P(IntersectionTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectionKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{p.input_wkt_a}, {p.input_wkt_b}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, IntersectionTest,
    ::testing::Values(
        // Null inputs
        BinaryOpParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        BinaryOpParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        BinaryOpParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Empty inputs
        BinaryOpParam{"both_empty", "POINT EMPTY", "POINT EMPTY",
                      "GEOMETRYCOLLECTION EMPTY"},
        BinaryOpParam{"empty_a_point", "POINT EMPTY", "POINT (0 0)",
                      "GEOMETRYCOLLECTION EMPTY"},
        BinaryOpParam{"empty_b_point", "POINT (0 0)", "POINT EMPTY",
                      "GEOMETRYCOLLECTION EMPTY"},
        BinaryOpParam{"empty_a_polygon", "POLYGON EMPTY",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "GEOMETRYCOLLECTION EMPTY"},
        BinaryOpParam{"empty_b_polygon",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON EMPTY", "GEOMETRYCOLLECTION EMPTY"},

        // Point + Point
        BinaryOpParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                      "POINT (0 0)"},
        BinaryOpParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                      "POINT EMPTY"},

        // Multipoint + Point
        BinaryOpParam{"multipoint_point_overlap", "MULTIPOINT ((0 0), (1 1))",
                      "POINT (0 0)", "POINT (0 0)"},
        BinaryOpParam{"multipoint_point_disjoint", "MULTIPOINT ((0 0), (1 1))",
                      "POINT (2 2)", "POINT EMPTY"},

        // Point + Point: very far disjoint (triggers early-return for
        // definitely not intersecting)
        BinaryOpParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                      "POINT EMPTY"},

        // Linestring + Linestring
        BinaryOpParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 10, 10 10)", "LINESTRING EMPTY"},
        BinaryOpParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 0, 10 0)", "LINESTRING (0 0, 10 0)"},
        // Linestring + Linestring: crossing
        BinaryOpParam{"linestring_crossing", "LINESTRING (0 -5, 0 5)",
                      "LINESTRING (-5 0, 5 0)", "POINT (0 0)"},

        // Polygon + Polygon: disjoint
        BinaryOpParam{"polygon_disjoint", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                      "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                      "POLYGON EMPTY"},
        // Polygon + Polygon: identical
        BinaryOpParam{"polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon + Polygon: overlapping
        BinaryOpParam{"polygon_overlap",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((5 5, 15 5, 15 15, 5 15, 5 5))",
                      "POLYGON ((5 5, 10 5.019002, 10 10, 5 10.037423, 5 5))"},
        // Polygon + Polygon: one contains the other
        BinaryOpParam{"polygon_contains",
                      "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))",
                      "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
                      "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))"},

        // Point + Linestring: point at endpoint
        BinaryOpParam{"point_on_linestring", "POINT (0 0)",
                      "LINESTRING (0 0, 10 0)", "POINT (0 0)"},
        // Point + Linestring: point off line
        BinaryOpParam{"point_off_linestring", "POINT (5 5)",
                      "LINESTRING (0 0, 10 0)", "POINT EMPTY"},

        // Point + Polygon: point inside
        BinaryOpParam{"point_inside_polygon", "POINT (5 5)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", "POINT (5 5)"},
        // Point + Polygon: point outside
        BinaryOpParam{"point_outside_polygon", "POINT (20 20)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", "POINT EMPTY"},
        // Point + Polygon: point on boundary
        BinaryOpParam{"point_on_polygon_boundary", "POINT (10 5)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POINT (10 5)"},

        // Linestring + Polygon: line inside
        BinaryOpParam{"linestring_inside_polygon", "LINESTRING (2 5, 8 5)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "LINESTRING (2 5, 8 5)"},
        // Linestring + Polygon: line outside
        BinaryOpParam{"linestring_outside_polygon", "LINESTRING (20 0, 30 0)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "LINESTRING EMPTY"},
        // Linestring + Polygon: line crossing boundary
        BinaryOpParam{"linestring_crossing_polygon", "LINESTRING (-5 5, 5 5)",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "LINESTRING (0 5.019002, 5 5)"}

        ),
    [](const ::testing::TestParamInfo<BinaryOpParam>& info) {
      return info.param.name;
    });

TEST(Build, SedonaUdfDifference) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::DifferenceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POINT (0 0)", "POINT (0 1)", std::nullopt}, {"POINT (0 0)"}}, {},
      out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT EMPTY", "POINT (0 1)", std::nullopt}));
}

class DifferenceTest : public ::testing::TestWithParam<BinaryOpParam> {};

TEST_P(DifferenceTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::DifferenceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{p.input_wkt_a}, {p.input_wkt_b}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, DifferenceTest,
    ::testing::Values(
        // Null inputs
        BinaryOpParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        BinaryOpParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        BinaryOpParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Branch: value0 is empty -> empty result
        BinaryOpParam{"empty_a", "POINT EMPTY", "POINT (0 0)",
                      "GEOMETRYCOLLECTION EMPTY"},

        // Branch: value1 is empty -> return value0
        BinaryOpParam{"empty_b_point", "POINT (0 0)", "POINT EMPTY",
                      "POINT (0 0)"},
        BinaryOpParam{
            "empty_b_polygon", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON EMPTY", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Branch: coverings don't intersect -> return value0
        BinaryOpParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                      "POINT (0 0)"},
        BinaryOpParam{"polygon_very_far", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                      "POLYGON ((170 -5, 175 -5, 175 0, 170 0, 170 -5))",
                      "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))"},

        // Point - Point: same -> empty
        BinaryOpParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                      "POINT EMPTY"},
        // Point - Point: different (but coverings overlap)
        BinaryOpParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                      "POINT (0 0)"},

        // Linestring - Linestring: same -> empty
        BinaryOpParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 0, 10 0)", "LINESTRING EMPTY"},
        // Linestring - Linestring: disjoint (coverings may overlap)
        BinaryOpParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                      "LINESTRING (0 10, 10 10)", "LINESTRING (0 0, 10 0)"},

        // Polygon - Polygon: same -> empty
        BinaryOpParam{"polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON EMPTY"},
        // Polygon - Polygon: disjoint (coverings may overlap)
        BinaryOpParam{"polygon_disjoint", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                      "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                      "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))"},
        // Polygon - Polygon: overlapping
        BinaryOpParam{"polygon_overlap",
                      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                      "POLYGON ((5 5, 15 5, 15 15, 5 15, 5 5))",
                      "POLYGON ((5 10.037423, 0 10, 0 0, 10 0, 10 5.019002, "
                      "5 5, 5 10.037423))"},
        // Polygon - Polygon: A contains B
        BinaryOpParam{"polygon_a_contains_b",
                      "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))",
                      "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
                      "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                      "(5 10, 10 10, 10 5, 5 5, 5 10))"},
        // Polygon - Polygon: B contains A -> empty
        BinaryOpParam{
            "polygon_b_contains_a", "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
            "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))", "POLYGON EMPTY"}

        ),
    [](const ::testing::TestParamInfo<BinaryOpParam>& info) {
      return info.param.name;
    });

TEST(Build, SedonaUdfSymDifference) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::SymDifferenceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POINT (0 0)", "POINT (0 1)", std::nullopt}, {"POINT (0 0)"}}, {},
      out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(),
      {"POINT EMPTY", "MULTIPOINT ((0 1), (0 0))", std::nullopt}));
}

using SymBinaryOpParam = BinaryOpParam;

class SymDifferenceTest : public ::testing::TestWithParam<SymBinaryOpParam> {};

TEST_P(SymDifferenceTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::SymDifferenceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{p.input_wkt_a}, {p.input_wkt_b}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, SymDifferenceTest,
    ::testing::Values(
        // Null inputs
        SymBinaryOpParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        SymBinaryOpParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        SymBinaryOpParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Branch: both empty -> empty
        SymBinaryOpParam{"both_empty", "POINT EMPTY", "POINT EMPTY",
                         "POINT EMPTY"},

        // Branch: value0 empty -> return value1
        SymBinaryOpParam{"empty_a", "POINT EMPTY", "POINT (0 0)",
                         "POINT (0 0)"},
        SymBinaryOpParam{"empty_a_polygon", "POLYGON EMPTY",
                         "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                         "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Branch: value1 empty -> return value0
        SymBinaryOpParam{"empty_b", "POINT (0 0)", "POINT EMPTY",
                         "POINT (0 0)"},
        SymBinaryOpParam{
            "empty_b_polygon", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON EMPTY", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Branch: coverings don't intersect -> combine both
        SymBinaryOpParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                         "MULTIPOINT ((0 0), (180 0))"},
        SymBinaryOpParam{"polygon_very_far",
                         "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                         "POLYGON ((170 -5, 175 -5, 175 0, 170 0, 170 -5))",
                         "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                         "((170 -5, 175 -5, 175 0, 170 0, 170 -5)))"},

        // Point symdiff Point: same -> empty
        SymBinaryOpParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                         "POINT EMPTY"},
        // Point symdiff Point: different (coverings overlap)
        SymBinaryOpParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                         "MULTIPOINT ((0 0), (0 1))"},

        // Linestring symdiff Linestring: same -> empty
        SymBinaryOpParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                         "LINESTRING (0 0, 10 0)", "LINESTRING EMPTY"},
        // Linestring symdiff Linestring: disjoint (coverings may overlap)
        SymBinaryOpParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                         "LINESTRING (0 10, 10 10)",
                         "MULTILINESTRING ((0 0, 10 0), (0 10, 10 10))"},

        // Polygon symdiff Polygon: same -> empty
        SymBinaryOpParam{
            "polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", "POLYGON EMPTY"},
        // Polygon symdiff Polygon: disjoint (coverings may overlap)
        SymBinaryOpParam{"polygon_disjoint",
                         "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                         "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                         "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                         "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Polygon symdiff Polygon: B contains A
        SymBinaryOpParam{"polygon_b_contains_a",
                         "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
                         "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))",
                         "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                         "(5 10, 10 10, 10 5, 5 5, 5 10))"}

        ),
    [](const ::testing::TestParamInfo<SymBinaryOpParam>& info) {
      return info.param.name;
    });

TEST(Build, SedonaUdfReducePrecision) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ReducePrecisionKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                        {{"POINT (0 0)", "POINT (0.001 0.001)", std::nullopt}},
                        {{1.0, 1.0, std::nullopt}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 0)", "POINT (0 0)", std::nullopt}));
}

struct UnaryScalarOpParam {
  std::string name;
  std::optional<std::string> input_wkt;
  std::optional<double> scalar_arg;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os,
                                  const UnaryScalarOpParam& p) {
    os << (p.input_wkt ? *p.input_wkt : "null")
       << " arg=" << (p.scalar_arg ? std::to_string(*p.scalar_arg) : "null")
       << " -> " << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class ReducePrecisionTest
    : public ::testing::TestWithParam<UnaryScalarOpParam> {};

TEST_P(ReducePrecisionTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ReducePrecisionKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                        {{p.input_wkt}}, {{p.scalar_arg}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, ReducePrecisionTest,
    ::testing::Values(
        // Null inputs
        UnaryScalarOpParam{"null_geom", std::nullopt, 1.0, std::nullopt},
        UnaryScalarOpParam{"null_grid_size", "POINT (0 0)", std::nullopt,
                           std::nullopt},
        UnaryScalarOpParam{"null_both", std::nullopt, std::nullopt,
                           std::nullopt},

        // Point snapping to whole degrees (grid_size = 1.0)
        UnaryScalarOpParam{"point_on_grid", "POINT (0 0)", 1.0, "POINT (0 0)"},
        UnaryScalarOpParam{"point_not_on_grid", "POINT (0.001 0.001)", 1.0,
                           "POINT (0 0)"},
        UnaryScalarOpParam{"point_no_snap", "POINT (0.001 0.001)", -1,
                           "POINT (0.001 0.001)"},

        // Point snapping to 0.1 degree grid (grid_size = 0.1)
        UnaryScalarOpParam{"point_tenth_degree_on_grid", "POINT (0.1 0.1)", 0.1,
                           "POINT (0.1 0.1)"},
        UnaryScalarOpParam{"point_tenth_degree_snap", "POINT (0.12 0.12)", 0.1,
                           "POINT (0.1 0.1)"},

        // Multipoint: two nearby points snap to same location
        UnaryScalarOpParam{"multipoint_merge",
                           "MULTIPOINT ((0.001 0.001), (0.002 0.002))", 1.0,
                           "POINT (0 0)"},
        // Multipoint: points remain distinct after snapping
        UnaryScalarOpParam{"multipoint_distinct", "MULTIPOINT ((0 0), (10 10))",
                           1.0, "MULTIPOINT ((0 0), (10 10))"},

        // Linestring: no snapping needed
        UnaryScalarOpParam{"linestring_on_grid", "LINESTRING (0 0, 10 10)", 1.0,
                           "LINESTRING (0 0, 10 10)"},
        // Linestring: endpoints snap to grid
        UnaryScalarOpParam{"linestring_snap",
                           "LINESTRING (0.001 0.001, 10.001 10.001)", 1.0,
                           "LINESTRING (0 0, 10 10)"},
        // Linestring: midpoints snap together on a grid
        UnaryScalarOpParam{"linestring_midpoint_snap",
                           "LINESTRING (0 0, 4.9 4.9, 5.1 5.1, 10 10)", 1.0,
                           "LINESTRING (0 0, 5 5, 10 10)"},
        // Linestring: component collapses because the endpoints snap together
        UnaryScalarOpParam{"linestring_collapse",
                           "LINESTRING (0.01 0.02, 0.03 0.04)", 1.0,
                           "LINESTRING EMPTY"},
        // Linestring: no snapping with negative grid size
        UnaryScalarOpParam{"linestring_no_snap",
                           "LINESTRING (0.001 0.001, 10.001 10.001)", -1,
                           "LINESTRING (0.001 0.001, 10.001 10.001)"},
        // Linestring with Z
        UnaryScalarOpParam{"linestring_z", "LINESTRING Z (0 0 100, 10 10 200)",
                           1.0, "LINESTRING Z (0 0 100, 10 10 200)"},
        // Linestring with Z and snapping (Z values are interpolated when
        // endpoints are snapped, so they won't be exact)
        UnaryScalarOpParam{"linestring_snap_z",
                           "LINESTRING Z (0.001 0.001 100, 10.001 10.001 200)",
                           1.0,
                           "LINESTRING Z (0 0 100.010024, 10 10 199.99005)"},
        // Linestring with M
        UnaryScalarOpParam{"linestring_m", "LINESTRING M (0 0 100, 10 10 200)",
                           1.0, "LINESTRING M (0 0 100, 10 10 200)"},
        // Linestring with ZM
        UnaryScalarOpParam{"linestring_zm",
                           "LINESTRING ZM (0 0 100 1000, 10 10 200 2000)", 1.0,
                           "LINESTRING ZM (0 0 100 1000, 10 10 200 2000)"},

        // Multilinestring: no snapping needed
        UnaryScalarOpParam{"multilinestring_on_grid",
                           "MULTILINESTRING ((0 0, 10 10), (20 20, 30 30))",
                           1.0,
                           "MULTILINESTRING ((0 0, 10 10), (20 20, 30 30))"},
        // Multilinestring: endpoints snap to grid
        UnaryScalarOpParam{"multilinestring_snap",
                           "MULTILINESTRING ((0.001 0.001, 10.001 10.001), "
                           "(20.001 20.001, 30.001 30.001))",
                           1.0,
                           "MULTILINESTRING ((0 0, 10 10), (20 20, 30 30))"},
        // Multilinestring: midpoints snap together on a grid
        UnaryScalarOpParam{
            "multilinestring_midpoint_snap",
            "MULTILINESTRING ((0 0, 4.9 4.9, 5.1 5.1, 10 10), "
            "(20 20, 24.9 24.9, 25.1 25.1, 30 30))",
            1.0, "MULTILINESTRING ((0 0, 5 5, 10 10), (20 20, 25 25, 30 30))"},
        // Multilinestring: one component collapses because endpoints snap
        // together
        UnaryScalarOpParam{
            "multilinestring_partial_collapse",
            "MULTILINESTRING ((0 0, 10 10), (0.01 0.02, 0.03 0.04))", 1.0,
            "LINESTRING (0 0, 10 10)"},
        // Multilinestring with Z
        UnaryScalarOpParam{
            "multilinestring_z",
            "MULTILINESTRING Z ((0 0 100, 10 10 200), (20 20 300, 30 30 400))",
            1.0,
            "MULTILINESTRING Z ((0 0 100, 10 10 200), "
            "(20 20 300, 30 30 400))"},
        // Multilinestring with M
        UnaryScalarOpParam{
            "multilinestring_m",
            "MULTILINESTRING M ((0 0 100, 10 10 200), (20 20 300, 30 30 400))",
            1.0,
            "MULTILINESTRING M ((0 0 100, 10 10 200), "
            "(20 20 300, 30 30 400))"},
        // Multilinestring with ZM
        UnaryScalarOpParam{
            "multilinestring_zm",
            "MULTILINESTRING ZM ((0 0 100 1000, 10 10 200 2000), "
            "(20 20 300 3000, 30 30 400 4000))",
            1.0,
            "MULTILINESTRING ZM ((0 0 100 1000, 10 10 200 2000), "
            "(20 20 300 3000, 30 30 400 4000))"},

        // Check Z handling
        UnaryScalarOpParam{"point_on_grid_z", "POINT Z (0 1 10)", 1.0,
                           "POINT Z (0 1 10)"},
        UnaryScalarOpParam{"point_not_on_grid_z", "POINT Z (0.01 1.01 10)", 1.0,
                           "POINT Z (0 1 10)"},
        UnaryScalarOpParam{"multipoint_merge_z",
                           "MULTIPOINT Z (0.01 1.01 10, 0.01 1.01 20)", 1.0,
                           "POINT Z (0 1 10)"},
        UnaryScalarOpParam{"multipoint_distinct_z",
                           "MULTIPOINT Z (0.01 1.01 10, 2.01 3.01 20)", 1.0,
                           "MULTIPOINT Z (0 1 10, 2 3 20)"},

        // Check M handling
        UnaryScalarOpParam{"point_on_grid_m", "POINT M (0 1 10)", 1.0,
                           "POINT M (0 1 10)"},
        UnaryScalarOpParam{"point_not_on_grid_m", "POINT M (0.01 1.01 10)", 1.0,
                           "POINT M (0 1 10)"},
        UnaryScalarOpParam{"multipoint_merge_m",
                           "MULTIPOINT M (0.01 1.01 10, 0.01 1.01 20)", 1.0,
                           "POINT M (0 1 10)"},
        UnaryScalarOpParam{"multipoint_distinct_m",
                           "MULTIPOINT M (0.01 1.01 10, 2.01 3.01 20)", 1.0,
                           "MULTIPOINT M (0 1 10, 2 3 20)"},

        // Check POINT ZM handling
        UnaryScalarOpParam{"point_on_grid_zm", "POINT ZM (0 1 10 100)", 1.0,
                           "POINT ZM (0 1 10 100)"},
        UnaryScalarOpParam{"point_not_on_grid_zm",
                           "POINT ZM (0.01 1.01 10 100)", 1.0,
                           "POINT ZM (0 1 10 100)"},
        UnaryScalarOpParam{"multipoint_merge_zm",
                           "MULTIPOINT ZM (0.01 1.01 10 100, 0.01 1.01 20 200)",
                           1.0, "POINT ZM (0 1 10 100)"},
        UnaryScalarOpParam{"multipoint_distinct_zm",
                           "MULTIPOINT ZM (0.01 1.01 10 100, 2.01 3.01 20 200)",
                           1.0, "MULTIPOINT ZM (0 1 10 100, 2 3 20 200)"},

        // Polygon: single ring, no snapping (single loop fast path)
        UnaryScalarOpParam{"polygon_simple",
                           "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1,
                           "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon: single ring with snapping
        UnaryScalarOpParam{
            "polygon_snap",
            "POLYGON ((0.001 0.001, 10.001 0.001, 10.001 10.001, "
            "0.001 10.001, 0.001 0.001))",
            1.0, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon: shell with one hole (one shell + holes branch)
        UnaryScalarOpParam{"polygon_with_collapsed_hole",
                           "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                           "(5 5, 5 5.1, 5.1 5.1, 5.1 5, 5 5))",
                           1, "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))"},
        // Polygon: shell with a hole that collapses
        UnaryScalarOpParam{"polygon_with_hole",
                           "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                           "(5 5, 5 15, 15 15, 15 5, 5 5))",
                           -1,
                           "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                           "(5 5, 5 15, 15 15, 15 5, 5 5))"},
        // Multipolygon: two disjoint shells (multiple shells, no holes)
        UnaryScalarOpParam{"multipolygon_disjoint",
                           "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                           "((10 10, 15 10, 15 15, 10 15, 10 10)))",
                           -1,
                           "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                           "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Multipolygon: two shells, one with a hole (multiple shells +
        // holes)
        UnaryScalarOpParam{"multipolygon_with_hole",
                           "MULTIPOLYGON (((0 0, 20 0, 20 20, 0 20, 0 0), "
                           "(5 5, 5 15, 15 15, 15 5, 5 5)), "
                           "((30 30, 40 30, 40 40, 30 40, 30 30)))",
                           -1,
                           "MULTIPOLYGON (((0 0, 20 0, 20 20, 0 20, 0 0), "
                           "(5 5, 5 15, 15 15, 15 5, 5 5)), "
                           "((30 30, 40 30, 40 40, 30 40, 30 30)))"}),
    [](const ::testing::TestParamInfo<UnaryScalarOpParam>& info) {
      return info.param.name;
    });

TEST(Build, SedonaUdfSimplify) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::SimplifyKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 0)", "LINESTRING (0 0, 10 0)", std::nullopt}},
      {{0.0, 0.0, std::nullopt}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(),
      {"POINT (0 0)", "LINESTRING (0 0, 10 0)", std::nullopt}));
}

class SimplifyTest : public ::testing::TestWithParam<UnaryScalarOpParam> {};

TEST_P(SimplifyTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::SimplifyKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                        {{p.input_wkt}}, {{p.scalar_arg}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, SimplifyTest,
    ::testing::Values(
        // Null inputs
        UnaryScalarOpParam{"null_geom", std::nullopt, 0.0, std::nullopt},
        UnaryScalarOpParam{"null_tolerance", "POINT (0 0)", std::nullopt,
                           std::nullopt},
        UnaryScalarOpParam{"null_both", std::nullopt, std::nullopt,
                           std::nullopt},

        // Point: unaffected by simplification (no edges)
        UnaryScalarOpParam{"point_zero_tolerance", "POINT (0 0)", 0.0,
                           "POINT (0 0)"},
        UnaryScalarOpParam{"point_large_tolerance", "POINT (0 0)", 1000000.0,
                           "POINT (0 0)"},

        // Multipoint: zero tolerance preserves all points
        UnaryScalarOpParam{"multipoint_zero_tolerance",
                           "MULTIPOINT ((0 0), (10 10))", 0.0,
                           "MULTIPOINT ((0 0), (10 10))"},
        // Multipoint: large tolerance merges nearby points
        UnaryScalarOpParam{"multipoint_merge",
                           "MULTIPOINT ((0 0), (0.001 0.001))", 1000000.0,
                           "POINT (0 0)"},
        // Multipoint: large negative tolerance also merges nearby points
        UnaryScalarOpParam{"negative_tolerance",
                           "MULTIPOINT ((0 0), (0.001 0.001))", -1000000.0,
                           "POINT (0 0)"},

        // Linestring: zero tolerance is identity
        UnaryScalarOpParam{"linestring_zero_tolerance",
                           "LINESTRING (0 0, 10 0)", 0.0,
                           "LINESTRING (0 0, 10 0)"},
        // Linestring: zero tolerance preserves intermediate vertex
        UnaryScalarOpParam{"linestring_zero_tolerance_3pt",
                           "LINESTRING (0 0, 5 1, 10 0)", 0.0,
                           "LINESTRING (0 0, 5 1, 10 0)"},
        // Linestring: large tolerance removes intermediate vertex
        UnaryScalarOpParam{"linestring_simplify", "LINESTRING (0 0, 5 1, 10 0)",
                           200000.0, "LINESTRING (0 0, 10 0)"},
        // Linestring: small tolerance keeps intermediate vertex
        UnaryScalarOpParam{"linestring_keep_vertex",
                           "LINESTRING (0 0, 5 1, 10 0)", 50000.0,
                           "LINESTRING (0 0, 5 1, 10 0)"},
        // Linestring: collapse when endpoints merge
        UnaryScalarOpParam{"linestring_collapse",
                           "LINESTRING (0 0, 0.0001 0.0001)", 1000000.0,
                           "LINESTRING EMPTY"},

        // Linestring with Z: zero tolerance preserves Z
        UnaryScalarOpParam{"linestring_z_zero_tolerance",
                           "LINESTRING Z (0 0 100, 10 0 200)", 0.0,
                           "LINESTRING Z (0 0 100, 10 0 200)"},
        // Linestring with M: zero tolerance preserves M
        UnaryScalarOpParam{"linestring_m_zero_tolerance",
                           "LINESTRING M (0 0 100, 10 0 200)", 0.0,
                           "LINESTRING M (0 0 100, 10 0 200)"},
        // Linestring with ZM: zero tolerance preserves ZM
        UnaryScalarOpParam{"linestring_zm_zero_tolerance",
                           "LINESTRING ZM (0 0 100 1000, 10 0 200 2000)", 0.0,
                           "LINESTRING ZM (0 0 100 1000, 10 0 200 2000)"},

        // Point with Z
        UnaryScalarOpParam{"point_z", "POINT Z (0 1 10)", 0.0,
                           "POINT Z (0 1 10)"},
        // Point with M
        UnaryScalarOpParam{"point_m", "POINT M (0 1 10)", 0.0,
                           "POINT M (0 1 10)"},
        // Point with ZM
        UnaryScalarOpParam{"point_zm", "POINT ZM (0 1 10 100)", 0.0,
                           "POINT ZM (0 1 10 100)"}

        // Polygon tests not included here because they currently test the same
        // code paths as the precision reducer.

        ),
    [](const ::testing::TestParamInfo<UnaryScalarOpParam>& info) {
      return info.param.name;
    });

TEST(Build, SedonaUdfBuffer) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::BufferKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 0)", "LINESTRING (0 0, 10 0)", std::nullopt}},
      {{0.0, 0.0, std::nullopt}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POLYGON EMPTY", "POLYGON EMPTY", std::nullopt}));
}

class BufferTest : public ::testing::TestWithParam<UnaryScalarOpParam> {};

TEST_P(BufferTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::BufferKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                        {{p.input_wkt}}, {{p.scalar_arg}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, BufferTest,
    ::testing::Values(
        // Null inputs
        UnaryScalarOpParam{"null_geom", std::nullopt, 0.0, std::nullopt},
        UnaryScalarOpParam{"null_distance", "POINT (0 0)", std::nullopt,
                           std::nullopt},
        UnaryScalarOpParam{"null_both", std::nullopt, std::nullopt,
                           std::nullopt},

        // Empty geometry: always POLYGON EMPTY regardless of distance
        UnaryScalarOpParam{"empty_point_zero", "POINT EMPTY", 0.0,
                           "POLYGON EMPTY"},
        UnaryScalarOpParam{"empty_point_positive", "POINT EMPTY", 100000.0,
                           "POLYGON EMPTY"},
        UnaryScalarOpParam{"empty_linestring", "LINESTRING EMPTY", 100000.0,
                           "POLYGON EMPTY"},
        UnaryScalarOpParam{"empty_polygon", "POLYGON EMPTY", 100000.0,
                           "POLYGON EMPTY"},

        // Point with zero distance: dimension < 2 and distance <= 0
        UnaryScalarOpParam{"point_zero_distance", "POINT (0 0)", 0.0,
                           "POLYGON EMPTY"},
        // Point with negative distance: dimension < 2 and distance <= 0
        UnaryScalarOpParam{"point_negative_distance", "POINT (0 0)", -100000.0,
                           "POLYGON EMPTY"},

        // Linestring with zero distance: dimension < 2 and distance <= 0
        UnaryScalarOpParam{"linestring_zero_distance", "LINESTRING (0 0, 10 0)",
                           0.0, "POLYGON EMPTY"},
        // Linestring with negative distance: dimension < 2 and distance <= 0
        UnaryScalarOpParam{"linestring_negative_distance",
                           "LINESTRING (0 0, 10 0)", -100000.0,
                           "POLYGON EMPTY"},

        // Polygon with negative distance: dimension == 2, goes through buffer
        // (erosion); a small polygon fully eroded produces empty
        UnaryScalarOpParam{"polygon_large_negative_distance",
                           "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", -1000000.0,
                           "POLYGON EMPTY"},

        // Point with positive distance: produces a polygon approximating a
        // circle
        UnaryScalarOpParam{"point_positive_distance", "POINT (0 0)", 100000.0,
                           "POLYGON ((-0.899308 0.004766, -0.88296 -0.170765, "
                           "-0.832686 -0.339735, -0.750414 -0.495651, "
                           "-0.639303 -0.632523, -0.50362 -0.745089, "
                           "-0.348578 -0.829023, -0.180135 -0.881096, "
                           "-0.004767 -0.899308, 0.170785 -0.882956, "
                           "0.339771 -0.832671, 0.495694 -0.750386, "
                           "0.632562 -0.639264, 0.745118 -0.503577, "
                           "0.829038 -0.348541, 0.881101 -0.180114, "
                           "0.899308 -0.004766, 0.88296 0.170765, "
                           "0.832686 0.339735, 0.750414 0.495651, "
                           "0.639303 0.632523, 0.50362 0.745089, "
                           "0.348578 0.829023, 0.180135 0.881096, "
                           "0.004767 0.899308, -0.170785 0.882956, "
                           "-0.339771 0.832671, -0.495694 0.750386, "
                           "-0.632562 0.639264, -0.745118 0.503577, "
                           "-0.829038 0.348541, -0.881101 0.180114, "
                           "-0.899308 0.004766))"},

        // Linestring with positive distance: produces a buffered corridor
        UnaryScalarOpParam{"linestring_positive_distance",
                           "LINESTRING (0 0, 1 0)", 100000.0,
                           "POLYGON ((0 0.89932, -0.175477 0.882036, "
                           "-0.344206 0.830847, -0.4997 0.747724, "
                           "-0.635982 0.635862, -0.747816 0.499561, "
                           "-0.830907 0.344063, -0.882062 0.175343, "
                           "-0.89932 -0.000115, -0.882017 -0.175569, "
                           "-0.830818 -0.344276, -0.747688 -0.499753, "
                           "-0.635819 -0.636025, -0.499508 -0.747852, "
                           "-0.343993 -0.830936, -0.175251 -0.882081, "
                           "0 -0.89932, 1 -0.89932, "
                           "1.175477 -0.882036, 1.344206 -0.830847, "
                           "1.4997 -0.747724, 1.635982 -0.635862, "
                           "1.747816 -0.499561, 1.830907 -0.344063, "
                           "1.882062 -0.175343, 1.89932 0.000115, "
                           "1.882017 0.175569, 1.830818 0.344276, "
                           "1.747688 0.499753, 1.635819 0.636025, "
                           "1.499508 0.747852, 1.343993 0.830936, "
                           "1.175251 0.882081, 1 0.89932, 0 0.89932))"},

        // Polygon with positive distance: expands the polygon
        UnaryScalarOpParam{"polygon_positive_distance",
                           "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", 100000.0,
                           "POLYGON ((0 -0.89932, 1 -0.89932, "
                           "1.175477 -0.882036, 1.344206 -0.830847, "
                           "1.4997 -0.747724, 1.635982 -0.635862, "
                           "1.747816 -0.499561, 1.830907 -0.344063, "
                           "1.882062 -0.175343, 1.89932 0, "
                           "1.899457 0.999877, 1.882221 1.175337, "
                           "1.831076 1.344064, 1.74798 1.499572, "
                           "1.636121 1.635882, 1.499794 1.74775, "
                           "1.344239 1.830874, 1.175437 1.882054, "
                           "1.000137 1.89932, -0.000137 1.89932, "
                           "-0.175685 1.882004, -0.344472 1.830777, "
                           "-0.500004 1.74761, -0.636299 1.635703, "
                           "-0.74812 1.499362, -0.831173 1.343831, "
                           "-0.882271 1.17509, -0.899457 0.999877, "
                           "-0.89932 0, -0.88204 -0.175456, "
                           "-0.830862 -0.34417, -0.747752 -0.499657, "
                           "-0.635901 -0.635943, -0.499604 -0.747788, "
                           "-0.344099 -0.830892, -0.175364 -0.882058, "
                           "0 -0.89932))"}

        ),
    [](const ::testing::TestParamInfo<UnaryScalarOpParam>& info) {
      return info.param.name;
    });

TEST(BufferParamsParse, Empty) {
  auto p = sedona_udf::BufferParams::Parse("");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kRound);
  EXPECT_EQ(p.join_style, sedona_udf::JoinStyle::kRound);
  EXPECT_FALSE(p.single_sided);
  EXPECT_DOUBLE_EQ(p.mitre_limit, 5.0);
  EXPECT_EQ(p.quadrant_segments, 8);
}

TEST(BufferParamsParse, EndcapRound) {
  auto p = sedona_udf::BufferParams::Parse("endcap=round");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kRound);
}

TEST(BufferParamsParse, EndcapFlat) {
  auto p = sedona_udf::BufferParams::Parse("endcap=flat");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kFlat);
}

TEST(BufferParamsParse, EndcapButt) {
  auto p = sedona_udf::BufferParams::Parse("endcap=butt");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kFlat);
}

TEST(BufferParamsParse, EndcapSquare) {
  auto p = sedona_udf::BufferParams::Parse("endcap=square");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kSquare);
}

TEST(BufferParamsParse, EndcapCaseInsensitive) {
  auto p = sedona_udf::BufferParams::Parse("ENDCAP=ROUND");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kRound);
}

TEST(BufferParamsParse, EndcapInvalid) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("endcap=invalid"), Exception);
}

TEST(BufferParamsParse, JoinRound) {
  auto p = sedona_udf::BufferParams::Parse("join=round");
  EXPECT_EQ(p.join_style, sedona_udf::JoinStyle::kRound);
}

TEST(BufferParamsParse, JoinMitre) {
  auto p = sedona_udf::BufferParams::Parse("join=mitre");
  EXPECT_EQ(p.join_style, sedona_udf::JoinStyle::kMitre);
}

TEST(BufferParamsParse, JoinMiter) {
  auto p = sedona_udf::BufferParams::Parse("join=miter");
  EXPECT_EQ(p.join_style, sedona_udf::JoinStyle::kMitre);
}

TEST(BufferParamsParse, JoinBevel) {
  auto p = sedona_udf::BufferParams::Parse("join=bevel");
  EXPECT_EQ(p.join_style, sedona_udf::JoinStyle::kBevel);
}

TEST(BufferParamsParse, JoinInvalid) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("join=invalid"), Exception);
}

TEST(BufferParamsParse, SideBoth) {
  auto p = sedona_udf::BufferParams::Parse("side=both");
  EXPECT_FALSE(p.single_sided);
}

TEST(BufferParamsParse, SideLeft) {
  auto p = sedona_udf::BufferParams::Parse("side=left");
  EXPECT_TRUE(p.single_sided);
  // When side is single-sided and endcap not explicitly set, defaults to square
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kSquare);
}

TEST(BufferParamsParse, SideRight) {
  auto p = sedona_udf::BufferParams::Parse("side=right");
  EXPECT_TRUE(p.single_sided);
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kSquare);
}

TEST(BufferParamsParse, SideWithExplicitEndcap) {
  auto p = sedona_udf::BufferParams::Parse("endcap=round side=left");
  EXPECT_TRUE(p.single_sided);
  // Endcap was explicitly set before side, so it stays round
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kRound);
}

TEST(BufferParamsParse, SideInvalid) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("side=invalid"), Exception);
}

TEST(BufferParamsParse, MitreLimit) {
  auto p = sedona_udf::BufferParams::Parse("mitre_limit=2.5");
  EXPECT_DOUBLE_EQ(p.mitre_limit, 2.5);
}

TEST(BufferParamsParse, MiterLimit) {
  auto p = sedona_udf::BufferParams::Parse("miter_limit=3.0");
  EXPECT_DOUBLE_EQ(p.mitre_limit, 3.0);
}

TEST(BufferParamsParse, MitreLimitInvalid) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("mitre_limit=abc"), Exception);
}

TEST(BufferParamsParse, QuadSegs) {
  auto p = sedona_udf::BufferParams::Parse("quad_segs=4");
  EXPECT_EQ(p.quadrant_segments, 4);
}

TEST(BufferParamsParse, QuadrantSegments) {
  auto p = sedona_udf::BufferParams::Parse("quadrant_segments=16");
  EXPECT_EQ(p.quadrant_segments, 16);
}

TEST(BufferParamsParse, QuadSegsInvalid) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("quad_segs=abc"), Exception);
}

TEST(BufferParamsParse, MultipleParams) {
  auto p = sedona_udf::BufferParams::Parse(
      "endcap=flat join=mitre mitre_limit=2.0 quad_segs=4");
  EXPECT_EQ(p.end_cap_style, sedona_udf::CapStyle::kFlat);
  EXPECT_EQ(p.join_style, sedona_udf::JoinStyle::kMitre);
  EXPECT_DOUBLE_EQ(p.mitre_limit, 2.0);
  EXPECT_EQ(p.quadrant_segments, 4);
}

TEST(BufferParamsParse, UnknownParam) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("unknown=value"), Exception);
}

TEST(BufferParamsParse, MissingValue) {
  EXPECT_THROW(sedona_udf::BufferParams::Parse("endcap"), Exception);
}

}  // namespace s2geography
