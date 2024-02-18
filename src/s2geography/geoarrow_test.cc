
#include <gtest/gtest.h>

#include "s2geography.h"

using s2geography::geoarrow::Reader;

TEST(GeoArrow, GeoArrowVersionTest) {
  EXPECT_STREQ(s2geography::geoarrow::version(), "0.2.0-SNAPSHOT");
}

TEST(GeoArrow, GeoArrowReaderBasic) {
  Reader reader;
  ArrowSchema schema;
  schema.release = nullptr;

  EXPECT_THROW(reader.Init(&schema), s2geography::Exception);
}
