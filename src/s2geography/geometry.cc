#include "s2geography/geometry.h"

#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Exec implementation for st_to_geography for geography (no-op)
struct ToGeographyNoOpExec {
  using arg0_t = GeoArrowGeometryInputView;
  using out_t = GeoArrowGeographyOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) { out->AppendGeometry(value); }
};

/// \brief Exec implementation for st_to_geometry for geometry (no-op)
///
/// This version still parses and rebuilds output (could be removed by an
/// optimizer rule).
struct ToGeometryNoOpExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = GeoArrowGeometryOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    out->AppendGeometry(value.geom());
  }
};

/// \brief Exec implementation for st_to_geography for geography (no-op)
///
/// This version still parses and rebuilds output (could be removed by an
/// optimizer rule).
struct ToGeographyExec {
  using arg0_t = GeoArrowGeometryInputView;
  using out_t = GeoArrowGeographyOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) { out->AppendGeometry(value); }
};

/// \brief Exec implementation for st_to_geometry for geometry (no-op)
struct ToGeometryExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = GeoArrowGeometryOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    out->AppendGeometry(value.geom());
  }
};

void ToGeographyKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<ToGeographyNoOpExec>(out, "st_to_geography");
}

void ToGeometryKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<ToGeometryNoOpExec>(out, "st_to_geometry");
}

}  // namespace sedona_udf

}  // namespace s2geography
