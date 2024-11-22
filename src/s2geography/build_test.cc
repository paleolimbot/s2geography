#include <gtest/gtest.h>

#include "s2geography.h"
#include "s2geography/s2geography_gtest_util.h"

namespace s2geography {

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
