
#include "s2geography/op/cell.h"

#include <gtest/gtest.h>

#include <cstring>

namespace s2geography::op::cell {

static constexpr LngLat kTestPoint{-64, 45};

static uint64_t TestCellId() {
  Point pt = Execute<point::ToPoint>(kTestPoint);
  return Execute<FromPoint>(pt);
}

TEST(Cell, Token) {
  uint64_t cell_id = TestCellId();
  EXPECT_EQ(Execute<FromToken>(ExecuteString<ToToken>(cell_id)), cell_id);
  EXPECT_EQ(Execute<FromToken>("not a valid token"), kCellIdNone);
}

TEST(Cell, DebugString) {
  uint64_t cell_id = TestCellId();
  EXPECT_EQ(Execute<FromDebugString>(ExecuteString<ToDebugString>(cell_id)),
            cell_id);
  EXPECT_EQ(Execute<FromDebugString>("not a valid debug"), kCellIdNone);
}

TEST(Cell, Point) {
  uint64_t cell_id = TestCellId();
  EXPECT_EQ(Execute<FromPoint>(Execute<ToPoint>(cell_id)), cell_id);

  Point invalid_point = Execute<ToPoint>(kCellIdSentinel);
  EXPECT_EQ(std::memcmp(&invalid_point, &point::kInvalidPoint, sizeof(Point)),
            0);
}

TEST(Cell, IsValid) {
  EXPECT_TRUE(Execute<IsValid>(TestCellId()));
  EXPECT_FALSE(Execute<IsValid>(kCellIdSentinel));
  EXPECT_FALSE(Execute<IsValid>(kCellIdNone));
}

TEST(Cell, CellCenter) {}

TEST(Cell, CellVertex) {}

TEST(Cell, Level) {}

TEST(Cell, Area) {}

TEST(Cell, AreaApprox) {}

TEST(Cell, Parent) {}

TEST(Cell, EdgeNeighbor) {}

TEST(Cell, Contains) {}

TEST(Cell, MayIntersect) {}

TEST(Cell, Distance) {}

TEST(Cell, MaxDistance) {}

TEST(Cell, CommonAncestorLevel) {}

}  // namespace s2geography::op::cell
