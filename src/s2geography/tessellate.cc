#include "s2geography/tessellate.h"

#include <s2/s2earth.h>
#include <s2/s2edge_tessellator.h>

#include "s2geography/geoarrow-geography_util.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

namespace sedona_udf {

namespace {

/// \brief Interpolate a GeoArrowVertex along a planar edge
///
/// Like GeoArrowEdge::Interpolate() but for planar (not geodesic) edges.
/// The lng/lat values come directly from the R2Point, while Z and M values
/// are linearly interpolated based on the position along the edge.
internal::GeoArrowVertex EdgeInterpolateGeom(const internal::GeoArrowEdge& e,
                                             const R2Point& p) {
  // Calculate the edge vector in planar space
  double dx = e.v1.lng - e.v0.lng;
  double dy = e.v1.lat - e.v0.lat;
  double edge_length_sq = dx * dx + dy * dy;

  // If the edge has zero length, return the first vertex
  if (edge_length_sq == 0) {
    return e.v0;
  }

  // Calculate the fraction along the edge using dot product projection
  double dpx = p.x() - e.v0.lng;
  double dpy = p.y() - e.v0.lat;
  double fraction = (dpx * dx + dpy * dy) / edge_length_sq;

  // Clamp fraction to [0, 1] and return endpoint if at boundary
  if (fraction <= 0) {
    return e.v0;
  } else if (fraction >= 1) {
    return e.v1;
  }

  // Interpolate Z and M values linearly
  double dzm0 = (e.v1.zm[0] - e.v0.zm[0]) * fraction;
  double dzm1 = (e.v1.zm[1] - e.v0.zm[1]) * fraction;

  return {p.x(), p.y(), {e.v0.zm[0] + dzm0, e.v0.zm[1] + dzm1}};
}

}  // namespace

/// \brief Exec implementation for st_tessellategeog for geography
struct TessellateGeogExec {
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

/// \brief Exec implementation for st_tessellategeom for geography
struct TessellateGeomExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowGeometryOutputBuilder;

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
        geom.geom(), [&](const struct GeoArrowGeometryNode* node) {
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
                      GeoArrowGeometryOutputBuilder* out) {
    internal::VisitNativeVertices(
        node, 0, node->size, [&](internal::GeoArrowVertex v) {
          v.SetPoint(projection_.Unproject(R2Point(v.lng, v.lat)));
          out->AppendPoint(v);
          return true;
        });
  }

  void TessellateLinestring(const struct GeoArrowGeometryNode* node,
                            GeoArrowGeometryOutputBuilder* out) {
    if (node->size == 0) {
      return;
    }

    // Add the first point
    internal::VisitNativeVertices(node, 0, 1, [&](internal::GeoArrowVertex v) {
      R2Point projected = projection_.Project(v.ToPoint());
      v.lng = projected.x();
      v.lat = projected.y();
      out->AppendPoint(v);
      return true;
    });

    // Add subsequent points resulting from the edge tessellation
    internal::VisitNativeEdges(
        node, 0, node->size, [&](const internal::GeoArrowEdge& e) {
          points_.clear();
          tessellator_.AppendProjected(e.v0.ToPoint(), e.v1.ToPoint(),
                                       &points_);
          S2GEOGRAPHY_DCHECK(points_.size() >= 2);
          for (size_t i = 1; i < points_.size(); ++i) {
            out->AppendPoint(EdgeInterpolateGeom(e, points_[i]));
          }
          return true;
        });
  }

  double last_distance_{-1};
  S2EdgeTessellator tessellator_;
  S2::PlateCarreeProjection projection_{S2::PlateCarreeProjection(180.0)};
  std::vector<R2Point> points_;
};

void TessellateToGeog(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<TessellateGeogExec>(out, "st_tessellategeog");
}

void TessellateToGeom(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<TessellateGeomExec>(out, "st_tessellategeom");
}

}  // namespace sedona_udf

}  // namespace s2geography
