
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

struct S2CentroidExec {
  using arg0_t = GeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  const Geography &Exec(const Geography &value) {
    S2Point out = s2_centroid(value);
    stashed_ = PointGeography(out);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> Centroid() {
  return std::make_unique<UnaryUDF<S2CentroidExec>>();
}

}  // namespace arrow_udf

}  // namespace s2geography
