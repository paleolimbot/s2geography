#include "s2geography/geometry.h"

#include <s2/s2earth.h>
#include <s2/s2edge_tessellator.h>

#include "s2geography/geoarrow-geography_util.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Exec implementation for st_to_geography for geography (no-op)
///
/// This version still parses and rebuilds output (could be removed by an
/// optimizer rule).
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

/// \brief Exec implementation for st_to_geography for geography
struct ToGeographyExec {
  using arg0_t = GeoArrowGeometryInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowGeographyOutputBuilder;

  void Exec(arg0_t::c_type geom, arg1_t::c_type distance, out_t* out) {
    if (distance != last_distance_) {
      S1Angle tolerance = S1Angle::Radians(distance / S2Earth::RadiusMeters());
      if (tolerance < S2EdgeTessellator::kMinTolerance()) {
        tolerance = S2EdgeTessellator::kMinTolerance();
      }

      tessellator_ = S2EdgeTessellator(&projection_, tolerance);
    }

    // TODO: we need to figure out how to call GeomEnd() at the right times
    int64_t remaining_rings = 0;
    internal::VisitGeoArrowNodes(
        geom, [&](const struct GeoArrowGeometryNode* node) {
          out->SetDimensions(node->dimensions);
          switch (node->geometry_type) {
            case GEOARROW_GEOMETRY_TYPE_POINT:
              out->GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
              UnprojectPoint(node, out);
              out->GeomEnd();
              break;

            case GEOARROW_GEOMETRY_TYPE_LINESTRING:
              if (remaining_rings > 0) {
                out->GeomStart(GEOARROW_GEOMETRY_TYPE_LINESTRING);
                TessellateLinestring(node, out);
                out->GeomEnd();
              } else {
                out->RingStart();
                TessellateLinestring(node, out);
                out->RingEnd();
              }

              --remaining_rings;
              break;
            case GEOARROW_GEOMETRY_TYPE_POLYGON:
              remaining_rings = node->size;
              break;
          }
          return true;
        });
  }

  void UnprojectPoint(const struct GeoArrowGeometryNode* node,
                      GeoArrowGeographyOutputBuilder* out) {
    internal::VisitNativeVertices(
        node, 0, node->size, [&](internal::GeoArrowVertex v) {
          v.SetPoint(projection_.Unproject(R2Point(v.lng, v.lat)));
          out->AppendPoint(v);
          return true;
        });
  }

  void TessellateLinestring(const struct GeoArrowGeometryNode* node,
                            GeoArrowGeographyOutputBuilder* out) {
    if (node->size == 0) {
      return;
    }

    // Add the first point
    internal::VisitNativeVertices(node, 0, 1, [&](internal::GeoArrowVertex v) {
      v.SetPoint(projection_.Unproject(R2Point(v.lng, v.lat)));
      out->AppendPoint(v);
      return true;
    });

    // Add subsequent points resulting from the edge tessellation
    internal::VisitNativeEdges(
        node, 0, node->size, [&](const internal::GeoArrowEdge& e) {
          points_.clear();
          tessellator_.AppendUnprojected(R2Point(e.v0.lng, e.v0.lat),
                                         R2Point(e.v1.lng, e.v1.lat), &points_);
          S2GEOGRAPHY_DCHECK(points_.size() >= 2);
          for (size_t i = 1; i < points_.size(); ++i) {
            out->AppendPoint(e.Interpolate(points_[i]));
          }
          return true;
        });
  }

  double last_distance_{-1};
  S2EdgeTessellator tessellator_;
  S2::PlateCarreeProjection projection_{S2::PlateCarreeProjection(180.0)};
  std::vector<S2Point> points_;
};

/// \brief Exec implementation for st_to_geometry for geography
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
