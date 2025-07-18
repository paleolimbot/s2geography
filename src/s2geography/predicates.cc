
#include "s2geography/predicates.h"

#include <s2/s2boolean_operation.h>
#include <s2/s2edge_tessellator.h>
#include <s2/s2lax_loop_shape.h>

#include "s2geography/accessors.h"
#include "s2geography/arrow_udf/arrow_udf_internal.h"

namespace s2geography {

bool s2_intersects(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2,
                   const S2BooleanOperation::Options& options) {
  return S2BooleanOperation::Intersects(geog1.ShapeIndex(), geog2.ShapeIndex(),
                                        options);
}

bool s2_equals(const ShapeIndexGeography& geog1,
               const ShapeIndexGeography& geog2,
               const S2BooleanOperation::Options& options) {
  return S2BooleanOperation::Equals(geog1.ShapeIndex(), geog2.ShapeIndex(),
                                    options);
}

bool s2_contains(const ShapeIndexGeography& geog1,
                 const ShapeIndexGeography& geog2,
                 const S2BooleanOperation::Options& options) {
  if (s2_is_empty(geog2)) {
    return false;
  } else {
    return S2BooleanOperation::Contains(geog1.ShapeIndex(), geog2.ShapeIndex(),
                                        options);
  }
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

  return S2BooleanOperation::Intersects(geog1.ShapeIndex(), index, options);
}

namespace arrow_udf {

struct S2Intersects {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_intersects(value0, value1, options_);
  }

  S2BooleanOperation::Options options_;
};

std::unique_ptr<ArrowUDF> Intersects() {
  return std::make_unique<BinaryUDF<S2Intersects>>();
}

struct S2Contains {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_contains(value0, value1, options_);
  }

  S2BooleanOperation::Options options_;
};

std::unique_ptr<ArrowUDF> Contains() {
  return std::make_unique<BinaryUDF<S2Contains>>();
}

struct S2Equals {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_equals(value0, value1, options_);
  }

  S2BooleanOperation::Options options_;
};

std::unique_ptr<ArrowUDF> Equals() {
  return std::make_unique<BinaryUDF<S2Equals>>();
}

}  // namespace arrow_udf

}  // namespace s2geography
