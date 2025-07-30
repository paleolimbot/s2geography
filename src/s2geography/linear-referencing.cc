
#include "s2geography/linear-referencing.h"

#include "s2geography/accessors.h"
#include "s2geography/arrow_udf/arrow_udf_internal.h"
#include "s2geography/build.h"
#include "s2geography/geography.h"

namespace s2geography {

double s2_project_normalized(const PolylineGeography& geog1,
                             const S2Point& point) {
  if (geog1.Polylines().size() != 1 || point.Norm2() == 0) {
    return NAN;
  }

  int next_vertex;
  S2Point point_on_line = geog1.Polylines()[0]->Project(point, &next_vertex);
  return geog1.Polylines()[0]->UnInterpolate(point_on_line, next_vertex);
}

double s2_project_normalized(const Geography& geog1, const Geography& geog2) {
  if (geog1.dimension() != 1 || geog2.dimension() != 0) {
    return NAN;
  }

  S2Point point;
  for (int i = 0; i < geog2.num_shapes(); i++) {
    auto shape = geog2.Shape(i);
    for (int j = 0; j < shape->num_edges(); j++) {
      if (point.Norm2() != 0) {
        return NAN;
      } else {
        point = shape->edge(j).v0;
      }
    }
  }

  auto geog1_poly_ptr = dynamic_cast<const PolylineGeography*>(&geog1);
  if (geog1_poly_ptr != nullptr) {
    return s2_project_normalized(*geog1_poly_ptr, point);
  }

  std::unique_ptr<Geography> geog_poly = s2_rebuild(geog1, GlobalOptions());
  return s2_project_normalized(*geog_poly, geog2);
}

S2Point s2_interpolate_normalized(const PolylineGeography& geog,
                                  double distance_norm) {
  if (s2_is_empty(geog)) {
    return S2Point();
  } else if (geog.Polylines().size() == 1) {
    return geog.Polylines()[0]->Interpolate(distance_norm);
  } else {
    throw Exception("`geog` must contain 0 or 1 polyines");
  }
}

S2Point s2_interpolate_normalized(const Geography& geog, double distance_norm) {
  if (s2_is_empty(geog)) {
    return S2Point();
  }

  if (geog.dimension() != 1 || geog.num_shapes() > 1) {
    throw Exception("`geog` must be a single polyline");
  }

  auto geog_poly_ptr = dynamic_cast<const PolylineGeography*>(&geog);
  if (geog_poly_ptr != nullptr) {
    return s2_interpolate_normalized(*geog_poly_ptr, distance_norm);
  }

  std::unique_ptr<Geography> geog_poly = s2_rebuild(geog, GlobalOptions());
  return s2_interpolate_normalized(*geog_poly, distance_norm);
}

namespace arrow_udf {

struct S2LineInterpolatePointExec {
  using arg0_t = GeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    stashed_ = s2_interpolate_normalized(value0, value1);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> LineInterpolatePoint() {
  return std::make_unique<BinaryUDF<S2LineInterpolatePointExec>>();
}

struct S2LineLocatePointExec {
  using arg0_t = GeographyInputView;
  using arg1_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_project_normalized(value0, value1);
  }
};

std::unique_ptr<ArrowUDF> LineLocatePoint() {
  return std::make_unique<BinaryUDF<S2LineLocatePointExec>>();
}

}  // namespace arrow_udf

}  // namespace s2geography
