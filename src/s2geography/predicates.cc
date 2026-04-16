
#include "s2geography/predicates.h"

#include <s2/s2boolean_operation.h>
#include <s2/s2contains_point_query.h>
#include <s2/s2crossing_edge_query.h>
#include <s2/s2edge_crosser.h>
#include <s2/s2edge_distances.h>
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

template <typename Output>
struct S2Intersects {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = Output;

  S2Intersects() {
    options_.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
  }

  void Exec(arg0_t::c_type value0, arg1_t::c_type value1, out_t* out) {
    // If either argument is EMPTY, the result is FALSE
    if (value0.is_empty() || value1.is_empty()) {
      out->Append(false);
      return;
    }

    // For small geometries where no index has been built yet,
    // use brute force edge crossing and containment checks to avoid the
    // cost of building an index.
    if (value0.is_unindexed() && value1.is_unindexed() &&
        value0.num_edges() < kMaxBruteForceEdges &&
        value1.num_edges() < kMaxBruteForceEdges) {
      out->Append(BruteForceExec(value0, value1));
      return;
    }

    // The containment test for a S2Point is ~20x faster than building an index
    // containing exactly one point for every element so we try very hard to
    // avoid it.
    auto maybe_point0 = value0.Point();
    auto maybe_point1 = value1.Point();
    if (maybe_point0 && maybe_point1) {
      out->Append(maybe_point0->Normalize() == maybe_point1->Normalize());
      return;
    } else if (maybe_point0) {
      auto region1 = value1.Region();
      if (!region1->MayIntersect(S2Cell(*maybe_point0))) {
        out->Append(false);
        return;
      }

      out->Append(region1->Contains(*maybe_point0) ||
                  ExecUsingShapeIndex(value0, value1));
      return;
    } else if (maybe_point1) {
      auto region0 = value0.Region();
      if (!region0->MayIntersect(S2Cell(*maybe_point1))) {
        out->Append(false);
        return;
      }

      out->Append(region0->Contains(*maybe_point1) ||
                  ExecUsingShapeIndex(value0, value1));
      return;
    }

    // Next we try a covering intersection check. This is very cheap if an index
    // has already been built. In the event that an index does have to be built
    // to build the covering, it is effectively reused in the actual
    // s2_intersection() check. This is 2x faster than an intersection check for
    // selective point-in-polygon queries but may need to be reevaluated.
    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      out->Append(false);
      return;
    }

    out->Append(ExecUsingShapeIndex(value0, value1));
  }

  bool BruteForceExec(GeoArrowGeography& geog0, GeoArrowGeography& geog1) {
    // Collect non-point edges from both geometries upfront
    edges0_.clear();
    geog0.VisitNonPointEdges([&](const S2Shape::Edge& e) {
      edges0_.push_back(e);
      return true;
    });
    edges1_.clear();
    geog1.VisitNonPointEdges([&](const S2Shape::Edge& e) {
      edges1_.push_back(e);
      return true;
    });

    // Check edge-edge crossings. CrossingSign returns:
    //   +1 if edges cross at an interior point
    //    0 if edges share a vertex
    //   -1 otherwise (no crossing)
    // For the CLOSED polygon model, shared vertices count as intersection.
    if (!geog0.VisitNonPointEdges([&](const S2Shape::Edge& e0) {
          S2CopyingEdgeCrosser crosser(e0.v0, e0.v1);
          for (const auto& e1 : edges1_) {
            if (crosser.CrossingSign(e1.v0, e1.v1) >= 0) {
              return false;
            }
          }
          return true;
        })) {
      return true;
    }

    // Check if any vertex of geog1 is inside geog0's polygons
    if (geog0.polygons()->num_edges() > 0) {
      auto ref = geog0.polygons()->GetReferencePoint();
      if (!geog1.VisitVertices([&](const S2Point& pt) {
            return !geog0.polygons()->BruteForceContains(pt, ref);
          })) {
        return true;
      }
    }

    // Check if any vertex of geog0 is inside geog1's polygons
    if (geog1.polygons()->num_edges() > 0) {
      auto ref = geog1.polygons()->GetReferencePoint();
      if (!geog0.VisitVertices([&](const S2Point& pt) {
            return !geog1.polygons()->BruteForceContains(pt, ref);
          })) {
        return true;
      }
    }

    // Check point vertices against the other geometry's vertices and edges.
    // VisitEdges only visits lines and polygons, so standalone point
    // geometries are not covered by the edge-crossing check above.

    if (!geog0.points()->geom().VisitVertices([&](const S2Point& p) {
          // Check if this point matches any vertex of geog1
          if (!geog1.VisitVertices(
                  [&](const S2Point& v) { return !(p == v); })) {
            return false;
          }
          // Check if this point lies on the interior of any edge of geog1
          for (const auto& e : edges1_) {
            if (S2::IsInteriorDistanceLess(p, e.v0, e.v1,
                                           S1ChordAngle::Zero().Successor())) {
              return false;
            }
          }
          return true;
        })) {
      return true;
    }

    if (!geog1.points()->geom().VisitVertices([&](const S2Point& p) {
          // Check if this point matches any vertex of geog0
          if (!geog0.VisitVertices(
                  [&](const S2Point& v) { return !(p == v); })) {
            return false;
          }
          // Check if this point lies on the interior of any edge of geog0
          for (const auto& e : edges0_) {
            if (S2::IsInteriorDistanceLess(p, e.v0, e.v1,
                                           S1ChordAngle::Zero().Successor())) {
              return false;
            }
          }
          return true;
        })) {
      return true;
    }

    return false;
  }

  bool ExecUsingShapeIndex(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_intersects(value0.ShapeIndex(), value1.ShapeIndex(), options_);
  }

  S2BooleanOperation::Options options_;
  std::vector<S2CellId> intersection_;
  std::vector<S2Shape::Edge> edges0_;
  std::vector<S2Shape::Edge> edges1_;
};

struct S2Contains {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = BoolOutputBuilder;

  void Exec(arg0_t::c_type value0, arg1_t::c_type value1, out_t* out) {
    // If either argument is EMPTY, the result is FALSE
    if (value0.is_empty() || value1.is_empty()) {
      out->Append(false);
      return;
    }

    auto maybe_point0 = value0.Point();
    auto maybe_point1 = value1.Point();
    if (maybe_point0) {
      // A point cannot contain anything
      out->Append(false);
      return;
    } else if (maybe_point1) {
      if (value0.is_unindexed() && value0.num_edges() < kMaxBruteForceEdges &&
          value0.dimension() == 2) {
        out->Append(value0.polygons()->BruteForceContains(*maybe_point1));
        return;
      }

      auto region0 = value0.Region();
      out->Append(region0->MayIntersect(S2Cell(*maybe_point1)) &&
                  region0->Contains(*maybe_point1));
      return;
    }

    // For small non-point geometries where A has polygons and no index has
    // been built yet, use brute force containment and edge crossing checks.
    if (value0.is_unindexed() && value1.is_unindexed() &&
        value0.num_edges() < kMaxBruteForceEdges &&
        value1.num_edges() < kMaxBruteForceEdges &&
        value0.polygons()->num_edges() > 0) {
      out->Append(BruteForceExec(value0, value1));
      return;
    }

    // When the container (value0) has an index and value1 is small+fresh,
    // check containment using the index's point query and crossing query.
    if (!value0.is_unindexed() && value1.is_unindexed() &&
        value1.num_edges() < kMaxBruteForceEdges) {
      out->Append(SemiBruteForceIndexedContains(value0.ShapeIndex(), value1));
      return;
    }

    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      out->Append(false);
      return;
    }

    out->Append(ExecUsingShapeIndex(value0, value1));
  }

  bool BruteForceExec(GeoArrowGeography& geog0, GeoArrowGeography& geog1) {
    // All vertices of geog1 must be inside geog0's polygons
    auto ref = geog0.polygons()->GetReferencePoint();
    if (!geog1.VisitVertices([&](const S2Point& pt) {
          return geog0.polygons()->BruteForceContains(pt, ref);
        })) {
      return false;
    }

    // No edges of geog1 may properly cross edges of geog0
    edges_.clear();
    geog0.VisitNonPointEdges([&](const S2Shape::Edge& e) {
      edges_.push_back(e);
      return true;
    });

    if (!geog1.VisitNonPointEdges([&](const S2Shape::Edge& e1) {
          S2CopyingEdgeCrosser crosser(e1.v0, e1.v1);
          for (const auto& e0 : edges_) {
            if (crosser.CrossingSign(e0.v0, e0.v1) > 0) {
              return false;
            }
          }
          return true;
        })) {
      return false;
    }

    return true;
  }

  // Container (value0) is indexed, value1 is small and unindexed.
  // Use index queries to check all vertices/edges of value1 against value0.
  bool SemiBruteForceIndexedContains(const S2ShapeIndex& indexed,
                                     GeoArrowGeography& fresh_geog) {
    // All vertices of the fresh geometry must be contained by the index.
    auto contains_query = MakeS2ContainsPointQuery(
        &indexed, S2ContainsPointQueryOptions(S2VertexModel::SEMI_OPEN));
    if (!fresh_geog.VisitVertices(
            [&](const S2Point& pt) { return contains_query.Contains(pt); })) {
      return false;
    }

    // No edge of the fresh geometry may properly cross an edge of the index.
    S2CrossingEdgeQuery crossing_query(&indexed);
    if (!fresh_geog.VisitNonPointEdges([&](const S2Shape::Edge& e) {
          crossing_query.GetCrossingEdges(e.v0, e.v1,
                                          s2shapeutil::CrossingType::INTERIOR,
                                          &crossing_edges_);
          return crossing_edges_.empty();
        })) {
      return false;
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

  S2Equals() {
    options_.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
  }

  void Exec(arg0_t::c_type value0, arg1_t::c_type value1, out_t* out) {
    // Empties equal each other regardless of exactly how they are empty
    if (value0.is_empty() && value1.is_empty()) {
      out->Append(true);
      return;
    }

    if (GeographyIdentical(value0, value1)) {
      out->Append(true);
      return;
    }

    S2CellUnion::GetIntersection(value0.Covering(), value1.Covering(),
                                 &intersection_);
    if (intersection_.empty()) {
      out->Append(false);
      return;
    }

    out->Append(s2_equals(value0.ShapeIndex(), value1.ShapeIndex(), options_));
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

void IntersectsKernel(struct SedonaCScalarKernel* out, bool prepare_arg0_scalar,
                      bool prepare_arg1_scalar) {
  InitBinaryKernel<S2Intersects<BoolOutputBuilder>>(
      out, "st_intersects", prepare_arg0_scalar, prepare_arg1_scalar);
}

void ContainsKernel(struct SedonaCScalarKernel* out, bool prepare_arg0_scalar,
                    bool prepare_arg1_scalar) {
  InitBinaryKernel<S2Contains>(out, "st_contains", prepare_arg0_scalar,
                               prepare_arg1_scalar);
}

void EqualsKernel(struct SedonaCScalarKernel* out, bool prepare_arg0_scalar,
                  bool prepare_arg1_scalar) {
  InitBinaryKernel<S2Equals>(out, "st_equals", prepare_arg0_scalar,
                             prepare_arg1_scalar);
}

}  // namespace sedona_udf

struct StashedBoolOutput {
  void Append(bool value) { *out_ = value; }
  bool* out_;
};

class IntersectsPredicate : public BinaryPredicate {
 public:
  IntersectsPredicate() : out_(StashedBoolOutput{&result_}) {}

  bool Evaluate(const GeoArrowGeography& lhs,
                const GeoArrowGeography& rhs) override {
    S2GEOGRAPHY_UNUSED(lhs);
    S2GEOGRAPHY_UNUSED(rhs);
    S2GEOGRAPHY_UNUSED(out_);
    // exec_.Exec(lhs, rhs, &out_);
    return false;
  }

 private:
  bool result_{};
  StashedBoolOutput out_;
  sedona_udf::S2Intersects<StashedBoolOutput> exec_;
};

class ContainsPredicate : public BinaryPredicate {
 public:
  bool Evaluate(const GeoArrowGeography& lhs,
                const GeoArrowGeography& rhs) override {
    S2GEOGRAPHY_UNUSED(lhs);
    S2GEOGRAPHY_UNUSED(rhs);
    // Const-correctness
    // p_.Exec(lhs, rhs, nullptr);

    return result_;
  }

 private:
  sedona_udf::S2Contains p_;
  bool result_{};
};

class WithinPredicate : public BinaryPredicate {
 public:
  bool Evaluate(const GeoArrowGeography& lhs,
                const GeoArrowGeography& rhs) override {
    S2GEOGRAPHY_UNUSED(lhs);
    S2GEOGRAPHY_UNUSED(rhs);
    // TODO: implement
    return false;
  }
};

class EqualsPredicate : public BinaryPredicate {
 public:
  bool Evaluate(const GeoArrowGeography& lhs,
                const GeoArrowGeography& rhs) override {
    S2GEOGRAPHY_UNUSED(lhs);
    S2GEOGRAPHY_UNUSED(rhs);
    // TODO: implement
    return false;
  }
};

std::unique_ptr<BinaryPredicate> BinaryPredicate::Intersects() {
  return std::make_unique<IntersectsPredicate>();
}

std::unique_ptr<BinaryPredicate> BinaryPredicate::Contains() {
  return std::make_unique<ContainsPredicate>();
}

std::unique_ptr<BinaryPredicate> BinaryPredicate::Within() {
  return std::make_unique<WithinPredicate>();
}

std::unique_ptr<BinaryPredicate> BinaryPredicate::Equals() {
  return std::make_unique<EqualsPredicate>();
}

}  // namespace s2geography
