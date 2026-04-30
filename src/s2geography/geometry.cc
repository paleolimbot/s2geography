#include "s2geography/geometry.h"

#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Exec implementation for st_to_geography
///
/// Converts planar geometry (WKB) to geography with spherical edges.
/// This is a stub implementation that passes through the input.
struct ToGeographyExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = GeoArrowOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    // Stub: pass through the geometry
    // TODO: Implement actual planar to spherical conversion
    out->AppendGeometry(value.geom());
  }
};

/// \brief Exec implementation for st_to_geometry
///
/// Converts geography (spherical edges) to planar geometry (WKB).
/// This is a stub implementation that passes through the input.
struct ToGeometryExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = GeoArrowOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    // Stub: pass through the geography
    // TODO: Implement actual spherical to planar conversion
    out->AppendGeometry(value.geom());
  }
};

void ToGeographyKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<ToGeographyExec>(out, "st_to_geography");
}

void ToGeometryKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<ToGeometryExec>(out, "st_to_geometry");
}

}  // namespace sedona_udf

}  // namespace s2geography
