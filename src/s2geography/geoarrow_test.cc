
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
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0]->Shape(0)->edge(0).v0,
            S2LatLng::FromDegrees(1, 0).ToPoint());
}
