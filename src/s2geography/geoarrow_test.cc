#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "vendored/geoarrow/geoarrow.h"
#include "s2geography.h"

using testing::ElementsAre;
using testing::DoubleEq;

const double EARTH_RADIUS_METERS = 6371.01 * 1000;

void InitArrayWKT(ArrowArray* array, std::vector<std::string> values) {
  NANOARROW_THROW_NOT_OK(ArrowArrayInitFromType(array, NANOARROW_TYPE_STRING));
  NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array));
  for (const auto& value : values) {
    if (value == "") {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array, 1));
    } else {
      ArrowStringView na_value{value.data(),
                               static_cast<int64_t>(value.size())};
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendString(array, na_value));
    }
  }

  NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(array, nullptr));
}

void InitArrayWKB(ArrowArray* array, std::vector<std::vector<uint8_t>> values) {
  NANOARROW_THROW_NOT_OK(ArrowArrayInitFromType(array, NANOARROW_TYPE_BINARY));
  NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array));
  for (const auto& value : values) {
    if (value.size() == 0) {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array, 1));
    } else {
      ArrowBufferView na_value{value.data(),
                               static_cast<int64_t>(value.size())};
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendBytes(array, na_value));
    }
  }

  NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(array, nullptr));
}

void InitSchemaGeoArrowPoint(ArrowSchema* schema) {
  ArrowSchemaInit(schema);
  NANOARROW_THROW_NOT_OK(ArrowSchemaSetTypeStruct(schema, 2));
  NANOARROW_THROW_NOT_OK(ArrowSchemaSetName(schema->children[0], "x"));
  NANOARROW_THROW_NOT_OK(
      ArrowSchemaSetType(schema->children[0], NANOARROW_TYPE_DOUBLE));
  NANOARROW_THROW_NOT_OK(ArrowSchemaSetName(schema->children[1], "y"));
  NANOARROW_THROW_NOT_OK(
      ArrowSchemaSetType(schema->children[1], NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueBuffer buffer;
  NANOARROW_THROW_NOT_OK(ArrowMetadataBuilderInit(buffer.get(), nullptr));
  NANOARROW_THROW_NOT_OK(ArrowMetadataBuilderAppend(
      buffer.get(), ArrowCharView("ARROW:extension:name"),
      ArrowCharView("geoarrow.point")));
  NANOARROW_THROW_NOT_OK(ArrowSchemaSetMetadata(
      schema, reinterpret_cast<const char*>(buffer->data)));
}

void InitArrayGeoArrowPoint(ArrowArray* array, std::vector<double> x,
                            std::vector<double> y) {
  NANOARROW_DCHECK(x.size() == y.size());
  NANOARROW_THROW_NOT_OK(ArrowArrayInitFromType(array, NANOARROW_TYPE_STRUCT));
  NANOARROW_THROW_NOT_OK(ArrowArrayAllocateChildren(array, 2));
  NANOARROW_THROW_NOT_OK(
      ArrowArrayInitFromType(array->children[0], NANOARROW_TYPE_DOUBLE));
  NANOARROW_THROW_NOT_OK(
      ArrowArrayInitFromType(array->children[1], NANOARROW_TYPE_DOUBLE));
  nanoarrow::BufferInitSequence(ArrowArrayBuffer(array->children[0], 1), x);
  nanoarrow::BufferInitSequence(ArrowArrayBuffer(array->children[1], 1), y);
  array->length = x.size();
  array->children[0]->length = y.size();
  array->children[1]->length = y.size();
  NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(array, nullptr));
}

using s2geography::geoarrow::Reader;

TEST(GeoArrow, GeoArrowVersionTest) {
  EXPECT_STREQ(s2geography::geoarrow::version(), "0.2.0-SNAPSHOT");
}

TEST(GeoArrow, GeoArrowReaderErrorOnInit) {
  Reader reader;
  nanoarrow::UniqueSchema schema;

  EXPECT_THROW(reader.Init(schema.get()), s2geography::Exception);
}

TEST(GeoArrow, GeoArrowReaderReadWKTPoint) {
  Reader reader;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitArrayWKT(array.get(), {"POINT (0 1)", {}});

  reader.Init(Reader::InputType::kWKT, s2geography::geoarrow::ImportOptions());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 0);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(1, 0).ToPoint());
  EXPECT_EQ(result[1].get(), nullptr);
}

TEST(GeoArrow, GeoArrowReaderReadWKBPoint) {
  Reader reader;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  reader.Init(Reader::InputType::kWKB, s2geography::geoarrow::ImportOptions());
  InitArrayWKB(array.get(), {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x3e, 0x40, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40},
                             {}});

  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 0);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(10, 30).ToPoint());
  EXPECT_EQ(result[1].get(), nullptr);
}

TEST(GeoArrow, GeoArrowReaderReadGeoArrow) {
  Reader reader;
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitSchemaGeoArrowPoint(schema.get());
  InitArrayGeoArrowPoint(array.get(), {30, 40}, {10, 20});

  reader.Init(schema.get());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 0);
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(10, 30).ToPoint());
  EXPECT_EQ(result[1]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(20, 40).ToPoint());
}

TEST(GeoArrow, GeoArrowReaderReadWKTLinestring) {
  Reader reader;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitArrayWKT(array.get(), {"LINESTRING (0 1, 2 3)"});

  reader.Init(Reader::InputType::kWKT, s2geography::geoarrow::ImportOptions());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 1);
  ASSERT_EQ(result.size(), 1);
  auto shape = result[0]->Shape(0);
  ASSERT_EQ(shape->num_edges(), 1);
  EXPECT_EQ(shape->edge(0).v0, S2LatLng::FromDegrees(1, 0).ToPoint());
  EXPECT_EQ(shape->edge(0).v1, S2LatLng::FromDegrees(3, 2).ToPoint());
}

TEST(GeoArrow, GeoArrowReaderReadWKTPolygon) {
  Reader reader;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitArrayWKT(array.get(), {"POLYGON ((0 0, 1 0, 0 1, 0 0))"});

  reader.Init(Reader::InputType::kWKT, s2geography::geoarrow::ImportOptions());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 2);
  ASSERT_EQ(result.size(), 1);
  auto shape = result[0]->Shape(0);
  ASSERT_EQ(shape->num_edges(), 3);
  EXPECT_EQ(shape->edge(0).v0, S2LatLng::FromDegrees(0, 0).ToPoint());
  EXPECT_EQ(shape->edge(0).v1, S2LatLng::FromDegrees(0, 1).ToPoint());
  EXPECT_EQ(shape->edge(1).v0, S2LatLng::FromDegrees(0, 1).ToPoint());
  EXPECT_EQ(shape->edge(1).v1, S2LatLng::FromDegrees(1, 0).ToPoint());
  EXPECT_EQ(shape->edge(2).v0, S2LatLng::FromDegrees(1, 0).ToPoint());
  EXPECT_EQ(shape->edge(2).v1, S2LatLng::FromDegrees(0, 0).ToPoint());
}

TEST(GeoArrow, GeoArrowReaderReadWKTCollection) {
  Reader reader;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitArrayWKT(array.get(),
               {"GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 2 3))"});

  reader.Init(Reader::InputType::kWKT, s2geography::geoarrow::ImportOptions());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), -1);
  ASSERT_EQ(result.size(), 1);
  ASSERT_EQ(result[0]->num_shapes(), 2);

  auto point = result[0]->Shape(0);
  ASSERT_EQ(point->dimension(), 0);
  ASSERT_EQ(point->num_edges(), 1);
  EXPECT_EQ(point->edge(0).v0, S2LatLng::FromDegrees(1, 0).ToPoint());

  auto linestring = result[0]->Shape(1);
  ASSERT_EQ(linestring->dimension(), 1);
  ASSERT_EQ(linestring->num_edges(), 1);
  EXPECT_EQ(linestring->edge(0).v0, S2LatLng::FromDegrees(1, 0).ToPoint());
  EXPECT_EQ(linestring->edge(0).v1, S2LatLng::FromDegrees(3, 2).ToPoint());
}

using s2geography::geoarrow::Writer;

TEST(GeoArrow, GeoArrowWriterPoint) {
  s2geography::WKTReader reader;
  auto geog1 = reader.read_feature("POINT (0 1)");
  auto geog2 = reader.read_feature("POINT (2 3)");

  Writer writer;
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;

  InitSchemaGeoArrowPoint(schema.get());
  writer.Init(schema.get());

  writer.WriteGeography(*geog1);
  writer.WriteGeography(*geog2);
  writer.Finish(array.get());

  EXPECT_EQ(array->length, 2);

  // EXPECT_THAT(nanoarrow::ViewArrayAs<double_t>(array->children[0]), ElementsAre(DoubleEq(0.0), DoubleEq(2.0)));
  auto xs = reinterpret_cast<const double*>(array->children[0]->buffers[1]);
  EXPECT_DOUBLE_EQ(xs[0], 0.0);
  EXPECT_DOUBLE_EQ(xs[1], 2.0);
  // EXPECT_THAT(nanoarrow::ViewArrayAs<double_t>(array->children[1]), ElementsAre(DoubleEq(1.0), DoubleEq(3.0)));
  auto ys = reinterpret_cast<const double*>(array->children[1]->buffers[1]);
  EXPECT_DOUBLE_EQ(ys[0], 1.0);
  EXPECT_DOUBLE_EQ(ys[1], 3.0);
}

TEST(GeoArrow, GeoArrowWriterPointProjected) {
  s2geography::WKTReader reader;
  auto geog1 = reader.read_feature("POINT (0 1)");
  auto geog2 = reader.read_feature("POINT (2 3)");

  Writer writer;
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;

  s2geography::geoarrow::ExportOptions options;
  options.set_projection(s2geography::pseudo_mercator());
  InitSchemaGeoArrowPoint(schema.get());
  writer.Init(schema.get(), options);

  writer.WriteGeography(*geog1);
  writer.WriteGeography(*geog2);
  writer.Finish(array.get());

  EXPECT_EQ(array->length, 2);

  auto xs = reinterpret_cast<const double*>(array->children[0]->buffers[1]);
  EXPECT_DOUBLE_EQ(xs[0], 0.0);
  EXPECT_DOUBLE_EQ(xs[1], 222638.98158654661);
  auto ys = reinterpret_cast<const double*>(array->children[1]->buffers[1]);
  EXPECT_DOUBLE_EQ(ys[0], 111325.14286638441);
  EXPECT_DOUBLE_EQ(ys[1], 334111.17140195851);
}

TEST(GeoArrow, GeoArrowWriterPolylineTessellated) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("LINESTRING (-64 45, 0 45)");

  Writer writer;
  nanoarrow::UniqueSchema schema;
  GeoArrowSchemaInitExtension(schema.get(), GEOARROW_TYPE_LINESTRING);

  nanoarrow::UniqueArray array;
  writer.Init(schema.get());
  writer.WriteGeography(*geog);
  writer.Finish(array.get());

  EXPECT_EQ(array->length, 1);
  EXPECT_EQ(array->children[0]->length, 2);

  // with tessellation -> more coordinates
  nanoarrow::UniqueArray array2;
  s2geography::geoarrow::ExportOptions options;
  options.set_tessellate_tolerance(S1Angle::Radians(10000 / EARTH_RADIUS_METERS));
  writer.Init(schema.get(), options);
  writer.WriteGeography(*geog);
  writer.Finish(array2.get());

  EXPECT_EQ(array2->length, 1);
  EXPECT_GT(array2->children[0]->length, 2);
  EXPECT_EQ(array2->children[0]->length, 9);
  // first coordinate is still the same
  auto xs = reinterpret_cast<const double*>(array2->children[0]->children[0]->buffers[1]);
  EXPECT_DOUBLE_EQ(xs[0], -64);
  EXPECT_DOUBLE_EQ(xs[9], 0);
  auto ys = reinterpret_cast<const double*>(array2->children[0]->children[1]->buffers[1]);
  EXPECT_DOUBLE_EQ(ys[0], 45);
  EXPECT_DOUBLE_EQ(ys[8], 45);
  EXPECT_GT(ys[4], 45);

  // with tessellation and projection
  nanoarrow::UniqueArray array3;
  options.set_projection(s2geography::pseudo_mercator());
  writer.Init(schema.get(), options);
  writer.WriteGeography(*geog);
  writer.Finish(array3.get());

  EXPECT_EQ(array3->length, 1);
  EXPECT_GT(array3->children[0]->length, 2);
  EXPECT_EQ(array3->children[0]->length, 9);

  xs = reinterpret_cast<const double*>(array3->children[0]->children[0]->buffers[1]);
  EXPECT_NEAR(xs[0], -7124447.41, 0.01);
  EXPECT_DOUBLE_EQ(xs[9], 0);
  ys = reinterpret_cast<const double*>(array3->children[0]->children[1]->buffers[1]);
  EXPECT_NEAR(ys[0], 5621521.48, 0.01);
  EXPECT_NEAR(ys[8], 5621521.48, 0.01);
  EXPECT_GT(ys[4], 5621521);
}

TEST(GeoArrow, GeoArrowWriterPolygonTessellated) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("POLYGON ((-64 45, 0 45, 0 55, -64 55, -64 45))");

  Writer writer;
  nanoarrow::UniqueSchema schema;
  GeoArrowSchemaInitExtension(schema.get(), GEOARROW_TYPE_WKT);

  nanoarrow::UniqueArray array;
  writer.Init(schema.get());
  writer.WriteGeography(*geog);
  writer.Finish(array.get());

  EXPECT_EQ(array->length, 1);
  auto length_no_tesselation = reinterpret_cast<const int32*>(array->buffers[1])[1];

  // with tessellation -> more coordinates
  nanoarrow::UniqueArray array2;
  s2geography::geoarrow::ExportOptions options;
  options.set_tessellate_tolerance(S1Angle::Radians(10000 / EARTH_RADIUS_METERS));

  writer.Init(schema.get(), options);
  writer.WriteGeography(*geog);
  writer.Finish(array2.get());

  EXPECT_EQ(array2->length, 1);
  auto length_with_tesselation = reinterpret_cast<const int32*>(array2->buffers[1])[1];

  // dummy test to check that the WKT string length is larger with tesselation
  EXPECT_GT(length_with_tesselation, length_no_tesselation);
}

void TestGeoArrowRoundTrip(s2geography::Geography& geog, GeoArrowType type) {
  // writing
  Writer writer;
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;

  GeoArrowSchemaInitExtension(schema.get(), type);
  writer.Init(schema.get());
  writer.WriteGeography(geog);
  writer.Finish(array.get());

  EXPECT_EQ(array->length, 1);

  // reading back
  Reader reader;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  reader.Init(schema.get());
  reader.ReadGeography(array.get(), 0, array->length, &result);

  EXPECT_EQ(result[0]->dimension(), geog.dimension());
  EXPECT_EQ(result[0]->num_shapes(), geog.num_shapes());
  // TODO better assert equal for the geographies
}

TEST(GeoArrow, GeoArrowRoundtripPoint) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("POINT (30 10)");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_POINT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_POINT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}

TEST(GeoArrow, GeoArrowRoundtripLinestring) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("LINESTRING (30 10, 10 30, 40 40)");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_LINESTRING);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_LINESTRING);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}

TEST(GeoArrow, GeoArrowRoundtripPolygon) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_POLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_POLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);

  geog = reader.read_feature("POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10), (20 30, 35 35, 30 20, 20 30))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_POLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_POLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}

TEST(GeoArrow, GeoArrowRoundtripMultiPoint) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("MULTIPOINT ((10 40), (40 30), (20 20), (30 10))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_MULTIPOINT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_MULTIPOINT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}

TEST(GeoArrow, GeoArrowRoundtripMultiLinestring) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("MULTILINESTRING ((10 10, 20 20, 10 40), (40 40, 30 30, 40 20, 30 10))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_MULTILINESTRING);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_MULTILINESTRING);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}

TEST(GeoArrow, GeoArrowRoundtripMultiPolygon) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("MULTIPOLYGON (((30 20, 45 40, 10 40, 30 20)), ((15 5, 40 10, 10 20, 5 10, 15 5)))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_MULTIPOLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_MULTIPOLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);

  geog = reader.read_feature("MULTIPOLYGON (((40 40, 20 45, 45 30, 40 40)), ((20 35, 10 30, 10 10, 30 5, 45 20, 20 35), (30 20, 20 15, 20 25, 30 20)))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_MULTIPOLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_INTERLEAVED_MULTIPOLYGON);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}

TEST(GeoArrow, GeoArrowRoundtripCollection) {
  s2geography::WKTReader reader;
  auto geog = reader.read_feature("GEOMETRYCOLLECTION (POINT (40 10), LINESTRING (10 10, 20 20, 10 40), POLYGON ((40 40, 20 45, 45 30, 40 40)))");

  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKT);
  TestGeoArrowRoundTrip(*geog, GEOARROW_TYPE_WKB);
}
