#pragma once

#include <array>
#include <cmath>

#include "s2geography/op/op.h"

namespace s2geography {

namespace op {

namespace point {

/// \brief Longitude/Latitude pair (degrees)
using LngLat = std::array<double, 2>;

/// \brief XYZ unit vector tuple
using Point = std::array<double, 3>;

/// \brief Sentinel LngLat returned for invalid cells
static constexpr LngLat kInvalidLngLat{NAN, NAN};

/// \brief Senintel Point returned for invalid cells
static constexpr Point kInvalidPoint{NAN, NAN, NAN};

/// \brief Convert a point to its longitude/latitude
class ToLngLat : public UnaryOp<LngLat, Point> {
 public:
  LngLat ExecuteScalar(const Point point) override;
};

/// \brief Convert a longitude/latitude to a point
class ToPoint : public UnaryOp<Point, LngLat> {
 public:
  Point ExecuteScalar(const LngLat lnglat) override;
};

}  // namespace point

}  // namespace op

}  // namespace s2geography
