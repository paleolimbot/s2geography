
#include "s2geography/coverings.h"

#include <s2/s2edge_crosser.h>
#include <s2/s2latlng_rect_bounder.h>
#include <s2/s2region_coverer.h>
#include <s2/s2shape_index_buffered_region.h>

#include "s2geography/accessors-geog.h"
#include "s2geography/accessors.h"
#include "s2geography/geoarrow-geography.h"
#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

S2Point s2_point_on_surface(const Geography& geog, S2RegionCoverer& coverer) {
  if (s2_is_empty(geog)) {
    return S2Point();
  }

  int dimension = s2_dimension(geog);
  if (dimension == 2) {
    std::unique_ptr<S2Region> region = geog.Region();
    S2CellUnion covering = coverer.GetInteriorCovering(*region);

    // Take center of cell with smallest level (biggest)
    int min_level = 31;
    S2Point pt;
    for (const S2CellId& id : covering) {
      if (id.level() < min_level) {
        // Already normalized
        pt = id.ToPoint();
        min_level = id.level();
      }
    }

    return pt;
  }

  if (dimension == 0) {
    // For point, return point closest to centroid
    S2Point centroid = s2_centroid(geog);

    S1Angle nearest_dist = S1Angle::Infinity();
    S1Angle dist;
    S2Point closest_pt;
    for (int i = 0; i < geog.num_shapes(); i++) {
      auto shape = geog.Shape(i);
      for (int j = 0; j < shape->num_edges(); j++) {
        S2Shape::Edge e = shape->edge(j);
        dist = S1Angle(e.v0, centroid);
        if (dist < nearest_dist) {
          nearest_dist = dist;
          closest_pt = e.v0;
        }
      }
    }

    return closest_pt;
  }

  throw Exception("s2_point_on_surface() not implemented for polyline");
}

void s2_covering(const Geography& geog, std::vector<S2CellId>* covering,
                 S2RegionCoverer& coverer) {
  coverer.GetCovering(*geog.Region(), covering);
}

void s2_interior_covering(const Geography& geog,
                          std::vector<S2CellId>* covering,
                          S2RegionCoverer& coverer) {
  coverer.GetInteriorCovering(*geog.Region(), covering);
}

void s2_covering_buffered(const ShapeIndexGeography& geog,
                          double distance_radians,
                          std::vector<S2CellId>* covering,
                          S2RegionCoverer& coverer) {
  S2ShapeIndexBufferedRegion region(&geog.ShapeIndex(),
                                    S1ChordAngle::Radians(distance_radians));
  coverer.GetCovering(region, covering);
}

void LatLngRectBounder::Clear() { bounds_ = S2LatLngRect::Empty(); }

S2LatLngRect LatLngRectBounder::Finish() const { return bounds_; }

void LatLngRectBounder::Update(const GeoArrowGeography& value) {
  if (value.is_empty()) {
    return;
  }

  bounds_ = bounds_.Union(
      BoundPoints(value).Union(BoundLines(value)).Union(BoundLoops(value)));
}

S2LatLngRect LatLngRectBounder::BoundPoints(const GeoArrowGeography& value) {
  if (value.points()->is_empty()) {
    return S2LatLngRect::Empty();
  }

  S1Interval xs;
  R1Interval ys;
  value.points()->geom().VisitNativeVertices(
      [&](const internal::GeoArrowVertex& v) {
        S2LatLng ll = S2LatLng::FromDegrees(v.lat, v.lng).Normalized();
        xs.AddPoint(ll.lng().radians());
        ys.AddPoint(ll.lat().radians());
        return true;
      });

  // Expand for error in rounding for xs. This is the expansion that takes
  // place during the S2LatLngRectBounder's GetBound().
  xs = xs.Expanded(2 * DBL_EPSILON);

  S2LatLng lo(S1Angle::Radians(ys.lo()), S1Angle::Radians(xs.lo()));
  S2LatLng hi(S1Angle::Radians(ys.hi()), S1Angle::Radians(xs.hi()));
  return S2LatLngRect(lo, hi);
}

S2LatLngRect LatLngRectBounder::BoundLines(const GeoArrowGeography& value) {
  S2LatLngRect bounds = S2LatLngRect::Empty();

  value.lines()->geom().VisitChains([&](const GeoArrowChain& c) {
    S2LatLngRectBounder bounder;
    c.VisitNativeVertices([&](const internal::GeoArrowVertex& v) {
      S2LatLng ll = S2LatLng::FromDegrees(v.lat, v.lng).Normalized();
      bounder.AddLatLng(ll);
      return true;
    });
    bounds = bounds.Union(bounder.GetBound());
    return true;
  });

  return bounds;
}

S2LatLngRect LatLngRectBounder::BoundLoops(const GeoArrowGeography& value) {
  S2LatLngRect bounds = S2LatLngRect::Empty();
  auto reference = value.polygons()->GetReferencePoint();

  // Adapted from the s2loop.cc implementation of bounding
  value.polygons()->geom().VisitLoops(&scratch_, [&](const GeoArrowLoop& loop) {
    // Only shells contribute to the bounds of valid polygons
    if (loop.is_hole()) {
      return true;
    }

    // Loop through all the vertices, bounding the edges and checking
    // for edge crossings between the reference point and the north pole.
    // We brute force both containment checks because (1) we have to loop
    // through all the vertices anyway and (2) in most cases we can avoid
    // the check for south pole containment.
    S2LatLngRectBounder bounder;

    S2Point north_pole = S2Point(0, 0, 1);
    internal::GeoArrowVertex nv0 = loop.native_vertex(0);
    S2CopyingEdgeCrosser crosser(reference.point, north_pole, nv0.ToPoint());
    bool contains_north_pole = reference.contained;

    bounder.AddLatLng(S2LatLng::FromDegrees(nv0.lat, nv0.lng).Normalized());
    loop.VisitNativeVertices(
        1, loop.size() - 1, [&](const internal::GeoArrowVertex& v) {
          S2LatLng ll = S2LatLng::FromDegrees(v.lat, v.lng).Normalized();
          bounder.AddLatLng(ll);
          contains_north_pole ^= crosser.EdgeOrVertexCrossing(ll.ToPoint());
          return true;
        });
    S2LatLngRect b = bounder.GetBound();

    // Check north pole containment
    if (contains_north_pole) {
      b = S2LatLngRect(R1Interval(b.lat().lo(), M_PI_2), S1Interval::Full());
    }

    // If a loop contains the south pole, then either it wraps entirely
    // around the sphere (full longitude range), or it also contains the
    // north pole in which case b.lng().is_full() due to the test above.
    // Either way, we only need to do the south pole containment test if
    // b.lng().is_full().
    if (b.lng().is_full()) {
      bool contains_south_pole = reference.contained;
      S2Point south_pole(0, 0, -1);
      S2CopyingEdgeCrosser crosser(reference.point, south_pole, nv0.ToPoint());
      loop.VisitVertices(1, loop.size() - 1, [&](const S2Point& vertex) {
        contains_south_pole ^= crosser.EdgeOrVertexCrossing(vertex);
        return true;
      });

      if (contains_south_pole) {
        b.mutable_lat()->set_lo(-M_PI_2);
      }
    }

    bounds = bounds.Union(b);
    return true;
  });

  return bounds;
}

namespace sedona_udf {

struct CellIdFromPointExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = IntOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    if (value.is_empty()) {
      out->AppendNull();
      return;
    }

    auto pt = value.Point();
    if (!pt) {
      throw Exception("Can't compute point cell ID from a non-point geography");
    }

    S2CellId id(*pt);
    out->Append(static_cast<int64_t>(id.id()));
  }
};

struct CoveringCellIdsExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = ListOutputBuilder<IntOutputBuilder>;

  void Exec(arg0_t::c_type value, out_t* out) {
    if (value.is_empty()) {
      out->Append();
      return;
    }

    // Canonically consider the S2CellId of a Point to be
    // its covering. Otherwise we get funny coverings for points
    // (no need to have four cells for a single point).
    auto pt = value.Point();
    if (pt) {
      S2CellId id(*pt);
      out->items().Append(static_cast<int64_t>(id.id()));
      out->Append();
      return;
    }

    // For now, don't make any attempt to optimize calculating this.
    // This will build a shape index for each item and may be slow.
    // We may want to consider just implementing S2Region for the
    // GeoArrowGeography.
    coverer_.mutable_options()->set_max_cells(8);
    covering_.clear();
    coverer_.GetCovering(*value.Region(), &covering_);
    for (const S2CellId id : covering_) {
      out->items().Append(static_cast<int64_t>(id.id()));
    }

    out->Append();
  }

  std::vector<S2CellId> covering_;
  S2RegionCoverer coverer_;
};

struct BoundingBoxExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = StructOutputBuilder<DoubleOutputBuilder, DoubleOutputBuilder,
                                    DoubleOutputBuilder, DoubleOutputBuilder>;

  void Init(arg0_t* input, out_t* out) {
    S2GEOGRAPHY_UNUSED(input);
    out->SetNames({"xmin", "ymin", "xmax", "ymax"});
  }

  void Exec(arg0_t::c_type value, out_t* out) {
    if (value.is_empty()) {
      out->AppendNull();
      return;
    }

    bounder_.Clear();
    bounder_.Update(value);
    S2LatLngRect bounds = bounder_.Finish();
    out->field<0>().Append(bounds.lng_lo().degrees());
    out->field<1>().Append(bounds.lat_lo().degrees());
    out->field<2>().Append(bounds.lng_hi().degrees());
    out->field<3>().Append(bounds.lat_hi().degrees());
    out->Append();
  }

  LatLngRectBounder bounder_;
};

void CellIdFromPointKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<CellIdFromPointExec>(out, "s2_cellidfrompoint");
}

void CoveringCellIdsKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<CoveringCellIdsExec>(out, "s2_coveringcellids");
}

void BoundingBoxKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<BoundingBoxExec>(out, "st_boundingbox");
}

}  // namespace sedona_udf

}  // namespace s2geography
