#pragma once

#include <array>
#include <string>
#include <string_view>

#include "s2geography/op/op.h"

namespace s2geography {

namespace op {

namespace cell {

/// \brief Longitude/Latitude pair (degrees)
using LngLat = std::array<double, 2>;

/// \brief XYZ unit vector tuple
using Point = std::array<double, 3>;

/// \brief Sentinel LngLat returned for invalid cells
static constexpr LngLat kInvalidLngLat{NAN, NAN};

/// \brief Senintel Point returned for invalid cells
static constexpr Point kInvalidPoint{NAN, NAN, NAN};

/// \brief Cell identifier returned for invalid input
static constexpr uint64_t kCellIdNone = 0;

/// \brief Cell identifier that is greater than all other cells
static constexpr uint64_t kCellIdSentinel = ~uint64_t{0};

/// \brief Create a cell identifier from a token
class FromToken : public UnaryOp<uint64_t, std::string_view> {
 public:
  uint64_t ExecuteScalar(const std::string_view cell_token) override;
};

/// \brief Create a cell identifier from a debug string
class FromDebugString : public UnaryOp<uint64_t, std::string_view> {
 public:
  uint64_t ExecuteScalar(const std::string_view debug_string) override;
};

/// \brief Create a cell identifier from a longitude/latitude pair (in degrees)
class FromLngLat : public UnaryOp<uint64_t, LngLat> {
 public:
  uint64_t ExecuteScalar(LngLat lnglat) override;
};

/// \brief Create a cell identifier from an xyz unit vector
class FromPoint : public UnaryOp<uint64_t, Point> {
 public:
  uint64_t ExecuteScalar(Point point) override;
};

/// \brief Calculate the cell centre in longitude/latitude (degrees)
class ToLngLat : public UnaryOp<LngLat, uint64_t> {
 public:
  LngLat ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Calculate the cell centre as an xyz vector
class ToPoint : public UnaryOp<Point, uint64_t> {
 public:
  Point ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Get the token string of a cell identifier
class ToToken : public UnaryOp<std::string_view, uint64_t> {
 public:
  std::string_view ExecuteScalar(const uint64_t cell_id) override;

 private:
  std::string last_result_;
};

/// \brief Get the debug string of a cell identifier
class ToDebugString : public UnaryOp<std::string_view, uint64_t> {
 public:
  std::string_view ExecuteScalar(const uint64_t cell_id) override;

 private:
  std::string last_result_;
};

}  // namespace cell

}  // namespace op

}  // namespace s2geography
