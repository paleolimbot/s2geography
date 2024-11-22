#pragma once

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <vector>

#include "s2geography/geography.h"
#include "s2geography/wkt-writer.h"

// Define pretty printers for Geography. These seem to be required for
// subclasses as well to get the desired output on failure.
namespace s2geography {
inline void PrintTo(GeographyKind kind, std::ostream* os) {
  switch (kind) {
    case GeographyKind::UNINITIALIZED:
      *os << "GeographyKind::UNINITIALIZED";
      break;
    case GeographyKind::POINT:
      *os << "GeographyKind::POINT";
      break;
    case GeographyKind::POLYLINE:
      *os << "GeographyKind::POLYLINE";
      break;
    case GeographyKind::POLYGON:
      *os << "GeographyKind::POLYGON";
      break;
    case GeographyKind::GEOGRAPHY_COLLECTION:
      *os << "GeographyKind::GEOGRAPHY_COLLECTION";
      break;
    case GeographyKind::SHAPE_INDEX:
      *os << "GeographyKind::SHAPE_INDEX";
      break;
    case GeographyKind::ENCODED_SHAPE_INDEX:
      *os << "GeographyKind::ENCODED_SHAPE_INDEX";
      break;
    case GeographyKind::CELL_CENTER:
      *os << "GeographyKind::CELL_CENTER";
      break;
    default:
      *os << "Unknown GeographyKind <" << static_cast<int>(kind) << ">";
      break;
  }
}

inline void PrintTo(const Geography& geog, std::ostream* os) {
  WKTWriter writer(6);
  *os << writer.write_feature(geog);
}

inline void PrintTo(const PointGeography& geog, std::ostream* os) {
  WKTWriter writer;
  *os << writer.write_feature(geog);
}

inline void PrintTo(const PolylineGeography& geog, std::ostream* os) {
  WKTWriter writer(6);
  *os << writer.write_feature(geog);
}

inline void PrintTo(const PolygonGeography& geog, std::ostream* os) {
  WKTWriter writer(6);
  *os << writer.write_feature(geog);
}

inline void PrintTo(const GeographyCollection& geog, std::ostream* os) {
  WKTWriter writer(6);
  *os << writer.write_feature(geog);
}

inline void PrintTo(const ShapeIndexGeography& geog, std::ostream* os) {
  *os << "ShapeIndexGeography with " << geog.ShapeIndex().num_shape_ids()
      << " shapes";
}

inline void PrintTo(const EncodeOptions& obj, std::ostream* os) {
  *os << "EncodeOptions(";

  if (obj.coding_hint() == s2coding::CodingHint::COMPACT) {
    *os << "COMPACT, ";
  } else {
    *os << "FAST, ";
  }

  *os << "enable_lazy_decode: " << obj.enable_lazy_decode()
      << ", include_covering: " << obj.include_covering() << ")";
}

// Define a custom matcher for a geography being equal to some WKT, rounded
// to 6 decimal places. This is usually sufficient to ensure that integer
// lon/lat coordinates are equal.
MATCHER_P(WktEquals6, wkt, "") {
  WKTWriter writer(6);
  return writer.write_feature(arg) == std::string(wkt);
}
}  // namespace s2geography

static const char* kRoundtrippableWkt[] = {
    "POINT (30 10)",
    "POINT EMPTY",
    "LINESTRING (30 10, 10 30, 40 40)",
    "LINESTRING EMPTY",
    "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))",
    "POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10), (20 30, 35 35, 30 20, 20 "
    "30))",
    "POLYGON EMPTY",
    "MULTIPOINT ((10 40), (40 30), (20 20), (30 10))",
    "MULTILINESTRING ((10 10, 20 20, 10 40), (40 40, 30 30, 40 20, 30 10))",
    "MULTIPOLYGON (((30 20, 45 40, 10 40, 30 20)), ((15 5, 40 10, 10 20, 5 10, "
    "15 5)))",
    "MULTIPOLYGON (((40 40, 20 45, 45 30, 40 40)), ((20 35, 10 30, 10 10, 30 "
    "5, 45 20, 20 35), (30 20, 20 15, 20 25, 30 20)))",
    "GEOMETRYCOLLECTION (POINT (30 10))",
    "GEOMETRYCOLLECTION (LINESTRING (30 10, 10 30, 40 40))",
    "GEOMETRYCOLLECTION (POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "GEOMETRYCOLLECTION (POINT (30 10), LINESTRING (30 10, 10 30, 40 40), "
    "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (POINT (30 10)))",
    "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (LINESTRING (30 10, 10 30, 40 "
    "40)))",
    "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (POLYGON ((30 10, 40 40, 20 40, 10 "
    "20, 30 10))))",
    "GEOMETRYCOLLECTION EMPTY"};

static int kNumRoundtrippableWkt =
    sizeof(kRoundtrippableWkt) / sizeof(const char*);

static const char* kNonRoundtrippableWkt[] = {
    "MULTIPOINT ((30 10))",
    "MULTIPOINT EMPTY",
    "MULTILINESTRING ((30 10, 10 30, 40 40))",
    "MULTILINESTRING EMPTY",
    "MULTIPOLYGON (((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "MULTIPOLYGON EMPTY",
};
static int kNumNonRoundtrippableWkt =
    sizeof(kNonRoundtrippableWkt) / sizeof(const char*);

static const char* kNonRoundtrippableWktRoundtrip[] = {
    "POINT (30 10)",
    "POINT EMPTY",
    "LINESTRING (30 10, 10 30, 40 40)",
    "LINESTRING EMPTY",
    "POLYGON (((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "MULTIPOLYGON EMPTY",
};

static inline std::vector<std::string> TestWKT(const std::string& prefix = "") {
  std::vector<std::string> out;
  for (int i = 0; i < kNumRoundtrippableWkt; i++) {
    std::string item(kRoundtrippableWkt[i]);
    if (item.find(prefix) == 0) {
      out.push_back(item);
    }
  }
  return out;
}
