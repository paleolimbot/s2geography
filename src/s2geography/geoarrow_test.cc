
#include "geoarrow/geoarrow.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"

void InitSchemaWKT(ArrowSchema* schema) {
  NANOARROW_THROW_NOT_OK(
      GeoArrowSchemaInitExtension(schema, GEOARROW_TYPE_WKT));
}

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

void InitSchemaWKB(ArrowSchema* schema) {
  NANOARROW_THROW_NOT_OK(
      GeoArrowSchemaInitExtension(schema, GEOARROW_TYPE_WKB));
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
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitSchemaWKT(schema.get());
  InitArrayWKT(array.get(), {"POINT (0 1)"});

  reader.Init(schema.get());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 0);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(1, 0).ToPoint());
}

TEST(GeoArrow, GeoArrowReaderReadWKBPoint) {
  Reader reader;
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitSchemaWKB(schema.get());
  InitArrayWKB(array.get(), {{0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x3e, 0x40, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40}});

  reader.Init(schema.get());
  reader.ReadGeography(array.get(), 0, array->length, &result);
  EXPECT_EQ(result[0]->dimension(), 0);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(10, 30).ToPoint());
}

TEST(GeoArrow, GeoArrowReaderReadWKTLinestring) {
  Reader reader;
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitSchemaWKT(schema.get());
  InitArrayWKT(array.get(), {"LINESTRING (0 1, 2 3)"});

  reader.Init(schema.get());
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
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitSchemaWKT(schema.get());
  InitArrayWKT(array.get(), {"POLYGON ((0 0, 1 0, 0 1, 0 0))"});

  reader.Init(schema.get());
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
  nanoarrow::UniqueSchema schema;
  nanoarrow::UniqueArray array;
  std::vector<std::unique_ptr<s2geography::Geography>> result;

  InitSchemaWKT(schema.get());
  InitArrayWKT(array.get(),
               {"GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 2 3))"});

  reader.Init(schema.get());
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
