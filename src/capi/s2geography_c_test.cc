
#include "capi/s2geography_c.h"

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
  S2GeogErrorCode code = S2GeogErrorCreate(&err);
  ASSERT_EQ(code, S2GEOGRAPHY_OK);
  ASSERT_NE(err, nullptr);
  S2GeogErrorDestroy(err);
}

TEST(S2GeographyC, ErrorCreateNull) {
  // Creating with null pointer should return error
  S2GeogErrorCode code = S2GeogErrorCreate(nullptr);
  EXPECT_NE(code, S2GEOGRAPHY_OK);
}

TEST(S2GeographyC, ErrorGetMessage) {
  struct S2GeogError* err = nullptr;
  S2GeogErrorCreate(&err);

  // Fresh error should have empty message
  const char* msg = S2GeogErrorGetMessage(err);
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(strlen(msg), 0);

  S2GeogErrorDestroy(err);
}

TEST(S2GeographyC, ErrorGetMessageNull) {
  // Getting message from null error should return empty string
  const char* msg = S2GeogErrorGetMessage(nullptr);
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(strlen(msg), 0);
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
  S2GeogErrorCode code = S2GeogCreate(&geog);
  ASSERT_EQ(code, S2GEOGRAPHY_OK);
  ASSERT_NE(geog, nullptr);
  S2GeogDestroy(geog);
}

TEST(S2GeographyC, GeogCreateNull) {
  S2GeogErrorCode code = S2GeogCreate(nullptr);
  EXPECT_NE(code, S2GEOGRAPHY_OK);
}

// ============================================================================
// Geography Factory Tests
// ============================================================================

TEST(S2GeographyC, FactoryCreate) {
  struct S2GeogFactory* factory = nullptr;
  S2GeogErrorCode code = S2GeogFactoryCreate(&factory);
  ASSERT_EQ(code, S2GEOGRAPHY_OK);
  ASSERT_NE(factory, nullptr);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryCreateNull) {
  S2GeogErrorCode code = S2GeogFactoryCreate(nullptr);
  EXPECT_NE(code, S2GEOGRAPHY_OK);
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
  S2GeogFactoryCreate(&factory);

  struct S2Geog* geog = nullptr;
  S2GeogCreate(&geog);

  struct S2GeogError* err = nullptr;
  S2GeogErrorCreate(&err);

  S2GeogErrorCode code = S2GeogFactoryInitFromWkbNonOwning(
      factory, wkb_point, sizeof(wkb_point), geog, err);
  EXPECT_EQ(code, S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Rectangle Bounder Tests
// ============================================================================

TEST(S2GeographyC, RectBounderBound) {
  // WKB for POINT(10 20)
  const uint8_t wkb_point[] = {
      0x01,                    // byte order: little endian
      0x01, 0x00, 0x00, 0x00,  // type: Point (1)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40,  // x: 10.0
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x40   // y: 20.0
  };

  struct S2GeogFactory* factory = nullptr;
  S2GeogFactoryCreate(&factory);

  struct S2Geog* geog = nullptr;
  S2GeogCreate(&geog);

  struct S2GeogError* err = nullptr;
  S2GeogErrorCreate(&err);

  S2GeogFactoryInitFromWkbNonOwning(factory, wkb_point, sizeof(wkb_point), geog,
                                    err);

  struct S2GeogRectBounder* bounder = nullptr;
  S2GeogRectBounderCreate(&bounder);

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

TEST(S2GeographyC, NumKernels) {
  size_t num_kernels = S2GeogNumKernels();
  // Should have at least some kernels
  EXPECT_GT(num_kernels, 0);
  // Current implementation has 27 kernels
  EXPECT_EQ(num_kernels, 27);
}

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
