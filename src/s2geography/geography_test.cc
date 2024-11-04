#include "s2geography/geography.h"

#include <gtest/gtest.h>

#include "s2geography.h"

using namespace s2geography;

TEST(Geography, EmptyPoint) {
  PointGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::POINT);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), 0);

  EXPECT_TRUE(geog.Points().empty());
}

TEST(Geography, EmptyPolyline) {
  PolylineGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::POLYLINE);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), 1);

  EXPECT_TRUE(geog.Polylines().empty());
}

TEST(Geography, EmptyPolygon) {
  PolygonGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::POLYGON);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), 2);

  EXPECT_TRUE(geog.Polygon()->is_empty());
}

TEST(Geography, EmptyCollection) {
  GeographyCollection geog;
  EXPECT_EQ(geog.kind(), GeographyKind::GEOGRAPHY_COLLECTION);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), -1);
  EXPECT_TRUE(geog.Features().empty());
}

TEST(Geography, ShapeIndex) {
  ShapeIndexGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::SHAPE_INDEX);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), -1);
  EXPECT_EQ(geog.ShapeIndex().num_shape_ids(), 0);
}
