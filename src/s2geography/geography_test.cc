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

TEST(Geography, EmptyShapeIndex) {
  ShapeIndexGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::SHAPE_INDEX);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), -1);
  EXPECT_EQ(geog.ShapeIndex().num_shape_ids(), 0);
}

TEST(Geography, EncodedPoint) {
  S2Point pt = S2LatLng::FromDegrees(45, -64).ToPoint();
  Encoder encoder;

  PointGeography geog(pt);
  geog.EncodeTagged(&encoder);

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::POINT);

  auto roundtrip_typed = reinterpret_cast<PointGeography*>(roundtrip.get());
  ASSERT_EQ(roundtrip_typed->Points().size(), 1);
  EXPECT_EQ(roundtrip_typed->Points()[0], pt);
}

TEST(Geography, EncodedPolyline) {
  S2Point pt = S2LatLng::FromDegrees(45, -64).ToPoint();
  S2Point pt_end = S2LatLng::FromDegrees(0, 0).ToPoint();
  Encoder encoder;

  auto polyline = absl::make_unique<S2Polyline>();
  polyline->Init({pt, pt_end});
  PolylineGeography geog(std::move(polyline));
  geog.EncodeTagged(&encoder);

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::POLYLINE);

  auto roundtrip_typed = reinterpret_cast<PolylineGeography*>(roundtrip.get());
  ASSERT_EQ(roundtrip_typed->Polylines().size(), 1);
  const auto& polyline_rountrip = roundtrip_typed->Polylines()[0];
  EXPECT_TRUE(polyline_rountrip->Equals(*geog.Polylines()[0]));
}

TEST(Geography, EncodedPolygon) {
  S2Point pt = S2LatLng::FromDegrees(45, -64).ToPoint();
  S2Point pt_mid = S2LatLng::FromDegrees(45, 0).ToPoint();
  S2Point pt_end = S2LatLng::FromDegrees(0, 0).ToPoint();
  Encoder encoder;

  auto loop = absl::make_unique<S2Loop>();
  loop->Init({pt, pt_mid, pt_end});
  auto polygon = absl::make_unique<S2Polygon>();
  polygon->Init({std::move(loop)});
  PolygonGeography geog(std::move(polygon));
  geog.EncodeTagged(&encoder);

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::POLYGON);

  auto roundtrip_typed = reinterpret_cast<PolygonGeography*>(roundtrip.get());
  const auto& polygon_rountrip = roundtrip_typed->Polygon();
  EXPECT_TRUE(polygon_rountrip->Equals(*geog.Polygon()));
}

TEST(Geography, EncodedGeographyCollection) {
  S2Point pt = S2LatLng::FromDegrees(45, -64).ToPoint();
  Encoder encoder;

  auto child_geog = absl::make_unique<PointGeography>(pt);
  std::vector<std::unique_ptr<Geography>> child_geogs;
  child_geogs.emplace_back(child_geog.release());
  GeographyCollection geog(std::move(child_geogs));
  geog.EncodeTagged(&encoder);

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::GEOGRAPHY_COLLECTION);

  auto roundtrip_typed =
      reinterpret_cast<GeographyCollection*>(roundtrip.get());
  ASSERT_EQ(roundtrip_typed->Features().size(), 1);
  const auto& child_roundtrip = roundtrip_typed->Features()[0];
  ASSERT_EQ(child_roundtrip->kind(), GeographyKind::POINT);
  auto roundtrip_child_typed =
      reinterpret_cast<PointGeography*>(child_roundtrip.get());
  ASSERT_EQ(roundtrip_child_typed->Points().size(), 1);
  EXPECT_EQ(roundtrip_child_typed->Points()[0], pt);
}
