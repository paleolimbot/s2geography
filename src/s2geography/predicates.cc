
#include "s2geography/predicates.h"

#include <s2/s2boolean_operation.h>
#include <s2/s2edge_tessellator.h>
#include <s2/s2lax_loop_shape.h>

#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

bool s2_intersects(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2,
                   const S2BooleanOperation::Options& options) {
  return s2_intersects(geog1.ShapeIndex(), geog2.ShapeIndex(), options);
}

bool s2_intersects(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
                   const S2BooleanOperation::Options& options) {
  return S2BooleanOperation::Intersects(geog1, geog2, options);
}

bool s2_equals(const ShapeIndexGeography& geog1,
               const ShapeIndexGeography& geog2,
               const S2BooleanOperation::Options& options) {
  return s2_equals(geog1.ShapeIndex(), geog2.ShapeIndex(), options);
}

bool s2_equals(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
               const S2BooleanOperation::Options& options) {
  return S2BooleanOperation::Equals(geog1, geog2, options);
}

bool s2_contains(const ShapeIndexGeography& geog1,
                 const ShapeIndexGeography& geog2,
                 const S2BooleanOperation::Options& options) {
  return s2_contains(geog1.ShapeIndex(), geog2.ShapeIndex(), options);
}

bool s2_contains(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
                 const S2BooleanOperation::Options& options) {
  if (geog2.num_shape_ids() == 0) {
    return false;
  }

  for (int i = 0; i < geog2.num_shape_ids(); i++) {
    const S2Shape* shape = geog2.shape(i);
    if (shape != nullptr && !shape->is_empty()) {
      return S2BooleanOperation::Contains(geog1, geog2, options);
    }
  }

  return false;
}

// Note that 'touches' can be implemented using:
//
// S2BooleanOperation::Options closedOptions = options;
// closedOptions.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
// closedOptions.set_polyline_model(S2BooleanOperation::PolylineModel::CLOSED);
// S2BooleanOperation::Options openOptions = options;
// openOptions.set_polygon_model(S2BooleanOperation::PolygonModel::OPEN);
// openOptions.set_polyline_model(S2BooleanOperation::PolylineModel::OPEN);
// s2_intersects(geog1, geog2, closed_options) &&
//   !s2_intersects(geog1, geog2, open_options);
//
// ...it isn't implemented here because the options creation should be done
// outside of any loop.

bool s2_intersects_box(const ShapeIndexGeography& geog1,
                       const S2LatLngRect& rect,
                       const S2BooleanOperation::Options& options,
                       double tolerance) {
  return s2_intersects_box(geog1.ShapeIndex(), rect, options, tolerance);
}

bool s2_intersects_box(const S2ShapeIndex& geog1, const S2LatLngRect& rect,
                       const S2BooleanOperation::Options& options,
                       double tolerance) {
  // 99% of this is making a S2Loop out of a S2LatLngRect
  // This should probably be implemented elsewhere
  S2::PlateCarreeProjection projection(180);
  S2EdgeTessellator tessellator(&projection, S1Angle::Degrees(tolerance));
  std::vector<S2Point> vertices;

  tessellator.AppendUnprojected(
      R2Point(rect.lng_lo().degrees(), rect.lat_lo().degrees()),
      R2Point(rect.lng_hi().degrees(), rect.lat_lo().degrees()), &vertices);
  tessellator.AppendUnprojected(
      R2Point(rect.lng_hi().degrees(), rect.lat_lo().degrees()),
      R2Point(rect.lng_hi().degrees(), rect.lat_hi().degrees()), &vertices);
  tessellator.AppendUnprojected(
      R2Point(rect.lng_hi().degrees(), rect.lat_hi().degrees()),
      R2Point(rect.lng_lo().degrees(), rect.lat_hi().degrees()), &vertices);
  tessellator.AppendUnprojected(
      R2Point(rect.lng_lo().degrees(), rect.lat_hi().degrees()),
      R2Point(rect.lng_lo().degrees(), rect.lat_lo().degrees()), &vertices);

  vertices.pop_back();

  auto loop = absl::make_unique<S2LaxLoopShape>(std::move(vertices));
  MutableS2ShapeIndex index;
  index.Add(std::move(loop));

  return S2BooleanOperation::Intersects(geog1, index, options);
}

namespace sedona_udf {

struct S2Intersects {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {
    // Use Simple Features compatibility options
    options_.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
  }

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    // If either argument is EMPTY, the result is FALSE
    if (value0.is_empty() || value1.is_empty()) {
      return false;
    }

    // The containment test for a S2Point is ~20x faster than building an index
    // containing exactly one point for every element so we try very hard to
    // avoid it.
    auto maybe_point0 = value0.Point();
    auto maybe_point1 = value1.Point();
    if (maybe_point0 && maybe_point1) {
      return maybe_point0->Normalize() == maybe_point1->Normalize();
    } else if (maybe_point0) {
      auto region1 = value1.Region();
      if (!region1->MayIntersect(S2Cell(*maybe_point0))) {
        return false;
      }
      return region1->Contains(*maybe_point0) ||
             ExecUsingShapeIndex(value0, value1);
    } else if (maybe_point1) {
      auto region0 = value0.Region();
      if (!region0->MayIntersect(S2Cell(*maybe_point1))) {
        return false;
      }
      return region0->Contains(*maybe_point1) ||
             ExecUsingShapeIndex(value0, value1);
    }

    // We could consider special casing more things here. S2Geometry special
    // cases loops with less than 32 vertices to avoid building an index which
    // is likely worth doing here based on some heuristics for other geometry
    // types too.

    // Next we try a covering intersection check. This is very cheap if an index
    // has already been built. In the event that an index does have to be built
    // to build the covering, it is effectively reused in the actual
    // s2_intersection() check. This is 2x faster than an intersection check for
    // selective point-in-polygon queries but may need to be reevaluated.
    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      return false;
    }

    return ExecUsingShapeIndex(value0, value1);
  }

  bool ExecUsingShapeIndex(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_intersects(value0.ShapeIndex(), value1.ShapeIndex(), options_);
  }

  S2BooleanOperation::Options options_;
  std::vector<S2CellId> intersection_;
};

struct S2Contains {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    // If either argument is EMPTY, the result is FALSE
    if (value0.is_empty() || value1.is_empty()) {
      return false;
    }

    auto maybe_point0 = value0.Point();
    auto maybe_point1 = value1.Point();
    if (maybe_point0) {
      // A point cannot contain anything
      return false;
    } else if (maybe_point1) {
      auto region0 = value0.Region();
      return region0->MayIntersect(S2Cell(*maybe_point1)) &&
             region0->Contains(*maybe_point1);
    }

    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      return false;
    }

    return ExecUsingShapeIndex(value0, value1);
  }

  bool ExecUsingShapeIndex(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_contains(value0.ShapeIndex(), value1.ShapeIndex(), options_);
  }

  S2BooleanOperation::Options options_;
  std::vector<S2CellId> intersection_;
};

struct S2Equals {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {
    options_.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
  }

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    if (GeographyIdentical(value0, value1)) {
      return true;
    }

    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      return false;
    }

    return s2_equals(value0.ShapeIndex(), value1.ShapeIndex(), options_);
  }

  bool GeographyIdentical(GeoArrowGeography& value0,
                          GeoArrowGeography& value1) {
    if (value0.num_shapes() != value1.num_shapes() == 1) {
      return false;
    }

    for (int i = 0; i < value0.num_shapes(); ++i) {
      if (!ShapeIdentical(value0.Shape(i), value1.Shape(i))) {
        return false;
      }
    }

    return true;
  }

  bool ShapeIdentical(const S2Shape* lhs, const S2Shape* rhs) {
    if (lhs->dimension() != rhs->dimension()) {
      return false;
    }

    if (lhs->num_chains() != rhs->num_chains()) {
      return false;
    }

    for (int i = 0; i < lhs->num_chains(); i++) {
      S2Shape::Chain chain_lhs = lhs->chain(i);
      S2Shape::Chain chain_rhs = rhs->chain(i);

      if (chain_lhs.length != chain_rhs.length) {
        return false;
      }

      if (chain_lhs.length == 0) {
        continue;
      }

      S2Shape::Edge lhs_e = lhs->chain_edge(i, 0);
      S2Shape::Edge rhs_e = rhs->chain_edge(i, 0);
      S2Point lhs_v0 = lhs_e.v0;
      S2Point rhs_v0 = rhs_e.v0;
      if (lhs_v0 != rhs_v0) {
        return false;
      }

      for (int j = 1; j < chain_lhs.length; j++) {
        lhs_e = lhs->chain_edge(i, j);
        rhs_e = rhs->chain_edge(i, j);
        S2Point lhs_v1 = lhs_e.v1;
        S2Point rhs_v1 = rhs_e.v1;
        if (lhs_v1 != rhs_v1) {
          return false;
        }
      }
    }

    return true;
  }

  S2BooleanOperation::Options options_;
  std::vector<S2CellId> intersection_;
};

void IntersectsKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2Intersects>(out, "st_intersects");
}

void ContainsKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2Contains>(out, "st_contains");
}

void EqualsKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2Equals>(out, "st_equals");
}

}  // namespace sedona_udf

}  // namespace s2geography
