
#include "s2geography/linear-referencing.h"

#include <s2/s2edge_distances.h>

#include <algorithm>

#include "s2geography/accessors.h"
#include "s2geography/build.h"
#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

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

namespace sedona_udf {

static constexpr int kMaxEdgesLinearSearch = 32;

struct S2LineInterpolatePointExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowOutputBuilder;

  void Exec(arg0_t::c_type value0, arg1_t::c_type fraction, out_t* out) {
    if (value0.is_empty()) {
      out->AppendNull();
      return;
    }

    if (!value0.points()->is_empty() || !value0.polygons()->is_empty() ||
        value0.lines()->num_chains() != 1) {
      throw Exception(
          "Input to ST_LineInterpolatePoint must be a single linestring");
    }

    out->SetDimensions(value0.dimensions());

    if (fraction <= 0) {
      internal::GeoArrowVertex pt;
      value0.lines()->geom().VisitNativeVertices(
          [&](const internal::GeoArrowVertex& v) {
            pt = v;
            return false;
          });

      out->AppendPoint(pt, value0.dimensions());
      return;
    } else if (fraction >= 1) {
      auto pt = value0.lines()->native_edge(value0.lines()->num_edges() - 1).v1;

      out->AppendPoint(pt, value0.dimensions());
      return;
    }

    S1Angle length_sum;
    cumulative_lengths_.clear();
    cumulative_lengths_.push_back(length_sum);
    value0.VisitEdges([&](const S2Shape::Edge& e) {
      length_sum += S1Angle(e.v0, e.v1);
      cumulative_lengths_.push_back(length_sum);
      return true;
    });

    if (length_sum.radians() == 0) {
      internal::GeoArrowVertex pt;
      value0.lines()->geom().VisitNativeVertices(
          [&](const internal::GeoArrowVertex& v) {
            pt = v;
            return false;
          });

      out->AppendPoint(pt, value0.dimensions());
      return;
    }

    S1Angle target = fraction * length_sum;
    int num_edges = value0.lines()->num_edges();
    int edge_idx;
    if (num_edges <= kMaxEdgesLinearSearch) {
      for (size_t i = 1; i < cumulative_lengths_.size(); ++i) {
        if (target < cumulative_lengths_[i]) {
          edge_idx = static_cast<int>(i) - 1;
          break;
        }
      }
    } else {
      auto it = std::lower_bound(cumulative_lengths_.begin(),
                                 cumulative_lengths_.end(), target);
      edge_idx = static_cast<int>(it - cumulative_lengths_.begin()) - 1;
    }

    S1Angle prev_length = cumulative_lengths_[edge_idx];
    S1Angle next_length = cumulative_lengths_[edge_idx + 1];
    S1Angle edge_length = next_length - prev_length;
    double edge_fraction;
    if (edge_length == S1Angle::Zero()) {
      edge_fraction = 0;
    } else {
      S1Angle remaining = target - prev_length;
      edge_fraction = remaining / edge_length;
    }

    auto native_edge = value0.lines()->native_edge(edge_idx);
    auto pt = native_edge.Interpolate(edge_fraction);

    out->AppendPoint(pt, value0.dimensions());
  }

  std::vector<S1Angle> cumulative_lengths_;
};

struct S2LineLocatePointExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Exec(arg0_t::c_type value0, arg1_t::c_type value1, out_t* out) {
    if (value0.is_empty() || value1.is_empty()) {
      out->AppendNull();
      return;
    }

    if (!value0.points()->is_empty() || !value0.polygons()->is_empty() ||
        value0.lines()->num_chains() != 1) {
      throw Exception(
          "First argument to ST_LineLocatePoint must be a single linestring");
    }

    auto maybe_point = value1.Point();
    if (!maybe_point) {
      throw Exception("Second argument to ST_LineLocatePoint must be a point");
    }

    S2Point pt = *maybe_point;

    // This section always does a linear search. In theory some of this search
    // could be reused for a scalar input across multiple query points; however,
    // it's unclear that this would be a common query pattern.
    S1Angle min_distance_to_segment = S1Angle::Infinity();
    int closest_edge_id = -1;

    S1Angle length_sum;
    cumulative_lengths_.clear();
    cumulative_lengths_.push_back(length_sum);
    int edge_id = -1;
    value0.VisitEdges([&](const S2Shape::Edge& e) {
      ++edge_id;

      length_sum += S1Angle(e.v0, e.v1);
      cumulative_lengths_.push_back(length_sum);

      S1Angle distance_to_segment = S2::GetDistance(pt, e.v0, e.v1);
      if (distance_to_segment < min_distance_to_segment) {
        closest_edge_id = edge_id;
        min_distance_to_segment = distance_to_segment;
      }

      return true;
    });

    if (length_sum == S1Angle::Zero()) {
      out->Append(0.0);
      return;
    }

    S2GEOGRAPHY_DCHECK_GE(closest_edge_id, 0);
    S2Shape::Edge e = value0.lines()->edge(closest_edge_id);
    S2Point pt_on_edge = S2::Project(pt, e.v0, e.v1);
    S1Angle e_distance(e.v0, pt_on_edge);
    S1Angle total_distance = cumulative_lengths_[closest_edge_id] + e_distance;

    out->Append(total_distance / length_sum);
  }

  std::vector<S1Angle> cumulative_lengths_;
};

void LineInterpolatePointKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2LineInterpolatePointExec>(out, "st_lineinterpolatepoint");
}

void LineLocatePointKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2LineLocatePointExec>(out, "st_linelocatepoint");
}

}  // namespace sedona_udf

}  // namespace s2geography
