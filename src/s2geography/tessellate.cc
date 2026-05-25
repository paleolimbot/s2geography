#include "s2geography/tessellate.h"

#include <s2/s2earth.h>
#include <s2/s2edge_tessellator.h>

#include <cmath>
#include <optional>

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

/// \brief Check if a point node represents POINT EMPTY
///
/// geoarrow-c currently reads POINT EMPTY as POINT (nan nan) with size 1,
/// so we need to check for NaN coordinates to identify empty points.
bool IsEmptyPoint(const struct GeoArrowGeometryNode* node) {
  if (node->size == 0) {
    return true;
  }
  // Check if all coordinates are NaN (geoarrow-c representation of POINT EMPTY)
  struct GeoArrowGeometryView view = {node, 1};
  return internal::AllLngLatNaN(view);
}

template <typename VisitPoint, typename VisitLinestring, typename Out>
void TransformSegments(struct GeoArrowGeometryView geom, Out* out,
                       VisitPoint&& visit_point,
                       VisitLinestring&& visit_linestring) {
  static constexpr int kMaxRecursion = 32;
  int64_t remaining[kMaxRecursion] = {0};
  uint8_t parent_type[kMaxRecursion] = {0};
  int depth = 0;

  internal::VisitGeoArrowNodes(geom, [&](const struct GeoArrowGeometryNode*
                                             node) {
    out->SetDimensions(node->dimensions);

    // If parent expects children, decrement the remaining count
    if (depth > 0 && remaining[depth] > 0) {
      --remaining[depth];
    }

    switch (node->geometry_type) {
      case GEOARROW_GEOMETRY_TYPE_POINT:
        out->GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
        // Check for POINT EMPTY (geoarrow-c represents as POINT (nan nan))
        if (!IsEmptyPoint(node)) {
          visit_point(node, out);
        }
        out->GeomEnd();
        break;

      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
        // Check if we're inside a polygon (linestring is a ring)
        if (depth > 0 && parent_type[depth] == GEOARROW_GEOMETRY_TYPE_POLYGON) {
          out->RingStart();
          visit_linestring(node, out);
          out->RingEnd();
        } else {
          out->GeomStart(GEOARROW_GEOMETRY_TYPE_LINESTRING);
          visit_linestring(node, out);
          out->GeomEnd();
        }
        break;

      case GEOARROW_GEOMETRY_TYPE_POLYGON:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
        out->GeomStart(
            static_cast<enum GeoArrowGeometryType>(node->geometry_type));
        if ((depth + 1) >= kMaxRecursion) {
          throw Exception(
              "Can't transform edges of geometry with >32 levels of nesting");
        }

        ++depth;
        remaining[depth] = node->size;
        parent_type[depth] = node->geometry_type;
        break;
      default:
        throw Exception("Unsupported geometry type constant");
    }

    // Close any geometries that have no remaining children
    while (depth > 0 && remaining[depth] == 0) {
      out->GeomEnd();
      --depth;
    }

    return true;
  });
}

}  // namespace

/// \brief Exec implementation for st_tessellategeog for geography
struct TessellateGeogExec {
  using arg0_t = GeoArrowGeometryInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowGeographyOutputBuilder;

  void Exec(arg0_t::c_type geom, arg1_t::c_type distance, out_t* out) {
    if (!std::isfinite(distance) || distance <= 0) {
      throw Exception("tolerance must be finite and greater than 0");
    }

    if (distance != last_distance_) {
      S1Angle tolerance = S1Angle::Radians(distance / S2Earth::RadiusMeters());
      if (tolerance < S2EdgeTessellator::kMinTolerance()) {
        tolerance = S2EdgeTessellator::kMinTolerance();
      }

      tessellator_.emplace(&projection_, tolerance);
      last_distance_ = distance;
    }

    out->FeatureStart();
    TransformSegments(
        geom, out,
        [&](const struct GeoArrowGeometryNode* node, out_t* out) {
          UnprojectPoint(node, out);
        },
        [&](const struct GeoArrowGeometryNode* node, out_t* out) {
          TessellateLinestring(node, out);
        });
    out->FeatureEnd();
  }

  void UnprojectPoint(const struct GeoArrowGeometryNode* node,
                      GeoArrowGeographyOutputBuilder* out) {
    internal::VisitNativeVertices(
        node, 0, node->size, [&](internal::GeoArrowVertex v) {
          v.SetPoint(projection_.Unproject(R2Point(v.lng, v.lat)));
          out->WriteCoord(v, node->dimensions);
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
      out->WriteCoord(v, node->dimensions);
      return true;
    });

    // Add subsequent points resulting from the edge tessellation
    internal::VisitNativeEdges(
        node, 0, node->size - 1, [&](const internal::GeoArrowEdge& e) {
          points_.clear();
          tessellator_->AppendUnprojected(R2Point(e.v0.lng, e.v0.lat),
                                          R2Point(e.v1.lng, e.v1.lat),
                                          &points_);
          S2GEOGRAPHY_DCHECK(points_.size() >= 2);
          for (size_t i = 1; i < points_.size(); ++i) {
            out->WriteCoord(e.Interpolate(points_[i]), node->dimensions);
          }
          return true;
        });
  }

  double last_distance_{-1};
  std::optional<S2EdgeTessellator> tessellator_;
  S2::PlateCarreeProjection projection_{S2::PlateCarreeProjection(180.0)};
  std::vector<S2Point> points_;
};

/// \brief Exec implementation for st_tessellategeom for geography
struct TessellateGeomExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowGeometryOutputBuilder;

  void Exec(arg0_t::c_type geom, arg1_t::c_type distance, out_t* out) {
    if (!std::isfinite(distance) || distance <= 0) {
      throw Exception("tolerance must be finite and greater than 0");
    }

    if (distance != last_distance_) {
      S1Angle tolerance = S1Angle::Radians(distance / S2Earth::RadiusMeters());
      if (tolerance < S2EdgeTessellator::kMinTolerance()) {
        tolerance = S2EdgeTessellator::kMinTolerance();
      }

      tessellator_.emplace(&projection_, tolerance);
      last_distance_ = distance;
    }

    out->FeatureStart();
    TransformSegments(
        geom.geom(), out,
        [&](const struct GeoArrowGeometryNode* node, out_t* out) {
          UnprojectPoint(node, out);
        },
        [&](const struct GeoArrowGeometryNode* node, out_t* out) {
          TessellateLinestring(node, out);
        });
    out->FeatureEnd();
  }

  void UnprojectPoint(const struct GeoArrowGeometryNode* node,
                      GeoArrowGeometryOutputBuilder* out) {
    internal::VisitNativeVertices(
        node, 0, node->size, [&](internal::GeoArrowVertex v) {
          R2Point projected = projection_.Project(v.ToPoint());
          v.lng = projected.x();
          v.lat = projected.y();
          out->WriteCoord(v, node->dimensions);
          return true;
        });
  }

  void TessellateLinestring(const struct GeoArrowGeometryNode* node,
                            GeoArrowGeometryOutputBuilder* out) {
    if (node->size == 0) {
      return;
    }

    // Track the wrapped version of the last output vertex for continuity
    // across antimeridian crossings
    internal::GeoArrowVertex last_wrapped_v;

    // Add the first point
    internal::VisitNativeVertices(node, 0, 1, [&](internal::GeoArrowVertex v) {
      R2Point projected = projection_.Project(v.ToPoint());
      v.lng = projected.x();
      v.lat = projected.y();
      last_wrapped_v = v;
      out->WriteCoord(v, node->dimensions);
      return true;
    });

    // Add subsequent points resulting from the edge tessellation
    internal::VisitNativeEdges(
        node, 0, node->size - 1, [&](const internal::GeoArrowEdge& e) {
          points_.clear();
          tessellator_->AppendProjected(e.v0.ToPoint(), e.v1.ToPoint(),
                                        &points_);
          S2GEOGRAPHY_DCHECK(points_.size() >= 2);

          // Create edge copy using wrapped v0 for correct interpolation
          internal::GeoArrowEdge wrapped_e = e;
          wrapped_e.v0 = last_wrapped_v;

          // Project and wrap v1 relative to wrapped v0
          R2Point projected_v1 = projection_.Project(e.v1.ToPoint());
          R2Point wrapped_v1 = projection_.WrapDestination(
              R2Point(wrapped_e.v0.lng, wrapped_e.v0.lat), projected_v1);
          wrapped_e.v1.lng = wrapped_v1.x();
          wrapped_e.v1.lat = wrapped_v1.y();

          // Wrap and output intermediate points
          for (size_t i = 1; i < points_.size(); ++i) {
            R2Point wrapped_pt = projection_.WrapDestination(
                R2Point(last_wrapped_v.lng, last_wrapped_v.lat), points_[i]);
            internal::GeoArrowVertex out_v =
                EdgeInterpolateGeom(wrapped_e, wrapped_pt);
            out->WriteCoord(out_v, node->dimensions);
            last_wrapped_v = out_v;
          }

          return true;
        });
  }

  double last_distance_{-1};
  std::optional<S2EdgeTessellator> tessellator_;
  S2::PlateCarreeProjection projection_{S2::PlateCarreeProjection(180.0)};
  std::vector<R2Point> points_;
};

/// \brief Exec implementation for st_segmentize for geography
struct SegmentizeExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowGeographyOutputBuilder;

  void Exec(arg0_t::c_type geom, arg1_t::c_type distance, out_t* out) {
    if (!std::isfinite(distance) || distance <= 0) {
      throw Exception("ST_Segmentize distance must be finite and >0");
    }

    S1Angle max_segment_length =
        S1Angle::Radians(distance / S2Earth::RadiusMeters());

    out->FeatureStart();
    TransformSegments(
        geom.geom(), out,
        [&](const struct GeoArrowGeometryNode* node, out_t* out) {
          SegmentizePoint(node, out);
        },
        [&](const struct GeoArrowGeometryNode* node, out_t* out) {
          SegmentizeLinestring(node, out, max_segment_length);
        });
    out->FeatureEnd();
  }

  void SegmentizePoint(const struct GeoArrowGeometryNode* node,
                       GeoArrowGeographyOutputBuilder* out) {
    internal::VisitNativeVertices(node, 0, node->size,
                                  [&](const internal::GeoArrowVertex& v) {
                                    out->WriteCoord(v, node->dimensions);
                                    return true;
                                  });
  }

  void SegmentizeLinestring(const struct GeoArrowGeometryNode* node,
                            GeoArrowGeographyOutputBuilder* out,
                            S1Angle max_segment_length) {
    if (node->size == 0) {
      return;
    }

    // Add the first point
    internal::VisitNativeVertices(node, 0, 1,
                                  [&](const internal::GeoArrowVertex& v) {
                                    out->WriteCoord(v, node->dimensions);
                                    return true;
                                  });

    // Add subsequent points resulting from the segmentize
    internal::VisitNativeEdges(
        node, 0, node->size - 1, [&](const internal::GeoArrowEdge& e) {
          S2Point p0 = e.v0.ToPoint();
          S2Point p1 = e.v1.ToPoint();
          S1Angle edge_length(p0, p1);

          // Calculate the number of segments needed
          int64_t num_segments =
              static_cast<int64_t>(std::ceil(edge_length / max_segment_length));

          // Sanity check the number of segments to avoid mayhem
          if (num_segments > 65536) {
            throw Exception(
                "Can't add more than 65536 segments to a single edge in "
                "ST_Segmentize(). Use a larger max_segment_length or nested "
                "calls to ST_Segmentize().");
          }

          if (num_segments <= 1) {
            // No subdivision needed, just add endpoint
            out->WriteCoord(e.v1, node->dimensions);
          } else {
            // Add intermediate points at equal fractions
            for (int64_t i = 1; i < num_segments; ++i) {
              double fraction =
                  static_cast<double>(i) / static_cast<double>(num_segments);
              out->WriteCoord(e.Interpolate(fraction), node->dimensions);
            }
            // Add the final endpoint
            out->WriteCoord(e.v1, node->dimensions);
          }
          return true;
        });
  }
};

void TessellateToGeog(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<TessellateGeogExec>(out, "st_tessellategeog", false, false);
}

void TessellateToGeom(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<TessellateGeomExec>(out, "st_tessellategeom", false, false);
}

void Segmentize(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<SegmentizeExec>(out, "st_segmentize", false, false);
}

}  // namespace sedona_udf

}  // namespace s2geography
