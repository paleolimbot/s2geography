
#include <gtest/gtest.h>

#include "s2geography.h"

using namespace s2geography;

TEST(WKTWriter, SignificantDigits) {
  WKTReader reader;
  // Lat/lon is converted to XYZ here for the internals, so
  // we need to pick a value that will roundtrip with 16 digits of precision
  auto geog = reader.read_feature("POINT (0 3.333333333333334)");

  WKTWriter writer_default;
  EXPECT_EQ(writer_default.write_feature(*geog), "POINT (0 3.333333333333334)");

  WKTWriter writer_6digits(6);
  EXPECT_EQ(writer_6digits.write_feature(*geog), "POINT (0 3.33333)");

}

static std::string
wktRoundTrip(const std::string &wktIn) {
  WKTReader reader;
  WKTWriter writer;
  auto geog = reader.read_feature(wktIn.c_str());
  return writer.write_feature(*geog);
}

TEST(WKTWriter, EmptyGeometry) {
  std::vector<std::string> types =
    { "POINT EMPTY",
      "LINESTRING EMPTY",
      "POLYGON EMPTY",
      "GEOMETRYCOLLECTION EMPTY" };

    // Currently do not work
    // "MULTIPOINT EMPTY"
    // "MULTILINESTRING EMPTY"
    // "MULTIPOLYGON EMPTY"

  for (auto type: types) {
    EXPECT_EQ(wktRoundTrip(type), type);
  }
}

TEST(WKTWriter, InvalidLineString) {
  std::string wkt("LINESTRING (0 0)");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);
}

TEST(WKTWriter, MixedCollection) {
  std::string wkt("GEOMETRYCOLLECTION (LINESTRING (0 0), POINT EMPTY)");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);
}

TEST(WKTWriter, InvalidPolygon) {
  WKTReader reader;
  try {
    auto geog = reader.read_feature("POLYGON ((0 0, 1 1))");
  }
  catch (std::exception &e) {
    EXPECT_EQ(std::string(e.what()), "Loop 0: empty loops are not allowed");
  }
}
