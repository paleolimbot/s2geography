
#include "s2geography/predicates.h"

#include <s2/s2boolean_operation.h>
#include <s2/s2contains_point_query.h>
#include <s2/s2crossing_edge_query.h>
#include <s2/s2edge_crosser.h>
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

static const int kMaxBruteForceEdges = 32;



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

    // For small non-point geometries where no index has been built yet,
    // use brute force edge crossing and containment checks to avoid the
    // cost of building an index.
    if (value0.is_fresh() && value1.is_fresh() &&
        value0.num_edges() < kMaxBruteForceEdges &&
        value1.num_edges() < kMaxBruteForceEdges) {
      return BruteForceExec(value0, value1);
    }

    // When one side already has an index and the other is small+fresh, use
    // the indexed side's S2CrossingEdgeQuery and S2ContainsPointQuery to
    // iterate over the fresh side's edges/vertices without building a
    // second index.
    if (!value0.is_fresh() && value1.is_fresh() &&
        value1.num_edges() < kMaxBruteForceEdges) {
      return SemiBruteForceExec(value0.ShapeIndex(), value1);
    }
    if (value0.is_fresh() && !value1.is_fresh() &&
        value0.num_edges() < kMaxBruteForceEdges) {
      return SemiBruteForceExec(value1.ShapeIndex(), value0);
    }

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

  bool BruteForceExec(GeoArrowGeography& geog0, GeoArrowGeography& geog1) {
    // Collect edges from geog1 for repeated iteration
    edges_.clear();
    geog1.VisitEdges(
        [this](const S2Shape::Edge& e) { edges_.push_back(e); });

    // Check edge-edge crossings. CrossingSign returns:
    //   +1 if edges cross at an interior point
    //    0 if edges share a vertex
    //   -1 otherwise (no crossing)
    // For the CLOSED polygon model, shared vertices count as intersection.
    bool found = false;
    geog0.VisitEdges([&](const S2Shape::Edge& e0) {
      if (found) return;
      S2CopyingEdgeCrosser crosser(e0.v0, e0.v1);
      for (const auto& e1 : edges_) {
        if (crosser.CrossingSign(e1.v0, e1.v1) >= 0) {
          found = true;
          return;
        }
      }
    });
    if (found) return true;

    // Check if any vertex of geog1 is inside geog0's polygons
    if (geog0.polygons()->num_edges() > 0) {
      auto ref = geog0.polygons()->GetReferencePoint();
      geog1.VisitVertices([&](const S2Point& pt) {
        if (!found && geog0.polygons()->BruteForceContains(pt, ref)) {
          found = true;
        }
      });
      if (found) return true;
    }

    // Check if any vertex of geog0 is inside geog1's polygons
    if (geog1.polygons()->num_edges() > 0) {
      auto ref = geog1.polygons()->GetReferencePoint();
      geog0.VisitVertices([&](const S2Point& pt) {
        if (!found && geog1.polygons()->BruteForceContains(pt, ref)) {
          found = true;
        }
      });
      if (found) return true;
    }

    return false;
  }

  // One side is indexed, the other (fresh_geog) is small and unindexed.
  // Intersects is symmetric so we can always query the indexed side.
  bool SemiBruteForceExec(const S2ShapeIndex& indexed,
                          GeoArrowGeography& fresh_geog) {
    // Check if any edge of the fresh geometry crosses an edge in the index.
    // CrossingType::ALL includes shared-vertex intersections (CLOSED model).
    S2CrossingEdgeQuery crossing_query(&indexed);
    bool found = false;
    fresh_geog.VisitEdges([&](const S2Shape::Edge& e) {
      if (found) return;
      crossing_query.GetCrossingEdges(e.v0, e.v1,
                                      s2shapeutil::CrossingType::ALL,
                                      &crossing_edges_);
      if (!crossing_edges_.empty()) {
        found = true;
      }
    });
    if (found) return true;

    // Check if any vertex of the fresh geometry is contained by the index.
    auto contains_query = MakeS2ContainsPointQuery(
        &indexed,
        S2ContainsPointQueryOptions(S2VertexModel::CLOSED));
    fresh_geog.VisitVertices([&](const S2Point& pt) {
      if (!found && contains_query.Contains(pt)) {
        found = true;
      }
    });
    if (found) return true;

    // Check if any vertex of the indexed geometry is inside the fresh
    // geometry's polygons.
    if (fresh_geog.polygons()->num_edges() > 0) {
      auto ref = fresh_geog.polygons()->GetReferencePoint();
      for (int i = 0; i < indexed.num_shape_ids(); i++) {
        const S2Shape* shape = indexed.shape(i);
        if (shape == nullptr) continue;
        for (int j = 0; j < shape->num_edges(); j++) {
          S2Shape::Edge e = shape->edge(j);
          if (fresh_geog.polygons()->BruteForceContains(e.v0, ref)) {
            return true;
          }
        }
      }
    }

    return false;
  }

  bool ExecUsingShapeIndex(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_intersects(value0.ShapeIndex(), value1.ShapeIndex(), options_);
  }

  S2BooleanOperation::Options options_;
  std::vector<S2CellId> intersection_;
  std::vector<S2Shape::Edge> edges_;
  std::vector<s2shapeutil::ShapeEdge> crossing_edges_;
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
      if (value0.is_fresh() && value0.num_edges() < kMaxBruteForceEdges &&
          value0.dimension() == 2) {
        return value0.polygons()->BruteForceContains(*maybe_point1);
      }

      auto region0 = value0.Region();
      return region0->MayIntersect(S2Cell(*maybe_point1)) &&
             region0->Contains(*maybe_point1);
    }

    // For small non-point geometries where A has polygons and no index has
    // been built yet, use brute force containment and edge crossing checks.
    if (value0.is_fresh() && value1.is_fresh() &&
        value0.num_edges() < kMaxBruteForceEdges &&
        value1.num_edges() < kMaxBruteForceEdges &&
        value0.polygons()->num_edges() > 0) {
      return BruteForceExec(value0, value1);
    }

    // When the container (value0) has an index and value1 is small+fresh,
    // check containment using the index's point query and crossing query.
    if (!value0.is_fresh() && value1.is_fresh() &&
        value1.num_edges() < kMaxBruteForceEdges) {
      return SemiBruteForceIndexedContains(value0.ShapeIndex(), value1);
    }

    // When value1 has an index and value0 (container) is small+fresh with
    // polygons, use brute force containment on value0's polygons.
    if (value0.is_fresh() && !value1.is_fresh() &&
        value0.num_edges() < kMaxBruteForceEdges &&
        value0.polygons()->num_edges() > 0) {
      return SemiBruteForceContainsFresh(value0, value1.ShapeIndex());
    }

    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      return false;
    }

    return ExecUsingShapeIndex(value0, value1);
  }

  bool BruteForceExec(GeoArrowGeography& geog0, GeoArrowGeography& geog1) {
    // All vertices of geog1 must be inside geog0's polygons
    auto ref = geog0.polygons()->GetReferencePoint();
    bool all_inside = true;
    geog1.VisitVertices([&](const S2Point& pt) {
      if (all_inside && !geog0.polygons()->BruteForceContains(pt, ref)) {
        all_inside = false;
      }
    });
    if (!all_inside) return false;

    // No edges of geog1 may properly cross edges of geog0
    edges_.clear();
    geog0.VisitEdges(
        [this](const S2Shape::Edge& e) { edges_.push_back(e); });

    bool crossing_found = false;
    geog1.VisitEdges([&](const S2Shape::Edge& e1) {
      if (crossing_found) return;
      S2CopyingEdgeCrosser crosser(e1.v0, e1.v1);
      for (const auto& e0 : edges_) {
        if (crosser.CrossingSign(e0.v0, e0.v1) > 0) {
          crossing_found = true;
          return;
        }
      }
    });
    if (crossing_found) return false;

    return true;
  }

  // Container (value0) is indexed, value1 is small and unindexed.
  // Use index queries to check all vertices/edges of value1 against value0.
  bool SemiBruteForceIndexedContains(const S2ShapeIndex& indexed,
                                     GeoArrowGeography& fresh_geog) {
    // All vertices of the fresh geometry must be contained by the index.
    auto contains_query = MakeS2ContainsPointQuery(
        &indexed,
        S2ContainsPointQueryOptions(S2VertexModel::SEMI_OPEN));
    bool all_inside = true;
    fresh_geog.VisitVertices([&](const S2Point& pt) {
      if (all_inside && !contains_query.Contains(pt)) {
        all_inside = false;
      }
    });
    if (!all_inside) return false;

    // No edge of the fresh geometry may properly cross an edge of the index.
    S2CrossingEdgeQuery crossing_query(&indexed);
    bool crossing_found = false;
    fresh_geog.VisitEdges([&](const S2Shape::Edge& e) {
      if (crossing_found) return;
      crossing_query.GetCrossingEdges(e.v0, e.v1,
                                      s2shapeutil::CrossingType::INTERIOR,
                                      &crossing_edges_);
      if (!crossing_edges_.empty()) {
        crossing_found = true;
      }
    });
    if (crossing_found) return false;

    return true;
  }

  // Container (value0) is small+fresh with polygons, value1 is indexed.
  // Use brute force containment on value0's polygons for each vertex of
  // value1, and brute force edge crossing checks.
  bool SemiBruteForceContainsFresh(GeoArrowGeography& container,
                                   const S2ShapeIndex& contained) {
    // All vertices of the contained index must be inside the container's
    // polygons.
    auto ref = container.polygons()->GetReferencePoint();
    for (int i = 0; i < contained.num_shape_ids(); i++) {
      const S2Shape* shape = contained.shape(i);
      if (shape == nullptr) continue;
      for (int j = 0; j < shape->num_edges(); j++) {
        S2Shape::Edge e = shape->edge(j);
        if (!container.polygons()->BruteForceContains(e.v0, ref)) {
          return false;
        }
      }
      // Check the last vertex of each chain too (edge() gives (v0, v1) pairs
      // but the last vertex of a chain is only in the v1 of the last edge).
      for (int c = 0; c < shape->num_chains(); c++) {
        S2Shape::Chain chain = shape->chain(c);
        if (chain.length > 0) {
          S2Shape::Edge last = shape->chain_edge(c, chain.length - 1);
          if (!container.polygons()->BruteForceContains(last.v1, ref)) {
            return false;
          }
        }
      }
    }

    // No edges of the contained geometry may properly cross edges of the
    // container.
    edges_.clear();
    container.VisitEdges(
        [this](const S2Shape::Edge& e) { edges_.push_back(e); });

    for (int i = 0; i < contained.num_shape_ids(); i++) {
      const S2Shape* shape = contained.shape(i);
      if (shape == nullptr) continue;
      for (int j = 0; j < shape->num_edges(); j++) {
        S2Shape::Edge e1 = shape->edge(j);
        S2CopyingEdgeCrosser crosser(e1.v0, e1.v1);
        for (const auto& e0 : edges_) {
          if (crosser.CrossingSign(e0.v0, e0.v1) > 0) {
            return false;
          }
        }
      }
    }

    return true;
  }

  bool ExecUsingShapeIndex(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_contains(value0.ShapeIndex(), value1.ShapeIndex(), options_);
  }

  S2BooleanOperation::Options options_;
  std::vector<S2CellId> intersection_;
  std::vector<S2Shape::Edge> edges_;
  std::vector<s2shapeutil::ShapeEdge> crossing_edges_;
};

struct S2Equals {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = BoolOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {
    options_.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
  }

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    // Empties equal each other regardless of exactly how they are empty
    if (value0.is_empty() && value1.is_empty()) {
      return true;
    }

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
    if (value0.num_shapes() != value1.num_shapes()) {
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

    if (lhs->num_edges() != rhs->num_edges()) {
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

      S2Shape::Edge lhs_e, rhs_e;
      S2Point lhs_v, rhs_v;
      for (int j = 0; j < chain_lhs.length; j++) {
        lhs_e = lhs->chain_edge(i, j);
        rhs_e = rhs->chain_edge(i, j);

        // Only for the first edge: check the start point
        if (j == 0) {
          lhs_v = lhs_e.v0;
          rhs_v = rhs_e.v0;
          if (lhs_v != rhs_v) {
            return false;
          }
        }

        lhs_v = lhs_e.v1;
        rhs_v = rhs_e.v1;
        if (lhs_v != rhs_v) {
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
