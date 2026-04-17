
#include "s2geography_c.h"

#include <gtest/gtest.h>
#include <s2/s2cell_id.h>

#include <cstring>
#include <limits>
#include <vector>

// This test file performs "is it plugged in" level checks for all C API
// functions. The goal is to ensure that:
// 1. All functions are exported and linkable
// 2. Basic happy-path usage works
// 3. Functions return expected values for simple cases

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(S2GeographyC, ErrorCreate) {
  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);
  ASSERT_NE(err, nullptr);
  S2GeogErrorDestroy(err);
}

TEST(S2GeographyC, ErrorGetMessage) {
  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  // Fresh error should have empty message
  const char* msg = S2GeogErrorGetMessage(err);
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(strlen(msg), 0);

  S2GeogErrorDestroy(err);
}

// ============================================================================
// Cell Functions Tests
// ============================================================================

TEST(S2GeographyC, LngLatToCellId) {
  // Test with a valid coordinate (New York City area)
  struct S2GeogVertex vertex;
  vertex.v[0] = -73.9857;  // longitude
  vertex.v[1] = 40.7484;   // latitude
  vertex.v[2] = 0;
  vertex.v[3] = 0;

  uint64_t cell_id = S2GeogLngLatToCellId(&vertex);

  // Should return a valid (non-sentinel) cell ID
  EXPECT_NE(cell_id, S2CellId::Sentinel().id());
}

TEST(S2GeographyC, LngLatToCellIdNaN) {
  // Test with NaN coordinates - should return sentinel
  struct S2GeogVertex vertex;
  vertex.v[0] = std::numeric_limits<double>::quiet_NaN();
  vertex.v[1] = 40.0;

  EXPECT_EQ(S2GeogLngLatToCellId(&vertex), S2CellId::Sentinel().id());

  // Should return sentinel cell ID for NaN input
  // S2CellId::Sentinel().id() is expected here
  // The actual sentinel value - just verify it's consistent
  struct S2GeogVertex vertex2;
  vertex2.v[0] = 0.0;
  vertex2.v[1] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(S2GeogLngLatToCellId(&vertex2), S2CellId::Sentinel().id());
}

// ============================================================================
// Geography Accessors Tests
// ============================================================================

TEST(S2GeographyC, GeogCreate) {
  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);
  ASSERT_NE(geog, nullptr);

  // Should be able to force prepare a fresh geography
  ASSERT_EQ(S2GeogForcePrepare(geog, nullptr), S2GEOGRAPHY_OK);

  S2GeogDestroy(geog);
}

TEST(S2GeographyC, MemUsed) {
  const char* wkt_point = "POINT (0 0)";

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  // MemUsed should return something reasonable for an empty geography
  size_t mem_empty = S2GeogMemUsed(geog);
  EXPECT_GT(mem_empty, 0);

  // Initialize with a point
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt_point, strlen(wkt_point),
                                     geog, nullptr),
            S2GEOGRAPHY_OK);

  // MemUsed should return something reasonable for a point
  size_t mem_point = S2GeogMemUsed(geog);
  EXPECT_GT(mem_point, 0);

  // After forcing the index to build, memory should increase
  ASSERT_EQ(S2GeogForcePrepare(geog, nullptr), S2GEOGRAPHY_OK);
  size_t mem_prepared = S2GeogMemUsed(geog);
  EXPECT_GT(mem_prepared, mem_point);

  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Geography Factory Tests
// ============================================================================

TEST(S2GeographyC, FactoryCreate) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);
  ASSERT_NE(factory, nullptr);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromWkbPoint) {
  // WKB for POINT(0 0) - little endian
  const uint8_t wkb_point[] = {
      0x01,                    // byte order: little endian
      0x01, 0x00, 0x00, 0x00,  // type: Point (1)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 0.0
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // y: 0.0
  };

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  EXPECT_EQ(S2GeogFactoryInitFromWkbNonOwning(factory, wkb_point,
                                              sizeof(wkb_point), geog, err),
            S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromInvalidWkb) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(S2GeogFactoryInitFromWkbNonOwning(factory, nullptr, 0, geog, err),
            EINVAL);
  EXPECT_STREQ(S2GeogErrorGetMessage(err),
               "Expected endian byte but found end of buffer at byte 0");

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromWktPoint) {
  const char* wkt_point = "POINT (0 0)";

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  EXPECT_EQ(S2GeogFactoryInitFromWkt(factory, wkt_point, strlen(wkt_point),
                                     geog, err),
            S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromInvalidWkt) {
  const char* invalid_wkt = "NOT VALID WKT";

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  EXPECT_NE(S2GeogFactoryInitFromWkt(factory, invalid_wkt, strlen(invalid_wkt),
                                     geog, err),
            S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Rectangle Bounder Tests
// ============================================================================

TEST(S2GeographyC, RectBounderBound) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* wkt = "POINT (10 20)";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt, strlen(wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogRectBounder* bounder = nullptr;
  ASSERT_EQ(S2GeogRectBounderCreate(&bounder), S2GEOGRAPHY_OK);

  // Fresh bounder should be empty
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 1);

  // Bound a point
  S2GeogRectBounderBound(bounder, geog, err);

  // Should no longer be empty
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 0);

  struct S2GeogVertex lo, hi;
  EXPECT_EQ(S2GeogRectBounderFinish(bounder, &lo, &hi, err), S2GEOGRAPHY_OK);
  EXPECT_LE(lo.v[0], 10);
  EXPECT_GE(hi.v[0], 10);
  EXPECT_LE(lo.v[1], 20);
  EXPECT_GE(hi.v[1], 20);

  // If we clear, the bounder should be empty again
  S2GeogRectBounderClear(bounder);
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 1);

  S2GeogRectBounderDestroy(bounder);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Sedona UDF Interface Tests
// ============================================================================

TEST(S2GeographyC, NumKernels) { EXPECT_EQ(S2GeogNumKernels(), 27); }

TEST(S2GeographyC, InitKernelsInvalidFormat) {
  // Test with invalid format
  size_t num_kernels = S2GeogNumKernels();
  std::vector<char> buffer(num_kernels * 256);  // Oversized buffer

  // Invalid format (not S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF)
  S2GeogErrorCode code = S2GeogInitKernels(buffer.data(), buffer.size(), 999);
  EXPECT_NE(code, S2GEOGRAPHY_OK);
}

// ============================================================================
// Version Functions Tests
// ============================================================================

TEST(S2GeographyC, NanoarrowVersion) {
  const char* version = S2GeogNanoarrowVersion();
  ASSERT_NE(version, nullptr);
  // Should be a non-empty version string
  EXPECT_GT(strlen(version), 0);
  // Should contain at least one dot (like "0.5.0")
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, GeoArrowVersion) {
  const char* version = S2GeogGeoArrowVersion();
  ASSERT_NE(version, nullptr);
  EXPECT_GT(strlen(version), 0);
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, OpenSSLVersion) {
  const char* version = S2GeogOpenSSLVersion();
  ASSERT_NE(version, nullptr);
  EXPECT_GT(strlen(version), 0);
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, S2GeometryVersion) {
  const char* version = S2GeogS2GeometryVersion();
  ASSERT_NE(version, nullptr);
  EXPECT_GT(strlen(version), 0);
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, AbseilVersion) {
  const char* version = S2GeogAbseilVersion();
  ASSERT_NE(version, nullptr);
  // Could be a version string or "<live at head>"
  EXPECT_GT(strlen(version), 0);
}

// ============================================================================
// Binary Predicate Operations Tests (Parameterized)
// ============================================================================

struct BinaryPredicateParam {
  const char* name;
  int op_id;
  const char* lhs_wkt;
  const char* rhs_wkt;
  int64_t expected;
};

class BinaryPredicateTest
    : public ::testing::TestWithParam<BinaryPredicateParam> {};

TEST_P(BinaryPredicateTest, EvalGeogGeog) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.lhs_wkt, strlen(p.lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.rhs_wkt, strlen(p.rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_BOOL);

  ASSERT_EQ(S2GeogOpEvalGeogGeog(op, lhs, rhs, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpGetInt(op), p.expected);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, BinaryPredicateTest,
    ::testing::Values(BinaryPredicateParam{"intersects",
                                           S2GEOGRAPHY_OP_INTERSECTS,
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                           "POINT (0.25 0.25)", 1},
                      BinaryPredicateParam{"contains", S2GEOGRAPHY_OP_CONTAINS,
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                           "POINT (0.25 0.25)", 1},
                      BinaryPredicateParam{"within", S2GEOGRAPHY_OP_WITHIN,
                                           "POINT (0.25 0.25)",
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))", 1},
                      BinaryPredicateParam{"equals", S2GEOGRAPHY_OP_EQUALS,
                                           "POLYGON ((0 0, 1 0, 0 1, 0 0))",
                                           "POLYGON ((1 0, 0 1, 0 0, 1 0))",
                                           1}),
    [](const ::testing::TestParamInfo<BinaryPredicateParam>& info) {
      return info.param.name;
    });

// ============================================================================
// DistanceWithin Operation Test
// ============================================================================

TEST(S2GeographyC, DistanceWithinOperation) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  // Points ~111km apart (1 degree latitude)
  const char* lhs_wkt = "POINT (0 0)";
  const char* rhs_wkt = "POINT (0 1)";
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, lhs_wkt, strlen(lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, rhs_wkt, strlen(rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_DISTANCE_WITHIN),
            S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), "distance_within");
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_BOOL);

  // Distance is ~111195 meters, so 200000 meters should return true
  ASSERT_EQ(S2GeogOpEvalGeogGeogDouble(op, lhs, rhs, 200000.0, err),
            S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpGetInt(op), 1);

  // 50000 meters should return false
  ASSERT_EQ(S2GeogOpEvalGeogGeogDouble(op, lhs, rhs, 50000.0, err),
            S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpGetInt(op), 0);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}
