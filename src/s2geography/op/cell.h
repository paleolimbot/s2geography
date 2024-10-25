#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "s2geography/op/op.h"
#include "s2geography/op/point.h"

namespace s2geography {

namespace op {

namespace cell {

using point::LngLat;
using point::Point;

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

/// \brief Create a cell identifier from an xyz unit vector
class FromPoint : public UnaryOp<uint64_t, Point> {
 public:
  uint64_t ExecuteScalar(Point point) override;
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

/// \brief Returns true if the ID is a valid cell identifier or false otherwise
class IsValid : public UnaryOp<bool, uint64_t> {
 public:
  bool ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Retrieve the corners of a cell
class CellCenter : public UnaryOp<Point, uint64_t> {
 public:
  Point ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Retrieve the corners of a cell
class CellVertex : public BinaryOp<Point, uint64_t, int8_t> {
 public:
  Point ExecuteScalar(const uint64_t cell_id, const int8_t vertex_id) override;
};

/// \brief Calculate the level represented by the cell
class Level : public UnaryOp<int8_t, uint64_t> {
 public:
  int8_t ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Calculate the area of a given cell
class Area : public UnaryOp<double, uint64_t> {
 public:
  double ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Calculate the approximate area of a given cell
class AreaApprox : public UnaryOp<double, uint64_t> {
 public:
  double ExecuteScalar(const uint64_t cell_id) override;
};

/// \brief Calculate the parent cell at a given level
class Parent : public BinaryOp<uint64_t, uint64_t, int8_t> {
 public:
  uint64_t ExecuteScalar(const uint64_t cell_id, const int8_t level) override;
};

/// \brief Calculate the child cell at the next level
class Child : public BinaryOp<uint64_t, uint64_t, int8_t> {
 public:
  uint64_t ExecuteScalar(const uint64_t cell_id, const int8_t k) override;
};

/// \brief Get the edge neighbours of a given cell
class EdgeNeighbor : public BinaryOp<uint64_t, uint64_t, int8_t> {
 public:
  uint64_t ExecuteScalar(const uint64_t cell_id, const int8_t k) override;
};

/// \brief Returns true if cell_id contains cell_id_test or false otherwise
class Contains : public BinaryOp<bool, uint64_t, uint64_t> {
 public:
  bool ExecuteScalar(const uint64_t cell_id,
                     const uint64_t cell_id_test) override;
};

/// \brief Returns true if cell_id might intersect cell_id_test or false
/// otherwise
class MayIntersect : public BinaryOp<bool, uint64_t, uint64_t> {
 public:
  bool ExecuteScalar(const uint64_t cell_id,
                     const uint64_t cell_id_test) override;
};

/// \brief Returns the minimum spherical distance (in radians) between two cells
class Distance : public BinaryOp<double, uint64_t, uint64_t> {
 public:
  double ExecuteScalar(const uint64_t cell_id,
                       const uint64_t cell_id_test) override;
};

/// \brief Returns the maximum spherical distance (in radians) between two cells
class MaxDistance : public BinaryOp<double, uint64_t, uint64_t> {
 public:
  double ExecuteScalar(const uint64_t cell_id,
                       const uint64_t cell_id_test) override;
};

/// \brief Returns the level at which the two cells have a common ancestor
class CommonAncestorLevel : public BinaryOp<int8_t, uint64_t, uint64_t> {
 public:
  int8_t ExecuteScalar(const uint64_t cell_id,
                       const uint64_t cell_id_test) override;
};

}  // namespace cell

}  // namespace op

}  // namespace s2geography
