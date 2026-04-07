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

struct UnionParam {
  std::string name;
  std::optional<std::string> input_wkt_a;
  std::optional<std::string> input_wkt_b;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os, const UnionParam& p) {
    os << (p.input_wkt_a ? *p.input_wkt_a : "null") << " | "
       << (p.input_wkt_b ? *p.input_wkt_b : "null") << " -> "
       << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class UnionTest : public ::testing::TestWithParam<UnionParam> {};

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
        UnionParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        UnionParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        UnionParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Point + Point
        UnionParam{"point_same", "POINT (0 0)", "POINT (0 0)", "POINT (0 0)"},
        UnionParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                   "MULTIPOINT ((0 0), (0 1))"},

        // Multipoint + Point
        UnionParam{"multipoint_point", "MULTIPOINT ((0 0), (1 1))",
                   "POINT (2 2)", "MULTIPOINT ((0 0), (1 1), (2 2))"},
        UnionParam{"multipoint_point_overlap", "MULTIPOINT ((0 0), (1 1))",
                   "POINT (0 0)", "MULTIPOINT ((0 0), (1 1))"},

        // Point + Point: very close disjoint (triggers non-early-return path)
        UnionParam{"point_very_close", "POINT (0 0)", "POINT (0 0.001)",
                   "MULTIPOINT ((0 0), (0 0.001))"},
        // Point + Point: very far disjoint (triggers early-return for
        // definitely not intersecting)
        UnionParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                   "MULTIPOINT ((0 0), (180 0))"},

        // Linestring + Linestring
        UnionParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                   "LINESTRING (0 10, 10 10)",
                   "MULTILINESTRING ((0 0, 10 0), (0 10, 10 10))"},
        UnionParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                   "LINESTRING (0 0, 10 0)", "LINESTRING (0 0, 10 0)"},
        // Linestring + Linestring: very close disjoint
        UnionParam{"linestring_very_close", "LINESTRING (0 0, 10 0)",
                   "LINESTRING (0 0.001, 10 0.001)",
                   "MULTILINESTRING ((0 0, 10 0), (0 0.001, 10 0.001))"},
        // Linestring + Linestring: very far disjoint (near-antipodal)
        UnionParam{"linestring_very_far", "LINESTRING (0 0, 10 0)",
                   "LINESTRING (170 0, 180 0)",
                   "MULTILINESTRING ((0 0, 10 0), (170 0, 180 0))"},

        // Polygon + Polygon: disjoint
        UnionParam{"polygon_disjoint", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                   "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                   "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                   "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Polygon + Polygon: identical
        UnionParam{"polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon + Polygon: very close disjoint
        UnionParam{"polygon_very_close", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                   "POLYGON ((5.001 0, 10 0, 10 5, 5.001 5, 5.001 0))",
                   "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                   "((5.001 0, 10 0, 10 5, 5.001 5, 5.001 0)))"},
        // Polygon + Polygon: very far disjoint (near-antipodal)
        UnionParam{"polygon_very_far", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                   "POLYGON ((170 -5, 175 -5, 175 0, 170 0, 170 -5))",
                   "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                   "((170 -5, 175 -5, 175 0, 170 0, 170 -5)))"},

        // Point + Linestring: mixed dimension
        UnionParam{"point_linestring", "POINT (5 5)", "LINESTRING (0 0, 10 0)",
                   "GEOMETRYCOLLECTION (POINT (5 5), LINESTRING (0 0, 10 0))"},
        // Point + Polygon: mixed dimension
        UnionParam{"point_polygon", "POINT (5 5)",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Point outside polygon
        UnionParam{"point_outside_polygon", "POINT (20 20)",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                   "GEOMETRYCOLLECTION (POINT (20 20), "
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0)))"},
        // Linestring + Polygon
        UnionParam{"linestring_polygon", "LINESTRING (0 0, 10 0)",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Linestring outside polygon
        UnionParam{"linestring_outside_polygon", "LINESTRING (20 0, 30 0)",
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                   "GEOMETRYCOLLECTION (LINESTRING (20 0, 30 0), "
                   "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0)))"}

        ),
    [](const ::testing::TestParamInfo<UnionParam>& info) {
      return info.param.name;
    });

struct IntersectionParam {
  std::string name;
  std::optional<std::string> input_wkt_a;
  std::optional<std::string> input_wkt_b;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os,
                                  const IntersectionParam& p) {
    os << (p.input_wkt_a ? *p.input_wkt_a : "null") << " | "
       << (p.input_wkt_b ? *p.input_wkt_b : "null") << " -> "
       << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class IntersectionTest : public ::testing::TestWithParam<IntersectionParam> {};

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
        IntersectionParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        IntersectionParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        IntersectionParam{"null_both", std::nullopt, std::nullopt,
                          std::nullopt},

        // Point + Point
        IntersectionParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                          "POINT (0 0)"},
        IntersectionParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                          "POINT EMPTY"},

        // Multipoint + Point
        IntersectionParam{"multipoint_point_overlap",
                          "MULTIPOINT ((0 0), (1 1))", "POINT (0 0)",
                          "POINT (0 0)"},
        IntersectionParam{"multipoint_point_disjoint",
                          "MULTIPOINT ((0 0), (1 1))", "POINT (2 2)",
                          "POINT EMPTY"},

        // Point + Point: very far disjoint (triggers early-return for
        // definitely not intersecting)
        IntersectionParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                          "POINT EMPTY"},

        // Linestring + Linestring
        IntersectionParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                          "LINESTRING (0 10, 10 10)", "LINESTRING EMPTY"},
        IntersectionParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                          "LINESTRING (0 0, 10 0)", "LINESTRING (0 0, 10 0)"},
        // Linestring + Linestring: crossing
        IntersectionParam{"linestring_crossing", "LINESTRING (0 -5, 0 5)",
                          "LINESTRING (-5 0, 5 0)", "POINT (0 0)"},

        // Polygon + Polygon: disjoint
        IntersectionParam{
            "polygon_disjoint", "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
            "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))", "POLYGON EMPTY"},
        // Polygon + Polygon: identical
        IntersectionParam{"polygon_same",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon + Polygon: overlapping
        IntersectionParam{
            "polygon_overlap", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON ((5 5, 15 5, 15 15, 5 15, 5 5))",
            "POLYGON ((5 5, 10 5.019002, 10 10, 5 10.037423, 5 5))"},
        // Polygon + Polygon: one contains the other
        IntersectionParam{"polygon_contains",
                          "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))",
                          "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
                          "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))"},

        // Point + Linestring: point at endpoint
        IntersectionParam{"point_on_linestring", "POINT (0 0)",
                          "LINESTRING (0 0, 10 0)", "POINT (0 0)"},
        // Point + Linestring: point off line
        IntersectionParam{"point_off_linestring", "POINT (5 5)",
                          "LINESTRING (0 0, 10 0)", "POINT EMPTY"},

        // Point + Polygon: point inside
        IntersectionParam{"point_inside_polygon", "POINT (5 5)",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "POINT (5 5)"},
        // Point + Polygon: point outside
        IntersectionParam{"point_outside_polygon", "POINT (20 20)",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "POINT EMPTY"},
        // Point + Polygon: point on boundary
        IntersectionParam{"point_on_polygon_boundary", "POINT (10 5)",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "POINT (10 5)"},

        // Linestring + Polygon: line inside
        IntersectionParam{"linestring_inside_polygon", "LINESTRING (2 5, 8 5)",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "LINESTRING (2 5, 8 5)"},
        // Linestring + Polygon: line outside
        IntersectionParam{
            "linestring_outside_polygon", "LINESTRING (20 0, 30 0)",
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", "LINESTRING EMPTY"},
        // Linestring + Polygon: line crossing boundary
        IntersectionParam{"linestring_crossing_polygon",
                          "LINESTRING (-5 5, 5 5)",
                          "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                          "LINESTRING (0 5.019002, 5 5)"}

        ),
    [](const ::testing::TestParamInfo<IntersectionParam>& info) {
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

struct DifferenceParam {
  std::string name;
  std::optional<std::string> input_wkt_a;
  std::optional<std::string> input_wkt_b;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os, const DifferenceParam& p) {
    os << (p.input_wkt_a ? *p.input_wkt_a : "null") << " | "
       << (p.input_wkt_b ? *p.input_wkt_b : "null") << " -> "
       << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class DifferenceTest : public ::testing::TestWithParam<DifferenceParam> {};

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
        DifferenceParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        DifferenceParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        DifferenceParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Branch: value0 is empty -> empty result
        DifferenceParam{"empty_a", "POINT EMPTY", "POINT (0 0)",
                        "GEOMETRYCOLLECTION EMPTY"},

        // Branch: value1 is empty -> return value0
        DifferenceParam{"empty_b_point", "POINT (0 0)", "POINT EMPTY",
                        "POINT (0 0)"},
        DifferenceParam{
            "empty_b_polygon", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON EMPTY", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Branch: coverings don't intersect -> return value0
        DifferenceParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                        "POINT (0 0)"},
        DifferenceParam{"polygon_very_far",
                        "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                        "POLYGON ((170 -5, 175 -5, 175 0, 170 0, 170 -5))",
                        "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))"},

        // Point - Point: same -> empty
        DifferenceParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                        "POINT EMPTY"},
        // Point - Point: different (but coverings overlap)
        DifferenceParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                        "POINT (0 0)"},

        // Linestring - Linestring: same -> empty
        DifferenceParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                        "LINESTRING (0 0, 10 0)", "LINESTRING EMPTY"},
        // Linestring - Linestring: disjoint (coverings may overlap)
        DifferenceParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                        "LINESTRING (0 10, 10 10)", "LINESTRING (0 0, 10 0)"},

        // Polygon - Polygon: same -> empty
        DifferenceParam{
            "polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", "POLYGON EMPTY"},
        // Polygon - Polygon: disjoint (coverings may overlap)
        DifferenceParam{"polygon_disjoint",
                        "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                        "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                        "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))"},
        // Polygon - Polygon: overlapping
        DifferenceParam{"polygon_overlap",
                        "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                        "POLYGON ((5 5, 15 5, 15 15, 5 15, 5 5))",
                        "POLYGON ((5 10.037423, 0 10, 0 0, 10 0, 10 5.019002, "
                        "5 5, 5 10.037423))"},
        // Polygon - Polygon: A contains B
        DifferenceParam{"polygon_a_contains_b",
                        "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))",
                        "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
                        "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                        "(5 10, 10 10, 10 5, 5 5, 5 10))"},
        // Polygon - Polygon: B contains A -> empty
        DifferenceParam{
            "polygon_b_contains_a", "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
            "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))", "POLYGON EMPTY"}

        ),
    [](const ::testing::TestParamInfo<DifferenceParam>& info) {
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

struct SymDifferenceParam {
  std::string name;
  std::optional<std::string> input_wkt_a;
  std::optional<std::string> input_wkt_b;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os,
                                  const SymDifferenceParam& p) {
    os << (p.input_wkt_a ? *p.input_wkt_a : "null") << " | "
       << (p.input_wkt_b ? *p.input_wkt_b : "null") << " -> "
       << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class SymDifferenceTest : public ::testing::TestWithParam<SymDifferenceParam> {
};

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
        SymDifferenceParam{"null_a", std::nullopt, "POINT (0 0)", std::nullopt},
        SymDifferenceParam{"null_b", "POINT (0 0)", std::nullopt, std::nullopt},
        SymDifferenceParam{"null_both", std::nullopt, std::nullopt,
                           std::nullopt},

        // Branch: both empty -> empty
        SymDifferenceParam{"both_empty", "POINT EMPTY", "POINT EMPTY",
                           "POINT EMPTY"},

        // Branch: value0 empty -> return value1
        SymDifferenceParam{"empty_a", "POINT EMPTY", "POINT (0 0)",
                           "POINT (0 0)"},
        SymDifferenceParam{"empty_a_polygon", "POLYGON EMPTY",
                           "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                           "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Branch: value1 empty -> return value0
        SymDifferenceParam{"empty_b", "POINT (0 0)", "POINT EMPTY",
                           "POINT (0 0)"},
        SymDifferenceParam{
            "empty_b_polygon", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON EMPTY", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},

        // Branch: coverings don't intersect -> combine both
        SymDifferenceParam{"point_very_far", "POINT (0 0)", "POINT (180 0)",
                           "MULTIPOINT ((0 0), (180 0))"},
        SymDifferenceParam{"polygon_very_far",
                           "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                           "POLYGON ((170 -5, 175 -5, 175 0, 170 0, 170 -5))",
                           "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                           "((170 -5, 175 -5, 175 0, 170 0, 170 -5)))"},

        // Point symdiff Point: same -> empty
        SymDifferenceParam{"point_same", "POINT (0 0)", "POINT (0 0)",
                           "POINT EMPTY"},
        // Point symdiff Point: different (coverings overlap)
        SymDifferenceParam{"point_different", "POINT (0 0)", "POINT (0 1)",
                           "MULTIPOINT ((0 0), (0 1))"},

        // Linestring symdiff Linestring: same -> empty
        SymDifferenceParam{"linestring_same", "LINESTRING (0 0, 10 0)",
                           "LINESTRING (0 0, 10 0)", "LINESTRING EMPTY"},
        // Linestring symdiff Linestring: disjoint (coverings may overlap)
        SymDifferenceParam{"linestring_disjoint", "LINESTRING (0 0, 10 0)",
                           "LINESTRING (0 10, 10 10)",
                           "MULTILINESTRING ((0 0, 10 0), (0 10, 10 10))"},

        // Polygon symdiff Polygon: same -> empty
        SymDifferenceParam{
            "polygon_same", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", "POLYGON EMPTY"},
        // Polygon symdiff Polygon: disjoint (coverings may overlap)
        SymDifferenceParam{"polygon_disjoint",
                           "POLYGON ((0 0, 5 0, 5 5, 0 5, 0 0))",
                           "POLYGON ((10 10, 15 10, 15 15, 10 15, 10 10))",
                           "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                           "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Polygon symdiff Polygon: B contains A
        SymDifferenceParam{"polygon_b_contains_a",
                           "POLYGON ((5 5, 10 5, 10 10, 5 10, 5 5))",
                           "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))",
                           "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                           "(5 10, 10 10, 10 5, 5 5, 5 10))"}

        ),
    [](const ::testing::TestParamInfo<SymDifferenceParam>& info) {
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

struct ReducePrecisionParam {
  std::string name;
  std::optional<std::string> input_wkt;
  std::optional<double> grid_size;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os,
                                  const ReducePrecisionParam& p) {
    os << (p.input_wkt ? *p.input_wkt : "null")
       << " grid_size=" << (p.grid_size ? std::to_string(*p.grid_size) : "null")
       << " -> " << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class ReducePrecisionTest
    : public ::testing::TestWithParam<ReducePrecisionParam> {};

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
                        {{p.input_wkt}}, {{p.grid_size}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, ReducePrecisionTest,
    ::testing::Values(
        // Null inputs
        ReducePrecisionParam{"null_geom", std::nullopt, 1.0, std::nullopt},
        ReducePrecisionParam{"null_grid_size", "POINT (0 0)", std::nullopt,
                             std::nullopt},
        ReducePrecisionParam{"null_both", std::nullopt, std::nullopt,
                             std::nullopt},

        // Point snapping to whole degrees (grid_size = 1.0)
        ReducePrecisionParam{"point_on_grid", "POINT (0 0)", 1.0,
                             "POINT (0 0)"},
        ReducePrecisionParam{"point_not_on_grid", "POINT (0.001 0.001)", 1.0,
                             "POINT (0 0)"},
        ReducePrecisionParam{"point_no_snap", "POINT (0.001 0.001)", -1,
                             "POINT (0.001 0.001)"},

        // Point snapping to 0.1 degree grid (grid_size = 0.1)
        ReducePrecisionParam{"point_tenth_degree_on_grid", "POINT (0.1 0.1)",
                             0.1, "POINT (0.1 0.1)"},
        ReducePrecisionParam{"point_tenth_degree_snap", "POINT (0.12 0.12)",
                             0.1, "POINT (0.1 0.1)"},

        // Multipoint: two nearby points snap to same location
        ReducePrecisionParam{"multipoint_merge",
                             "MULTIPOINT ((0.001 0.001), (0.002 0.002))", 1.0,
                             "POINT (0 0)"},
        // Multipoint: points remain distinct after snapping
        ReducePrecisionParam{"multipoint_distinct",
                             "MULTIPOINT ((0 0), (10 10))", 1.0,
                             "MULTIPOINT ((0 0), (10 10))"},

        // Linestring: no snapping needed
        ReducePrecisionParam{"linestring_on_grid", "LINESTRING (0 0, 10 10)",
                             1.0, "LINESTRING (0 0, 10 10)"},
        // Linestring: endpoints snap to grid
        ReducePrecisionParam{"linestring_snap",
                             "LINESTRING (0.001 0.001, 10.001 10.001)", 1.0,
                             "LINESTRING (0 0, 10 10)"},
        // Linestring: midpoints snap together on a grid
        ReducePrecisionParam{"linestring_midpoint_snap",
                             "LINESTRING (0 0, 4.9 4.9, 5.1 5.1, 10 10)", 1.0,
                             "LINESTRING (0 0, 5 5, 10 10)"},
        // Linestring: component collapses because the endpoints snap together
        ReducePrecisionParam{"linestring_collapse",
                             "LINESTRING (0.01 0.02, 0.03 0.04)", 1.0,
                             "LINESTRING EMPTY"},
        // Linestring: no snapping with negative grid size
        ReducePrecisionParam{"linestring_no_snap",
                             "LINESTRING (0.001 0.001, 10.001 10.001)", -1,
                             "LINESTRING (0.001 0.001, 10.001 10.001)"},
        // Linestring with Z
        ReducePrecisionParam{"linestring_z",
                             "LINESTRING Z (0 0 100, 10 10 200)", 1.0,
                             "LINESTRING Z (0 0 100, 10 10 200)"},
        // Linestring with Z and snapping (Z values are interpolated when
        // endpoints are snapped, so they won't be exact)
        ReducePrecisionParam{
            "linestring_snap_z",
            "LINESTRING Z (0.001 0.001 100, 10.001 10.001 200)", 1.0,
            "LINESTRING Z (0 0 100.010024, 10 10 199.99005)"},
        // Linestring with M
        ReducePrecisionParam{"linestring_m",
                             "LINESTRING M (0 0 100, 10 10 200)", 1.0,
                             "LINESTRING M (0 0 100, 10 10 200)"},
        // Linestring with ZM
        ReducePrecisionParam{
            "linestring_zm", "LINESTRING ZM (0 0 100 1000, 10 10 200 2000)",
            1.0, "LINESTRING ZM (0 0 100 1000, 10 10 200 2000)"},

        // Multilinestring: no snapping needed
        ReducePrecisionParam{"multilinestring_on_grid",
                             "MULTILINESTRING ((0 0, 10 10), (20 20, 30 30))",
                             1.0,
                             "MULTILINESTRING ((0 0, 10 10), (20 20, 30 30))"},
        // Multilinestring: endpoints snap to grid
        ReducePrecisionParam{"multilinestring_snap",
                             "MULTILINESTRING ((0.001 0.001, 10.001 10.001), "
                             "(20.001 20.001, 30.001 30.001))",
                             1.0,
                             "MULTILINESTRING ((0 0, 10 10), (20 20, 30 30))"},
        // Multilinestring: midpoints snap together on a grid
        ReducePrecisionParam{
            "multilinestring_midpoint_snap",
            "MULTILINESTRING ((0 0, 4.9 4.9, 5.1 5.1, 10 10), "
            "(20 20, 24.9 24.9, 25.1 25.1, 30 30))",
            1.0, "MULTILINESTRING ((0 0, 5 5, 10 10), (20 20, 25 25, 30 30))"},
        // Multilinestring: one component collapses because endpoints snap
        // together
        ReducePrecisionParam{
            "multilinestring_partial_collapse",
            "MULTILINESTRING ((0 0, 10 10), (0.01 0.02, 0.03 0.04))", 1.0,
            "LINESTRING (0 0, 10 10)"},
        // Multilinestring with Z
        ReducePrecisionParam{
            "multilinestring_z",
            "MULTILINESTRING Z ((0 0 100, 10 10 200), (20 20 300, 30 30 400))",
            1.0,
            "MULTILINESTRING Z ((0 0 100, 10 10 200), "
            "(20 20 300, 30 30 400))"},
        // Multilinestring with M
        ReducePrecisionParam{
            "multilinestring_m",
            "MULTILINESTRING M ((0 0 100, 10 10 200), (20 20 300, 30 30 400))",
            1.0,
            "MULTILINESTRING M ((0 0 100, 10 10 200), "
            "(20 20 300, 30 30 400))"},
        // Multilinestring with ZM
        ReducePrecisionParam{
            "multilinestring_zm",
            "MULTILINESTRING ZM ((0 0 100 1000, 10 10 200 2000), "
            "(20 20 300 3000, 30 30 400 4000))",
            1.0,
            "MULTILINESTRING ZM ((0 0 100 1000, 10 10 200 2000), "
            "(20 20 300 3000, 30 30 400 4000))"},

        // Check Z handling
        ReducePrecisionParam{"point_on_grid_z", "POINT Z (0 1 10)", 1.0,
                             "POINT Z (0 1 10)"},
        ReducePrecisionParam{"point_not_on_grid_z", "POINT Z (0.01 1.01 10)",
                             1.0, "POINT Z (0 1 10)"},
        ReducePrecisionParam{"multipoint_merge_z",
                             "MULTIPOINT Z (0.01 1.01 10, 0.01 1.01 20)", 1.0,
                             "POINT Z (0 1 10)"},
        ReducePrecisionParam{"multipoint_distinct_z",
                             "MULTIPOINT Z (0.01 1.01 10, 2.01 3.01 20)", 1.0,
                             "MULTIPOINT Z (0 1 10, 2 3 20)"},

        // Check M handling
        ReducePrecisionParam{"point_on_grid_m", "POINT M (0 1 10)", 1.0,
                             "POINT M (0 1 10)"},
        ReducePrecisionParam{"point_not_on_grid_m", "POINT M (0.01 1.01 10)",
                             1.0, "POINT M (0 1 10)"},
        ReducePrecisionParam{"multipoint_merge_m",
                             "MULTIPOINT M (0.01 1.01 10, 0.01 1.01 20)", 1.0,
                             "POINT M (0 1 10)"},
        ReducePrecisionParam{"multipoint_distinct_m",
                             "MULTIPOINT M (0.01 1.01 10, 2.01 3.01 20)", 1.0,
                             "MULTIPOINT M (0 1 10, 2 3 20)"},

        // Check POINT ZM handling
        ReducePrecisionParam{"point_on_grid_zm", "POINT ZM (0 1 10 100)", 1.0,
                             "POINT ZM (0 1 10 100)"},
        ReducePrecisionParam{"point_not_on_grid_zm",
                             "POINT ZM (0.01 1.01 10 100)", 1.0,
                             "POINT ZM (0 1 10 100)"},
        ReducePrecisionParam{
            "multipoint_merge_zm",
            "MULTIPOINT ZM (0.01 1.01 10 100, 0.01 1.01 20 200)", 1.0,
            "POINT ZM (0 1 10 100)"},
        ReducePrecisionParam{
            "multipoint_distinct_zm",
            "MULTIPOINT ZM (0.01 1.01 10 100, 2.01 3.01 20 200)", 1.0,
            "MULTIPOINT ZM (0 1 10 100, 2 3 20 200)"},

        // Polygon: single ring, no snapping (single loop fast path)
        ReducePrecisionParam{"polygon_simple",
                             "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1,
                             "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon: single ring with snapping
        ReducePrecisionParam{
            "polygon_snap",
            "POLYGON ((0.001 0.001, 10.001 0.001, 10.001 10.001, "
            "0.001 10.001, 0.001 0.001))",
            1.0, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon: shell with one hole (one shell + holes branch)
        ReducePrecisionParam{"polygon_with_collapsed_hole",
                             "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                             "(5 5, 5 5.1, 5.1 5.1, 5.1 5, 5 5))",
                             1, "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0))"},
        // Polygon: shell with a hole that collapses
        ReducePrecisionParam{"polygon_with_hole",
                             "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                             "(5 5, 5 15, 15 15, 15 5, 5 5))",
                             -1,
                             "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                             "(5 5, 5 15, 15 15, 15 5, 5 5))"},
        // Multipolygon: two disjoint shells (multiple shells, no holes)
        ReducePrecisionParam{"multipolygon_disjoint",
                             "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                             "((10 10, 15 10, 15 15, 10 15, 10 10)))",
                             -1,
                             "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                             "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Multipolygon: two shells, one with a hole (multiple shells +
        // holes)
        ReducePrecisionParam{"multipolygon_with_hole",
                             "MULTIPOLYGON (((0 0, 20 0, 20 20, 0 20, 0 0), "
                             "(5 5, 5 15, 15 15, 15 5, 5 5)), "
                             "((30 30, 40 30, 40 40, 30 40, 30 30)))",
                             -1,
                             "MULTIPOLYGON (((0 0, 20 0, 20 20, 0 20, 0 0), "
                             "(5 5, 5 15, 15 15, 15 5, 5 5)), "
                             "((30 30, 40 30, 40 40, 30 40, 30 30)))"}),
    [](const ::testing::TestParamInfo<ReducePrecisionParam>& info) {
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

struct SimplifyParam {
  std::string name;
  std::optional<std::string> input_wkt;
  std::optional<double> tolerance;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os, const SimplifyParam& p) {
    os << (p.input_wkt ? *p.input_wkt : "null")
       << " tolerance=" << (p.tolerance ? std::to_string(*p.tolerance) : "null")
       << " -> " << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class SimplifyTest : public ::testing::TestWithParam<SimplifyParam> {};

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
                        {{p.input_wkt}}, {{p.tolerance}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {p.expected_wkt}));
}

INSTANTIATE_TEST_SUITE_P(
    Build, SimplifyTest,
    ::testing::Values(
        // Null inputs
        SimplifyParam{"null_geom", std::nullopt, 0.0, std::nullopt},
        SimplifyParam{"null_tolerance", "POINT (0 0)", std::nullopt,
                      std::nullopt},
        SimplifyParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Point: unaffected by simplification (no edges)
        SimplifyParam{"point_zero_tolerance", "POINT (0 0)", 0.0,
                      "POINT (0 0)"},
        SimplifyParam{"point_large_tolerance", "POINT (0 0)", 1000000.0,
                      "POINT (0 0)"},

        // Multipoint: zero tolerance preserves all points
        SimplifyParam{"multipoint_zero_tolerance",
                      "MULTIPOINT ((0 0), (10 10))", 0.0,
                      "MULTIPOINT ((0 0), (10 10))"},
        // Multipoint: large tolerance merges nearby points
        SimplifyParam{"multipoint_merge", "MULTIPOINT ((0 0), (0.001 0.001))",
                      1000000.0, "POINT (0 0)"},
        // Multipoint: large negative tolerance also merges nearby points
        SimplifyParam{"negative_tolerance", "MULTIPOINT ((0 0), (0.001 0.001))",
                      -1000000.0, "POINT (0 0)"},

        // Linestring: zero tolerance is identity
        SimplifyParam{"linestring_zero_tolerance", "LINESTRING (0 0, 10 0)",
                      0.0, "LINESTRING (0 0, 10 0)"},
        // Linestring: zero tolerance preserves intermediate vertex
        SimplifyParam{"linestring_zero_tolerance_3pt",
                      "LINESTRING (0 0, 5 1, 10 0)", 0.0,
                      "LINESTRING (0 0, 5 1, 10 0)"},
        // Linestring: large tolerance removes intermediate vertex
        SimplifyParam{"linestring_simplify", "LINESTRING (0 0, 5 1, 10 0)",
                      200000.0, "LINESTRING (0 0, 10 0)"},
        // Linestring: small tolerance keeps intermediate vertex
        SimplifyParam{"linestring_keep_vertex", "LINESTRING (0 0, 5 1, 10 0)",
                      50000.0, "LINESTRING (0 0, 5 1, 10 0)"},
        // Linestring: collapse when endpoints merge
        SimplifyParam{"linestring_collapse", "LINESTRING (0 0, 0.0001 0.0001)",
                      1000000.0, "LINESTRING EMPTY"},

        // Linestring with Z: zero tolerance preserves Z
        SimplifyParam{"linestring_z_zero_tolerance",
                      "LINESTRING Z (0 0 100, 10 0 200)", 0.0,
                      "LINESTRING Z (0 0 100, 10 0 200)"},
        // Linestring with M: zero tolerance preserves M
        SimplifyParam{"linestring_m_zero_tolerance",
                      "LINESTRING M (0 0 100, 10 0 200)", 0.0,
                      "LINESTRING M (0 0 100, 10 0 200)"},
        // Linestring with ZM: zero tolerance preserves ZM
        SimplifyParam{"linestring_zm_zero_tolerance",
                      "LINESTRING ZM (0 0 100 1000, 10 0 200 2000)", 0.0,
                      "LINESTRING ZM (0 0 100 1000, 10 0 200 2000)"},

        // Point with Z
        SimplifyParam{"point_z", "POINT Z (0 1 10)", 0.0, "POINT Z (0 1 10)"},
        // Point with M
        SimplifyParam{"point_m", "POINT M (0 1 10)", 0.0, "POINT M (0 1 10)"},
        // Point with ZM
        SimplifyParam{"point_zm", "POINT ZM (0 1 10 100)", 0.0,
                      "POINT ZM (0 1 10 100)"}

        // Polygon tests not included here because they currently test the same
        // code paths as the precision reducer.

        ),
    [](const ::testing::TestParamInfo<SimplifyParam>& info) {
      return info.param.name;
    });

}  // namespace s2geography
