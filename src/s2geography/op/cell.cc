
#include "s2geography/op/cell.h"

#include <array>
#include <cstdint>
#include <string_view>

#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"

namespace s2geography::op::cell {

uint64_t FromToken::ExecuteScalar(const std::string_view cell_token) {
  // This constructor may not work for all s2 versions
  return S2CellId::FromToken({cell_token.data(), cell_token.size()}).id();
}

uint64_t FromDebugString::ExecuteScalar(const std::string_view debug_string) {
  // This constructor may not work for all s2 versions
  return S2CellId::FromDebugString({debug_string.data(), debug_string.size()})
      .id();
}

uint64_t FromPoint::ExecuteScalar(Point point) {
  S2Point pt(point[0], point[1], point[2]);
  return S2CellId(pt).id();
}

Point ToPoint::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return point::kInvalidPoint;
  } else {
    S2Point point = S2CellId(cell_id).ToPoint();
    return {point.x(), point.y(), point.z()};
  }
}

std::string_view ToToken::ExecuteScalar(const uint64_t cell_id) {
  last_result_ = S2CellId(cell_id).ToToken();
  return last_result_;
}

std::string_view ToDebugString::ExecuteScalar(const uint64_t cell_id) {
  last_result_ = S2CellId(cell_id).ToString();
  return last_result_;
}

bool IsValid::ExecuteScalar(const uint64_t cell_id) {
  return S2CellId(cell_id).is_valid();
}

Point CellCenter::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return point::kInvalidPoint;
  }

  S2Point pt = S2Cell(cell).GetCenter();
  return {pt.x(), pt.y(), pt.z()};
}

Point CellVertex::ExecuteScalar(const uint64_t cell_id,
                                const int8_t vertex_id) {
  S2CellId cell(cell_id);

  if (vertex_id < 0 || !cell.is_valid()) {
    return point::kInvalidPoint;
  }

  S2Point pt = S2Cell(cell).GetVertex(vertex_id);
  return {pt.x(), pt.y(), pt.z()};
}

int8_t Level::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return -1;
  }

  return cell.level();
}

double Area::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return NAN;
  }

  return S2Cell(cell).ExactArea();
}

double AreaApprox::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return NAN;
  }

  return S2Cell(cell).ApproxArea();
}

uint64_t Parent::ExecuteScalar(const uint64_t cell_id, const int8_t level) {
  // allow negative numbers to relate to current level
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return kCellIdSentinel;
  }

  int8_t level_final;
  int8_t cell_level = cell.level();
  if (level < 0) {
    level_final = cell.level() + level;
  } else {
    level_final = level;
  }

  if (level_final > cell.level() || level_final < 0) {
    return kCellIdSentinel;
  } else {
    return cell.parent(level_final).id();
  }
}

uint64_t Child::ExecuteScalar(const uint64_t cell_id, const int8_t k) {
  if (k < 0 || k > 3) {
    return kCellIdSentinel;
  }

  return S2CellId(cell_id).child(k).id();
}

uint64_t EdgeNeighbor::ExecuteScalar(const uint64_t cell_id, const int8_t k) {
  S2CellId cell(cell_id);
  if (k < 0 || k > 3) {
    return kCellIdSentinel;
  }

  S2CellId neighbours[4];
  cell.GetEdgeNeighbors(neighbours);
  return neighbours[k].id();
}

bool Contains::ExecuteScalar(const uint64_t cell_id,
                             const uint64_t cell_id_test) {
  S2CellId cell(cell_id);
  S2CellId cell_test(cell_id_test);
  if (!cell.is_valid() || !cell_test.is_valid()) {
    return false;
  }

  return cell.contains(cell_test);
}

bool MayIntersect::ExecuteScalar(const uint64_t cell_id,
                                 const uint64_t cell_id_test) {
  S2CellId cell(cell_id);
  S2CellId cell_test(cell_id_test);
  if (!cell.is_valid() || !cell_test.is_valid()) {
    return false;
  }

  return S2Cell(cell).MayIntersect(S2Cell(cell_test));
}

double Distance::ExecuteScalar(const uint64_t cell_id,
                               const uint64_t cell_id_test) {
  S2CellId cell(cell_id);
  S2CellId cell_test(cell_id_test);
  if (!cell.is_valid() || !cell_test.is_valid()) {
    return NAN;
  }

  return S2Cell(cell).GetDistance(S2Cell(cell_test)).radians();
}

double MaxDistance::ExecuteScalar(const uint64_t cell_id,
                                  const uint64_t cell_id_test) {
  S2CellId cell(cell_id);
  S2CellId cell_test(cell_id_test);
  if (!cell.is_valid() || !cell_test.is_valid()) {
    return NAN;
  }

  return S2Cell(cell).GetMaxDistance(S2Cell(cell_test)).radians();
}

int8_t CommonAncestorLevel::ExecuteScalar(const uint64_t cell_id,
                                          const uint64_t cell_id_test) {
  S2CellId cell(cell_id);
  S2CellId cell_test(cell_id_test);
  if (!cell.is_valid() || !cell_test.is_valid()) {
    return -1;
  }

  return cell.GetCommonAncestorLevel(cell_test);
}

}  // namespace s2geography::op::cell
