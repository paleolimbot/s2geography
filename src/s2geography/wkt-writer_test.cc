
#include <gtest/gtest.h>

#include "s2geography.h"

using namespace s2geography;

// TEST(WKTWriter, SignificantDigits) {
//   WKTReader reader;
//   // Lat/lon is converted to XYZ here for the internals, so
//   // we need to pick a value that will roundtrip with 16 digits of precision
//   auto geog = reader.read_feature("POINT (0 3.333333333333334)");

//   WKTWriter writer_default;
//   EXPECT_EQ(writer_default.write_feature(*geog), "POINT (0 3.333333333333334)");

//   WKTWriter writer_6digits(6);
//   EXPECT_EQ(writer_6digits.write_feature(*geog), "POINT (0 3.33333)");

// }

static std::string
wktRoundTrip(const std::string &wktIn) {
  WKTReader reader;
  WKTWriter writer;
  auto geog = reader.read_feature(wktIn);
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

TEST(WKTWriter, LineString) {
  std::string wkt("LINESTRING (30 10, 10 30, 40 40)");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);
}

TEST(WKTWriter, Polygon) {
  std::string wkt("POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);

  std::string wkt2("POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10), (20 30, 35 35, 30 20, 20 30))");
  EXPECT_EQ(wktRoundTrip(wkt2), wkt2);
}

TEST(WKTWriter, MultiPoint) {
  std::string wkt("MULTIPOINT ((10 40), (40 30), (20 20), (30 10))");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);
}

TEST(WKTWriter, MultiLineString) {
  std::string wkt("MULTILINESTRING ((10 10, 20 20, 10 40), (40 40, 30 30, 40 20, 30 10))");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);
}

TEST(WKTWriter, MultiPolygon) {
  std::string wkt("MULTIPOLYGON (((30 20, 45 40, 10 40, 30 20)), ((15 5, 40 10, 10 20, 5 10, 15 5)))");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);

  std::string wkt2("MULTIPOLYGON (((40 40, 20 45, 45 30, 40 40)), ((20 35, 10 30, 10 10, 30 5, 45 20, 20 35), (30 20, 20 15, 20 25, 30 20)))");
  EXPECT_EQ(wktRoundTrip(wkt2), wkt2);
}

TEST(WKTWriter, Collection) {
  std::string wkt("GEOMETRYCOLLECTION (POINT (40 10), LINESTRING (10 10, 20 20, 10 40), POLYGON ((40 40, 20 45, 45 30, 40 40)))");
  EXPECT_EQ(wktRoundTrip(wkt), wkt);
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
