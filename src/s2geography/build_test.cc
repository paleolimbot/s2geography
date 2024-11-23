#include <gtest/gtest.h>

#include "s2geography.h"
#include "s2geography/s2geography_gtest_util.h"

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

}  // namespace s2geography
