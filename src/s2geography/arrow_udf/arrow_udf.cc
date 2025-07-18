
#include "s2geography/arrow_udf/arrow_udf.h"

#include <cerrno>
#include <unordered_map>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2/s2earth.h"
#include "s2geography.h"
#include "s2geography/arrow_udf/arrow_udf_internal.h"

namespace s2geography {

namespace arrow_udf {

struct S2InterpolateNormalizedExec {
  using arg0_t = GeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    stashed_ = s2_interpolate_normalized(value0, value1);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> InterpolateNormalized() {
  return std::make_unique<BinaryUDF<S2InterpolateNormalizedExec>>();
}

struct S2LengthExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_length(value) * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Length() {
  return std::make_unique<UnaryUDF<S2LengthExec>>();
}

struct S2AreaExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_area(value) * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Area() {
  return std::make_unique<UnaryUDF<S2AreaExec>>();
}

struct S2PerimeterExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_perimeter(value) * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Perimeter() {
  return std::make_unique<UnaryUDF<S2PerimeterExec>>();
}

struct S2CentroidExec {
  using arg0_t = GeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    S2Point out = s2_centroid(value);
    stashed_ = PointGeography(out);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> Centroid() {
  return std::make_unique<UnaryUDF<S2CentroidExec>>();
}

struct S2ClosestPointExec {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg0_t::c_type value1) {
    S2Point out = s2_closest_point(value0, value1);
    stashed_ = PointGeography(out);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> ClosestPoint() {
  return std::make_unique<BinaryUDF<S2ClosestPointExec>>();
}

struct S2ConvexHullExec {
  using arg0_t = GeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    stashed_ = s2_convex_hull(value);
    return *stashed_;
  }

  std::unique_ptr<Geography> stashed_;
};

std::unique_ptr<ArrowUDF> ConvexHull() {
  return std::make_unique<UnaryUDF<S2ConvexHullExec>>();
}

struct S2PointOnSurfaceExec {
  using arg0_t = GeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    S2Point out = s2_point_on_surface(value, coverer_);
    stashed_ = PointGeography(out);
    return stashed_;
  }

  PointGeography stashed_;
  S2RegionCoverer coverer_;
};

std::unique_ptr<ArrowUDF> PointOnSurface() {
  return std::make_unique<UnaryUDF<S2PointOnSurfaceExec>>();
}

struct S2Intersects {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

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

  void Init(const std::unordered_map<std::string, std::string> &options) {}

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

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_equals(value0, value1, options_);
  }

  S2BooleanOperation::Options options_;
};

std::unique_ptr<ArrowUDF> Equals() {
  return std::make_unique<BinaryUDF<S2Equals>>();
}

struct S2DistanceExec {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_distance(value0, value1);
  }
};

std::unique_ptr<ArrowUDF> Distance() {
  return std::make_unique<BinaryUDF<S2DistanceExec>>();
}

template <S2BooleanOperation::OpType op_type>
struct BooleanOperationExec {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg0_t::c_type value1) {
    stashed_ = s2_boolean_operation(value0, value1, op_type, options_);
    return *stashed_;
  }

  std::unique_ptr<Geography> stashed_;
  GlobalOptions options_;
};

std::unique_ptr<ArrowUDF> Difference() {
  return std::make_unique<BinaryUDF<
      BooleanOperationExec<S2BooleanOperation::OpType::DIFFERENCE>>>();
}

std::unique_ptr<ArrowUDF> SymmetricDifference() {
  return std::make_unique<BinaryUDF<BooleanOperationExec<
      S2BooleanOperation::OpType::SYMMETRIC_DIFFERENCE>>>();
}

std::unique_ptr<ArrowUDF> Intersection() {
  return std::make_unique<BinaryUDF<
      BooleanOperationExec<S2BooleanOperation::OpType::INTERSECTION>>>();
}

std::unique_ptr<ArrowUDF> Union() {
  return std::make_unique<
      BinaryUDF<BooleanOperationExec<S2BooleanOperation::OpType::UNION>>>();
}

struct S2ShortestLineExec {
  using arg0_t = GeographyIndexInputView;
  using arg1_t = GeographyIndexInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg0_t::c_type value1) {
    std::pair<S2Point, S2Point> out =
        s2_minimum_clearance_line_between(value0, value1);
    stashed_ = PolylineGeography(std::make_unique<S2Polyline>(
        std::vector<S2Point>{out.first, out.second}));
    return stashed_;
  }

  PolylineGeography stashed_;
};

std::unique_ptr<ArrowUDF> ShortestLine() {
  return std::make_unique<BinaryUDF<S2ShortestLineExec>>();
}

}  // namespace arrow_udf

}  // namespace s2geography
