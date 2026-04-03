#include "s2geography/build.h"

#include <gtest/gtest.h>

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
      out_array.get(),
      {"POINT (0 0)", "GEOMETRYCOLLECTION EMPTY", std::nullopt}));
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
      {"POINT (0 0)", "MULTIPOINT ((0 0), (0 1))", std::nullopt}));
}

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
      out_array.get(),
      {"GEOMETRYCOLLECTION EMPTY", "POINT (0 1)", std::nullopt}));
}

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
      {"GEOMETRYCOLLECTION EMPTY", "MULTIPOINT ((0 0), (0 1))", std::nullopt}));
}

TEST(Build, SedonaUdfUnaryUnionGridSize) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::UnaryUnionGridSizeKernel(&kernel);
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

struct UnaryUnionGridSizeParam {
  std::string name;
  std::optional<std::string> input_wkt;
  std::optional<double> grid_size;
  std::optional<std::string> expected_wkt;

  friend std::ostream& operator<<(std::ostream& os,
                                  const UnaryUnionGridSizeParam& p) {
    os << (p.input_wkt ? *p.input_wkt : "null")
       << " grid_size=" << (p.grid_size ? std::to_string(*p.grid_size) : "null")
       << " -> " << (p.expected_wkt ? *p.expected_wkt : "null");
    return os;
  }
};

class UnaryUnionGridSizeTest
    : public ::testing::TestWithParam<UnaryUnionGridSizeParam> {};

TEST_P(UnaryUnionGridSizeTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::UnaryUnionGridSizeKernel(&kernel);
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
    Build, UnaryUnionGridSizeTest,
    ::testing::Values(
        // Null inputs
        UnaryUnionGridSizeParam{"null_geom", std::nullopt, 1.0, std::nullopt},
        UnaryUnionGridSizeParam{"null_grid_size", "POINT (0 0)", std::nullopt,
                                std::nullopt},
        UnaryUnionGridSizeParam{"null_both", std::nullopt, std::nullopt,
                                std::nullopt},

        // Point snapping to whole degrees (grid_size = 1.0)
        UnaryUnionGridSizeParam{"point_on_grid", "POINT (0 0)", 1.0,
                                "POINT (0 0)"},
        UnaryUnionGridSizeParam{"point_not_on_grid", "POINT (0.001 0.001)", 1.0,
                                "POINT (0 0)"},
        UnaryUnionGridSizeParam{"point_no_snap", "POINT (0.001 0.001)", -1,
                                "POINT (0.001 0.001)"},

        // Point snapping to 0.1 degree grid (grid_size = 0.1)
        UnaryUnionGridSizeParam{"point_tenth_degree_on_grid", "POINT (0.1 0.1)",
                                0.1, "POINT (0.1 0.1)"},
        UnaryUnionGridSizeParam{"point_tenth_degree_snap", "POINT (0.12 0.12)",
                                0.1, "POINT (0.1 0.1)"},

        // Multipoint: two nearby points snap to same location
        UnaryUnionGridSizeParam{"multipoint_merge",
                                "MULTIPOINT ((0.001 0.001), (0.002 0.002))",
                                1.0, "POINT (0 0)"},
        // Multipoint: points remain distinct after snapping
        UnaryUnionGridSizeParam{"multipoint_distinct",
                                "MULTIPOINT ((0 0), (10 10))", 1.0,
                                "MULTIPOINT ((0 0), (10 10))"},

        // Linestring: no snapping needed
        UnaryUnionGridSizeParam{"linestring_on_grid", "LINESTRING (0 0, 10 10)",
                                1.0, "LINESTRING (0 0, 10 10)"},
        // Linestring: endpoints snap to grid
        UnaryUnionGridSizeParam{"linestring_snap",
                                "LINESTRING (0.001 0.001, 10.001 10.001)", 1.0,
                                "LINESTRING (0 0, 10 10)"},
        // Linestring: no snapping with negative grid size
        UnaryUnionGridSizeParam{"linestring_no_snap",
                                "LINESTRING (0.001 0.001, 10.001 10.001)", -1,
                                "LINESTRING (0.001 0.001, 10.001 10.001)"},
        // Linestring with Z
        UnaryUnionGridSizeParam{"linestring_z",
                                "LINESTRING Z (0 0 100, 10 10 200)", 1.0,
                                "LINESTRING Z (0 0 100, 10 10 200)"},
        // Linestring with Z and snapping (Z values are interpolated when
        // endpoints are snapped, so they won't be exact)
        UnaryUnionGridSizeParam{
            "linestring_snap_z",
            "LINESTRING Z (0.001 0.001 100, 10.001 10.001 200)", 1.0,
            "LINESTRING Z (0 0 100.010024, 10 10 199.99005)"},
        // Linestring with M
        UnaryUnionGridSizeParam{"linestring_m",
                                "LINESTRING M (0 0 100, 10 10 200)", 1.0,
                                "LINESTRING M (0 0 100, 10 10 200)"},
        // Linestring with ZM
        UnaryUnionGridSizeParam{
            "linestring_zm", "LINESTRING ZM (0 0 100 1000, 10 10 200 2000)",
            1.0, "LINESTRING ZM (0 0 100 1000, 10 10 200 2000)"},
        // Check Z handling
        UnaryUnionGridSizeParam{"point_on_grid_z", "POINT Z (0 1 10)", 1.0,
                                "POINT Z (0 1 10)"},
        UnaryUnionGridSizeParam{"point_no_snap_z", "POINT Z (0.01 1.01 10)",
                                1.0, "POINT Z (0 1 10)"},
        UnaryUnionGridSizeParam{"point_not_on_grid_z", "POINT Z (0.01 1.01 10)",
                                1.0, "POINT Z (0 1 10)"},
        UnaryUnionGridSizeParam{"multipoint_merge_z",
                                "MULTIPOINT Z (0.01 1.01 10, 0.01 1.01 20)",
                                1.0, "POINT Z (0 1 10)"},
        UnaryUnionGridSizeParam{"multipoint_distinct_z",
                                "MULTIPOINT Z (0.01 1.01 10, 2.01 3.01 20)",
                                1.0, "MULTIPOINT Z (0 1 10, 2 3 20)"},

        // Check M handling
        UnaryUnionGridSizeParam{"point_on_grid_m", "POINT M (0 1 10)", 1.0,
                                "POINT M (0 1 10)"},
        UnaryUnionGridSizeParam{"point_no_snap_m", "POINT M (0.01 1.01 10)",
                                1.0, "POINT M (0 1 10)"},
        UnaryUnionGridSizeParam{"point_not_on_grid_m", "POINT M (0.01 1.01 10)",
                                1.0, "POINT M (0 1 10)"},
        UnaryUnionGridSizeParam{"multipoint_merge_m",
                                "MULTIPOINT M (0.01 1.01 10, 0.01 1.01 20)",
                                1.0, "POINT M (0 1 10)"},
        UnaryUnionGridSizeParam{"multipoint_distinct_m",
                                "MULTIPOINT M (0.01 1.01 10, 2.01 3.01 20)",
                                1.0, "MULTIPOINT M (0 1 10, 2 3 20)"},

        // Check POINT ZM handling
        UnaryUnionGridSizeParam{"point_on_grid_zm", "POINT ZM (0 1 10 100)",
                                1.0, "POINT ZM (0 1 10 100)"},
        UnaryUnionGridSizeParam{"point_no_snap_zm",
                                "POINT ZM (0.01 1.01 10 100)", 1.0,
                                "POINT ZM (0 1 10 100)"},
        UnaryUnionGridSizeParam{"point_not_on_grid_zm",
                                "POINT ZM (0.01 1.01 10 100)", 1.0,
                                "POINT ZM (0 1 10 100)"},
        UnaryUnionGridSizeParam{
            "multipoint_merge_zm",
            "MULTIPOINT ZM (0.01 1.01 10 100, 0.01 1.01 20 200)", 1.0,
            "POINT ZM (0 1 10 100)"},
        UnaryUnionGridSizeParam{
            "multipoint_distinct_zm",
            "MULTIPOINT ZM (0.01 1.01 10 100, 2.01 3.01 20 200)", 1.0,
            "MULTIPOINT ZM (0 1 10 100, 2 3 20 200)"},

        // Polygon: single ring, no snapping (single loop fast path)
        UnaryUnionGridSizeParam{"polygon_simple",
                                "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", -1,
                                "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon: single ring with snapping
        UnaryUnionGridSizeParam{
            "polygon_snap",
            "POLYGON ((0.001 0.001, 10.001 0.001, 10.001 10.001, "
            "0.001 10.001, 0.001 0.001))",
            1.0, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))"},
        // Polygon: shell with one hole (one shell + holes branch)
        UnaryUnionGridSizeParam{"polygon_with_hole",
                                "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                                "(5 5, 5 15, 15 15, 15 5, 5 5))",
                                -1,
                                "POLYGON ((0 0, 20 0, 20 20, 0 20, 0 0), "
                                "(5 5, 5 15, 15 15, 15 5, 5 5))"},
        // Multipolygon: two disjoint shells (multiple shells, no holes)
        UnaryUnionGridSizeParam{"multipolygon_disjoint",
                                "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                                "((10 10, 15 10, 15 15, 10 15, 10 10)))",
                                -1,
                                "MULTIPOLYGON (((0 0, 5 0, 5 5, 0 5, 0 0)), "
                                "((10 10, 15 10, 15 15, 10 15, 10 10)))"},
        // Multipolygon: two shells, one with a hole (multiple shells +
        // holes)
        UnaryUnionGridSizeParam{"multipolygon_with_hole",
                                "MULTIPOLYGON (((0 0, 20 0, 20 20, 0 20, 0 0), "
                                "(5 5, 5 15, 15 15, 15 5, 5 5)), "
                                "((30 30, 40 30, 40 40, 30 40, 30 30)))",
                                -1,
                                "MULTIPOLYGON (((0 0, 20 0, 20 20, 0 20, 0 0), "
                                "(5 5, 5 15, 15 15, 15 5, 5 5)), "
                                "((30 30, 40 30, 40 40, 30 40, 30 30)))"}
        // Multipolygon: overlapping shells not yet merged into a single polygon
        // UnaryUnionGridSizeParam{
        //     "multipolygon_overlapping",
        //     "MULTIPOLYGON (((0 0, 3 0, 3 3, 0 3, 0 0)), "
        //     "((1 1, 4 1, 4 4, 1 4, 1 1)))",
        //     -1,
        //     "MULTIPOLYGON (((0 0, 3 0, 3 3, 0 3, 0 0)), "
        //     "((1 1, 4 1, 4 4, 1 4, 1 1)))"}

        ),
    [](const ::testing::TestParamInfo<UnaryUnionGridSizeParam>& info) {
      return info.param.name;
    });

}  // namespace s2geography
