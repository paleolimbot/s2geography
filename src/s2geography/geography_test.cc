#include "s2geography/geography.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "s2geography.h"
#include "s2geography_gtest_util.h"

namespace s2geography {

TEST(Geography, EmptyPoint) {
  PointGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::POINT);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), 0);

  EXPECT_TRUE(geog.Points().empty());
  ASSERT_THAT(geog, WktEquals6("POINT EMPTY"));
}

TEST(Geography, EmptyPolyline) {
  PolylineGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::POLYLINE);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), 1);

  EXPECT_TRUE(geog.Polylines().empty());
  ASSERT_THAT(geog, WktEquals6("LINESTRING EMPTY"));
}

TEST(Geography, EmptyPolygon) {
  PolygonGeography geog;
  EXPECT_EQ(geog.kind(), GeographyKind::POLYGON);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), 2);

  EXPECT_TRUE(geog.Polygon()->is_empty());
  ASSERT_THAT(geog, WktEquals6("POLYGON EMPTY"));
}

TEST(Geography, EmptyCollection) {
  GeographyCollection geog;
  EXPECT_EQ(geog.kind(), GeographyKind::GEOGRAPHY_COLLECTION);
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), -1);
  EXPECT_TRUE(geog.Features().empty());
  ASSERT_THAT(geog, WktEquals6("GEOMETRYCOLLECTION EMPTY"));
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
  geog.EncodeTagged(&encoder, EncodeOptions());

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::POINT);
  ASSERT_THAT(*roundtrip, WktEquals6("POINT (-64 45)"));

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
  geog.EncodeTagged(&encoder, EncodeOptions());

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::POLYLINE);
  ASSERT_THAT(*roundtrip, WktEquals6("LINESTRING (-64 45, 0 0)"));

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
  geog.EncodeTagged(&encoder, EncodeOptions());

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::POLYGON);
  ASSERT_THAT(*roundtrip, WktEquals6("POLYGON ((-64 45, 0 45, 0 0, -64 45))"));

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
  geog.EncodeTagged(&encoder, EncodeOptions());

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::GEOGRAPHY_COLLECTION);
  ASSERT_THAT(*roundtrip, WktEquals6("GEOMETRYCOLLECTION (POINT (-64 45))"));

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

TEST(Geography, EncodedShapeIndex) {
  S2Point pt = S2LatLng::FromDegrees(45, -64).ToPoint();
  S2Point pt_mid = S2LatLng::FromDegrees(45, 0).ToPoint();
  S2Point pt_end = S2LatLng::FromDegrees(0, 0).ToPoint();
  Encoder encoder;

  PointGeography pt_geog(pt);

  auto polyline = absl::make_unique<S2Polyline>();
  polyline->Init({pt, pt_end});
  PolylineGeography line_geog(std::move(polyline));

  auto loop = absl::make_unique<S2Loop>();
  loop->Init({pt, pt_mid, pt_end});
  auto polygon = absl::make_unique<S2Polygon>();
  polygon->Init({std::move(loop)});
  PolygonGeography polygon_geog(std::move(polygon));

  ShapeIndexGeography geog;
  geog.Add(pt_geog);
  geog.Add(line_geog);
  geog.Add(polygon_geog);
  geog.EncodeTagged(&encoder, EncodeOptions());

  Decoder decoder(encoder.base(), encoder.length());
  auto roundtrip = Geography::DecodeTagged(&decoder);
  ASSERT_EQ(roundtrip->kind(), GeographyKind::ENCODED_SHAPE_INDEX);
  ASSERT_EQ(roundtrip->num_shapes(), 3);

  auto pt_shape = roundtrip->Shape(0);
  ASSERT_EQ(pt_shape->num_edges(), 1);
  EXPECT_EQ(pt_shape->edge(0).v0, pt);

  auto line_shape = roundtrip->Shape(1);
  ASSERT_EQ(line_shape->num_edges(), 1);
  EXPECT_EQ(line_shape->edge(0).v0, pt);
  EXPECT_EQ(line_shape->edge(0).v1, pt_end);

  auto poly_shape = roundtrip->Shape(2);
  ASSERT_EQ(poly_shape->num_edges(), 3);
  EXPECT_EQ(poly_shape->edge(0).v0, pt);
  EXPECT_EQ(poly_shape->edge(0).v1, pt_mid);
  EXPECT_EQ(poly_shape->edge(1).v0, pt_mid);
  EXPECT_EQ(poly_shape->edge(1).v1, pt_end);
  EXPECT_EQ(poly_shape->edge(2).v0, pt_end);
  EXPECT_EQ(poly_shape->edge(2).v1, pt);
}

void TestEncodeWKTRoundtrip(const std::string& wkt,
                            const EncodeOptions& options) {
  SCOPED_TRACE(wkt + " / " + ::testing::PrintToString(options) + " (WKT)");
  WKTReader reader;
  std::unique_ptr<Geography> original_geog = reader.read_feature(wkt);
  // Make sure the original geography matches the given WKT
  ASSERT_THAT(*original_geog, WktEquals6(wkt));

  Encoder encoder;
  original_geog->EncodeTagged(&encoder, options);

  Decoder decoder(encoder.base(), encoder.length());
  std::unique_ptr<Geography> roundtrip_geog = Geography::DecodeTagged(&decoder);
  EXPECT_THAT(*roundtrip_geog, WktEquals6(wkt));
}

void TestCoveringRoundtrip(const std::string& wkt,
                           const EncodeOptions& options) {
  SCOPED_TRACE(wkt + " / " + ::testing::PrintToString(options) + " (covering)");
  WKTReader reader;
  std::unique_ptr<Geography> original_geog = reader.read_feature(wkt);

  // Calculate the initial bound in the same way as EncodeTagged()
  std::vector<S2CellId> cell_ids;
  original_geog->GetCellUnionBound(&cell_ids);
  S2CellUnion::Normalize(&cell_ids);

  Encoder encoder;
  original_geog->EncodeTagged(&encoder, options);

  Decoder decoder(encoder.base(), encoder.length());
  EncodeTag tag;
  tag.Decode(&decoder);

  std::vector<S2CellId> cell_ids_roundtrip;
  tag.DecodeCovering(&decoder, &cell_ids_roundtrip);
  EXPECT_THAT(cell_ids_roundtrip,
              ::testing::ElementsAreArray(cell_ids.begin(), cell_ids.end()));
}

void TestWKTShapeIndexRoundtrip(const std::string& wkt,
                                const EncodeOptions& options) {
  SCOPED_TRACE(wkt + " / " + ::testing::PrintToString(options) + " (index)");
  WKTReader reader;
  std::unique_ptr<Geography> original_geog = reader.read_feature(wkt);
  ShapeIndexGeography original_index(*original_geog);

  Encoder encoder;
  original_index.EncodeTagged(&encoder, options);

  Decoder decoder(encoder.base(), encoder.length());
  std::unique_ptr<Geography> roundtrip_index =
      Geography::DecodeTagged(&decoder);

  EXPECT_EQ(roundtrip_index->kind(), GeographyKind::ENCODED_SHAPE_INDEX);
  EXPECT_EQ(roundtrip_index->num_shapes(), original_index.num_shapes());
}

TEST(Geography, EncodeRoundtrip) {
  std::vector<s2coding::CodingHint> hints{s2coding::CodingHint::COMPACT,
                                          s2coding::CodingHint::FAST};
  std::vector<bool> include_covering{true, false};
  std::vector<bool> enable_lazy{true, false};

  std::vector<EncodeOptions> option_options;
  for (const auto hint_opt : hints) {
    for (const auto covering_opt : include_covering) {
      for (const auto enable_lazy_opt : enable_lazy) {
        EncodeOptions opt;
        opt.set_coding_hint(hint_opt);
        opt.set_include_covering(covering_opt);
        opt.set_enable_lazy_decode(enable_lazy_opt);
        option_options.push_back(opt);
      }
    }
  }

  std::vector<std::string> wkt_options = TestWKT();
  // Make sure we aren't silently testing nothing
  ASSERT_GE(wkt_options.size(), 19);

  for (const auto& options : option_options) {
    for (const auto& wkt : wkt_options) {
      ASSERT_NO_FATAL_FAILURE(TestEncodeWKTRoundtrip(wkt, options));
      if (options.include_covering()) {
        ASSERT_NO_FATAL_FAILURE(TestCoveringRoundtrip(wkt, options));
      }

      if (!options.enable_lazy_decode() ||
          options.coding_hint() == s2coding::CodingHint::COMPACT) {
        ASSERT_NO_FATAL_FAILURE(TestWKTShapeIndexRoundtrip(wkt, options));
      }
    }
  }
}

}  // namespace s2geography
