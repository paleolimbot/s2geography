
#include "s2geography/op/cell.h"

#include <gtest/gtest.h>

#include <cmath>
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

TEST(Cell, CellCenter) {
  Point center = Execute<CellCenter>(TestCellId());
  LngLat center_pt = Execute<point::ToLngLat>(center);
  EXPECT_LT(std::abs(-64 - center_pt[0]), 0.0000001);
  EXPECT_LT(std::abs(45 - center_pt[1]), 0.0000001);

  Point invalid_point = Execute<CellCenter>(kCellIdSentinel);
  EXPECT_EQ(std::memcmp(&invalid_point, &point::kInvalidPoint, sizeof(Point)),
            0);
}

TEST(Cell, CellVertex) {}

TEST(Cell, Level) {
  EXPECT_EQ(Execute<Level>(TestCellId()), 30);
  EXPECT_EQ(Execute<Level>(kCellIdNone), -1);
  EXPECT_EQ(Execute<Level>(kCellIdSentinel), -1);
}

TEST(Cell, Area) {
  uint64_t face = Execute<Parent>(TestCellId(), 0);
  EXPECT_DOUBLE_EQ(Execute<Area>(face), 4 * M_PI / 6);
  EXPECT_TRUE(std::isnan(Execute<Area>(kCellIdNone)));
  EXPECT_TRUE(std::isnan(Execute<Area>(kCellIdSentinel)));
}

TEST(Cell, AreaApprox) {
  uint64_t face = Execute<Parent>(TestCellId(), 0);
  EXPECT_DOUBLE_EQ(Execute<AreaApprox>(face), 4 * M_PI / 6);
  EXPECT_TRUE(std::isnan(Execute<AreaApprox>(kCellIdNone)));
  EXPECT_TRUE(std::isnan(Execute<AreaApprox>(kCellIdSentinel)));
}

TEST(Cell, Parent) {
  EXPECT_EQ(Execute<Level>(Execute<Parent>(TestCellId(), 0)), 0);
  EXPECT_EQ(Execute<Level>(Execute<Parent>(TestCellId(), -1)), 29);
  EXPECT_EQ(Execute<Parent>(TestCellId(), 31), kCellIdSentinel);
  EXPECT_EQ(Execute<Parent>(kCellIdSentinel, 0), kCellIdSentinel);
}

TEST(Cell, EdgeNeighbor) {}

TEST(Cell, Contains) {
  EXPECT_TRUE(
      Execute<Contains>(Execute<Parent>(TestCellId(), -1), TestCellId()));
  EXPECT_FALSE(
      Execute<Contains>(TestCellId(), Execute<Parent>(TestCellId(), -1)));
  EXPECT_FALSE(Execute<Contains>(kCellIdSentinel, TestCellId()));
  EXPECT_FALSE(Execute<Contains>(TestCellId(), kCellIdSentinel));
}

TEST(Cell, MayIntersect) {
  EXPECT_TRUE(Execute<MayIntersect>(TestCellId(), TestCellId()));
  EXPECT_TRUE(
      Execute<MayIntersect>(TestCellId(), Execute<Parent>(TestCellId(), -1)));
  EXPECT_FALSE(Execute<MayIntersect>(TestCellId(),
                                     Execute<EdgeNeighbor>(TestCellId(), 0)));
}

TEST(Cell, Distance) {}

TEST(Cell, MaxDistance) {}

TEST(Cell, CommonAncestorLevel) {
  EXPECT_EQ(Execute<CommonAncestorLevel>(Execute<Parent>(TestCellId(), 5),
                                         TestCellId()),
            5);
  EXPECT_EQ(Execute<CommonAncestorLevel>(kCellIdSentinel, TestCellId()), -128);
}

}  // namespace s2geography::op::cell
