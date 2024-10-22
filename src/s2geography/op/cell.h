#pragma once

#include <array>
#include <string>
#include <string_view>

#include "s2geography/op/op.h"

namespace s2geography {

namespace op {

namespace cell {

using LngLat = std::array<double, 2>;
using Point = std::array<double, 3>;

static constexpr LngLat kInvalidLngLat{NAN, NAN};
static constexpr Point kInvalidPoint{NAN, NAN, NAN};
static constexpr uint64_t kCellIdNone = 0;
static constexpr uint64_t kCellIdSentinel = ~uint64_t{0};

class FromToken : public UnaryOp<uint64_t, std::string_view> {
 public:
  uint64_t ExecuteScalar(const std::string_view cell_token) override;
};

class FromDebugString : public UnaryOp<uint64_t, std::string_view> {
 public:
  uint64_t ExecuteScalar(const std::string_view debug_string) override;
};

class FromLngLat : public UnaryOp<uint64_t, LngLat> {
 public:
  uint64_t ExecuteScalar(LngLat lnglat) override;
};

class FromPoint : public UnaryOp<uint64_t, Point> {
 public:
  uint64_t ExecuteScalar(Point point) override;
};

class ToLngLat : public UnaryOp<LngLat, uint64_t> {
 public:
  LngLat ExecuteScalar(const uint64_t cell_id) override;
};

class ToPoint : public UnaryOp<Point, uint64_t> {
 public:
  Point ExecuteScalar(const uint64_t cell_id) override;
};

class ToToken : public UnaryOp<std::string_view, uint64_t> {
 public:
  std::string_view ExecuteScalar(const uint64_t cell_id) override;

 private:
  std::string last_result_;
};

class ToDebugString : public UnaryOp<std::string_view, uint64_t> {
 public:
  std::string_view ExecuteScalar(const uint64_t cell_id) override;

 private:
  std::string last_result_;
};

}  // namespace cell

}  // namespace op

}  // namespace s2geography
