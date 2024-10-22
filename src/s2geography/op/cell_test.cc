
#include "s2geography/op/cell.h"

#include <gtest/gtest.h>

#include <cstring>

namespace s2geography::op::cell {

static constexpr LngLat kTestPoint{-64, 45};

static uint64_t TestCellId() {
  FromLngLat from_lng_lat;
  return from_lng_lat.ExecuteScalar(kTestPoint);
}

TEST(Cell, Token) {
  FromToken from_token;
  ToToken to_token;

  uint64_t cell_id = TestCellId();
  EXPECT_EQ(from_token.ExecuteScalar(to_token.ExecuteScalar(cell_id)), cell_id);
  EXPECT_EQ(from_token.ExecuteScalar("not a valid token"), kCellIdNone);
}

TEST(Cell, DebugString) {
  FromDebugString from_debug;
  ToDebugString to_debug;

  uint64_t cell_id = TestCellId();
  EXPECT_EQ(from_debug.ExecuteScalar(to_debug.ExecuteScalar(cell_id)), cell_id);
  EXPECT_EQ(from_debug.ExecuteScalar("not a valid debug"), kCellIdNone);
}

TEST(Cell, Point) {
  FromPoint from_point;
  ToPoint to_point;

  uint64_t cell_id = TestCellId();
  EXPECT_EQ(from_point.ExecuteScalar(to_point.ExecuteScalar(cell_id)), cell_id);

  Point invalid_point = to_point.ExecuteScalar(kCellIdSentinel);
  EXPECT_EQ(std::memcmp(&invalid_point, &kInvalidPoint, sizeof(Point)), 0);
}

TEST(Cell, LngLat) {
  FromPoint from_lnglat;
  ToPoint to_lnglat;

  uint64_t cell_id = TestCellId();
  EXPECT_EQ(from_lnglat.ExecuteScalar(to_lnglat.ExecuteScalar(cell_id)),
            cell_id);

  Point invalid_lnglat = to_lnglat.ExecuteScalar(kCellIdSentinel);
  EXPECT_EQ(std::memcmp(&invalid_lnglat, &kInvalidLngLat, sizeof(LngLat)), 0);
}

}  // namespace s2geography::op::cell
