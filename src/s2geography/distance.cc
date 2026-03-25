
#include "s2geography/distance.h"

#include <s2/s2closest_edge_query.h>
#include <s2/s2crossing_edge_query.h>
#include <s2/s2debug.h>
#include <s2/s2earth.h>
#include <s2/s2edge_crossings.h>
#include <s2/s2furthest_edge_query.h>

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

double s2_distance(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2) {
  return s2_distance(geog1.ShapeIndex(), geog2.ShapeIndex());
}

double s2_distance(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2) {
  S2ClosestEdgeQuery query(&geog1);
  S2ClosestEdgeQuery::ShapeIndexTarget target(&geog2);

  const auto& result = query.FindClosestEdge(&target);

  S1ChordAngle angle = result.distance();
  return angle.ToAngle().radians();
}

double s2_max_distance(const ShapeIndexGeography& geog1,
                       const ShapeIndexGeography& geog2) {
  return s2_max_distance(geog1.ShapeIndex(), geog2.ShapeIndex());
}

double s2_max_distance(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2) {
  S2FurthestEdgeQuery query(&geog1);
  S2FurthestEdgeQuery::ShapeIndexTarget target(&geog2);

  const auto& result = query.FindFurthestEdge(&target);

  S1ChordAngle angle = result.distance();
  return angle.ToAngle().radians();
}

S2Point s2_closest_point(const ShapeIndexGeography& geog1,
                         const ShapeIndexGeography& geog2) {
  return s2_minimum_clearance_line_between(geog1, geog2).first;
}

S2Point s2_closest_point(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2) {
  return s2_minimum_clearance_line_between(geog1, geog2).first;
}

std::pair<S2Point, S2Point> s2_minimum_clearance_line_between(
    const ShapeIndexGeography& geog1, const ShapeIndexGeography& geog2) {
  return s2_minimum_clearance_line_between(geog1.ShapeIndex(),
                                           geog2.ShapeIndex());
}

std::pair<S2Point, S2Point> s2_minimum_clearance_line_between(
    const S2ShapeIndex& geog1, const S2ShapeIndex& geog2) {
  S2ClosestEdgeQuery query1(&geog1);
  query1.mutable_options()->set_include_interiors(false);
  S2ClosestEdgeQuery::ShapeIndexTarget target(&geog2);

  const auto& result1 = query1.FindClosestEdge(&target);

  if (result1.edge_id() == -1) {
    return std::pair<S2Point, S2Point>(S2Point(0, 0, 0), S2Point(0, 0, 0));
  }

  // Get the edge from index1 (edge1) that is closest to index2.
  S2Shape::Edge edge1 = query1.GetEdge(result1);

  // Now find the edge from index2 (edge2) that is closest to edge1.
  S2ClosestEdgeQuery query2(&geog2);
  query2.mutable_options()->set_include_interiors(false);
  S2ClosestEdgeQuery::EdgeTarget target2(edge1.v0, edge1.v1);
  auto result2 = query2.FindClosestEdge(&target2);

  // what if result2 has no edges?
  if (result2.is_interior()) {
    throw Exception("S2ClosestEdgeQuery result is interior!");
  }

  S2Shape::Edge edge2 = query2.GetEdge(result2);

  // Find the closest point pair on edge1 and edge2.
  return S2::GetEdgePairClosestPoints(edge1.v0, edge1.v1, edge2.v0, edge2.v1);
}

namespace sedona_udf {

static const int kMaxBruteForceEdgeComparisons = 64;
static const int kFlagComputeDistance = 1;
static const int kFlagComputePoints = 2;

struct EdgePair {
  int shape_id0{-1};
  int shape_id1{-1};
  int edge_id0{-1};
  int edge_id1{-1};
  std::pair<S2Point, S2Point> closest_points{};
  S1ChordAngle distance{S1ChordAngle::Infinity()};
};

void ClearanceLineOnlyEdgesBruteForce(const GeoArrowGeography& value0,
                                      const GeoArrowGeography& value1,
                                      EdgePair* out) {
  *out = {};
  S2GEOGRAPHY_DCHECK(value0.polygons()->is_empty());
  S2GEOGRAPHY_DCHECK(value1.polygons()->is_empty());

  int edge_id0 = -1;
  value0.VisitEdges([&](S2Shape::Edge& e0) {
    ++edge_id0;
    int edge_id1 = -1;
    value1.VisitEdges([&](S2Shape::Edge& e1) {
      ++edge_id1;

      auto closest_points_candidate =
          S2::GetEdgePairClosestPoints(e0.v0, e0.v1, e1.v0, e1.v1);
      S1ChordAngle distance_candidate = S1ChordAngle(
          closest_points_candidate.first, closest_points_candidate.second);
      if (distance_candidate < out->distance) {
        auto resolved0 = value0.ResolveGlobalEdgeId(edge_id0);
        out->shape_id0 = resolved0.first;
        out->edge_id0 = resolved0.second;

        auto resolved1 = value0.ResolveGlobalEdgeId(edge_id1);
        out->shape_id1 = resolved1.first;
        out->edge_id1 = resolved1.second;

        out->distance = distance_candidate;
        out->closest_points = closest_points_candidate;
      }

      return true;
    });
    return true;
  });

  if (out->edge_id0 != -1) {
    auto resolved0 = value0.ResolveGlobalEdgeId(out->edge_id0);
    auto resolved1 = value0.ResolveGlobalEdgeId(out->edge_id0);
    out->shape_id0 = resolved0.first;
    out->shape_id1 = resolved1.first;
    out->edge_id0 = resolved0.second;
    out->edge_id1 = resolved1.second;
  }
}

void ClearanceLineOnlyEdgesSemiBruteForce(const S2ShapeIndex& value0,
                                          const GeoArrowGeography& value1,
                                          EdgePair* out, int flags) {
  *out = {};
  S2GEOGRAPHY_DCHECK(value1.polygons()->is_empty());

  S2ClosestEdgeQuery query0(&value0);
  query0.mutable_options()->set_include_interiors(true);

  int edge_id1 = -1;
  value1.VisitEdges([&](S2Shape::Edge& e1) {
    ++edge_id1;

    S2ClosestEdgeQuery::EdgeTarget target(e1.v0, e1.v1);
    const auto& result0 = query0.FindClosestEdge(&target);

    if (result0.is_empty()) {
      // No matching edges
      return true;
    } else if (result0.is_interior()) {
      // Edge interior intersects a polygon in the index.
      out->shape_id0 = result0.shape_id();
      out->edge_id0 = -1;

      auto resolved_id1 = value1.ResolveGlobalEdgeId(edge_id1);
      out->shape_id1 = resolved_id1.first;
      out->edge_id1 = resolved_id1.second;

      out->distance = S1ChordAngle::Zero();

      // Find the actual intersection point using a crossing edge query. Only do
      // this if requested.
      if (flags & kFlagComputePoints) {
        S2CrossingEdgeQuery crossing_query(&value0);
        std::vector<s2shapeutil::ShapeEdge> crossing_edges;
        crossing_query.GetCrossingEdges(
            e1.v0, e1.v1, s2shapeutil::CrossingType::ALL, &crossing_edges);

        S2Point intersection_point = e1.v0;  // fallback
        if (!crossing_edges.empty()) {
          const auto& ce = crossing_edges[0];
          intersection_point =
              S2::GetIntersection(e1.v0, e1.v1, ce.v0(), ce.v1());
        }

        out->closest_points =
            std::make_pair(intersection_point, intersection_point);
      }

      return true;
    }

    auto distance_candidate = result0.distance();
    if (distance_candidate < out->distance) {
      out->shape_id0 = result0.shape_id();
      out->edge_id0 = result0.edge_id();

      auto resolved_id1 = value1.ResolveGlobalEdgeId(edge_id1);
      out->shape_id1 = resolved_id1.first;
      out->edge_id1 = resolved_id1.second;

      out->distance = distance_candidate;

      if (flags & kFlagComputePoints) {
        S2Shape::Edge e0 = query0.GetEdge(result0);
        out->closest_points =
            S2::GetEdgePairClosestPoints(e0.v0, e0.v1, e1.v0, e1.v1);
      }
    }

    return true;
  });
}

void ClearanceLineFromPoints(const S2Point& value0, const S2Point& value1,
                             EdgePair* out) {
  out->closest_points = std::make_pair(value0, value1);
  out->edge_id0 = 0;
  out->edge_id1 = 0;
  out->shape_id0 = 0;
  out->shape_id1 = 0;
  out->distance = S1ChordAngle(value0, value1);
}

void ClearanceLineUsingShapeIndex(const S2ShapeIndex& value0,
                                  const S2ShapeIndex& value1, EdgePair* out,
                                  int flags) {
  *out = {};

  S2ClosestEdgeQuery query0(&value0);
  query0.mutable_options()->set_include_interiors(true);
  query0.mutable_options()->set_max_results(1);
  S2ClosestEdgeQuery::ShapeIndexTarget target(&value1);

  // Get the edge from value0 that is closest to value1.
  const auto& result0 = query0.FindClosestEdge(&target);
  if (result0.is_empty()) {
    return;
  }

  // If distance is zero, the geometries touch or overlap.
  if (result0.distance().is_zero()) {
    out->distance = S1ChordAngle::Zero();
    out->shape_id0 = result0.shape_id();
    out->edge_id0 = result0.edge_id();

    if (flags & kFlagComputePoints) {
      if (!result0.is_interior()) {
        // result0 gives us the edge from value0 that touches value1.
        // Use value1's index to find the crossing edge.
        S2Shape::Edge e0 = query0.GetEdge(result0);
        S2CrossingEdgeQuery crossing_query(&value1);
        std::vector<s2shapeutil::ShapeEdge> crossing_edges;
        crossing_query.GetCrossingEdges(
            e0.v0, e0.v1, s2shapeutil::CrossingType::ALL, &crossing_edges);
        if (!crossing_edges.empty()) {
          auto pt = S2::GetIntersection(e0.v0, e0.v1, crossing_edges[0].v0(),
                                        crossing_edges[0].v1());
          out->closest_points = std::make_pair(pt, pt);
          out->shape_id1 = crossing_edges[0].id().shape_id;
          out->edge_id1 = crossing_edges[0].id().edge_id;
        } else {
          // Shared vertex, no proper crossing.
          out->closest_points = std::make_pair(e0.v0, e0.v0);
        }
      } else {
        // Interior match: one fully contains the other. Use a vertex from
        // value1.
        for (int s = 0; s < value1.num_shape_ids(); ++s) {
          const S2Shape* shape = value1.shape(s);
          if (shape != nullptr && shape->num_edges() > 0) {
            S2Point pt = shape->edge(0).v0;
            out->closest_points = std::make_pair(pt, pt);
            out->shape_id1 = s;
            out->edge_id1 = 0;
            break;
          }
        }
      }
    }

    return;
  }

  // Distance > 0: find the actual closest edge pair. Interior matches are
  // impossible at this point.
  out->edge_id0 = result0.edge_id();
  out->shape_id0 = result0.shape_id();
  out->distance = result0.distance();

  // If we need the other point, we need another edge crossing query
  if (flags & kFlagComputePoints) {
    S2ClosestEdgeQuery query1(&value1);
    query1.mutable_options()->set_include_interiors(false);
    query1.mutable_options()->set_max_results(1);
    S2Shape::Edge e0 = query0.GetEdge(result0);
    S2ClosestEdgeQuery::EdgeTarget target2(e0.v0, e0.v1);
    auto result1 = query1.FindClosestEdge(&target2);
    if (result1.is_empty()) {
      return;
    }

    out->edge_id1 = result1.edge_id();
    out->shape_id1 = result1.shape_id();
    S2Shape::Edge e1 = query1.GetEdge(result1);

    // Find the closest point pair on edge1 and edge2.
    out->closest_points =
        S2::GetEdgePairClosestPoints(e0.v0, e0.v1, e1.v0, e1.v1);
  }
}

void ClearanceLineUsingShapeIndexAndPoint(const S2ShapeIndex& value0,
                                          const S2Point& value1, EdgePair* out,
                                          int flags) {
  *out = {};

  S2ClosestEdgeQuery query0(&value0);
  query0.mutable_options()->set_include_interiors(true);
  query0.mutable_options()->set_max_results(1);
  S2ClosestEdgeQuery::PointTarget target(value1);

  const auto& result0 = query0.FindClosestEdge(&target);

  if (result0.is_empty()) {
    // No matching edges
    return;
  } else if (result0.is_interior()) {
    // Interior point match
    out->shape_id0 = result0.shape_id();
    out->edge_id0 = -1;

    out->shape_id1 = 0;
    out->edge_id1 = 0;

    out->distance = S1ChordAngle::Zero();
    out->closest_points = std::make_pair(value1, value1);
    return;
  }

  out->edge_id0 = result0.edge_id();
  out->shape_id0 = result0.shape_id();
  S2Shape::Edge e0 = query0.GetEdge(result0);

  out->edge_id1 = 0;
  out->shape_id1 = 0;

  // Find the closest point pair on edge1 and edge2.
  S2Point p0 = S2::Project(value1, e0.v0, e0.v1);
  out->closest_points = std::make_pair(p0, value1);

  if (flags & kFlagComputeDistance) {
    out->distance = S1ChordAngle(p0, value1);
  }
}

bool IsAlreadyIndexedOrLargeOrHasPolygons(const GeoArrowGeography& value) {
  return !value.polygons()->is_empty() || !value.is_unindexed() ||
         value.num_edges() > kMaxBruteForceEdgeComparisons;
}

bool HasNoPolygons(const GeoArrowGeography& value) {
  return value.polygons()->is_empty();
}

bool BothSmallWithoutPolygons(const GeoArrowGeography& value0,
                              const GeoArrowGeography& value1) {
  return HasNoPolygons(value0) && HasNoPolygons(value1) &&
         // Also check each side to ensure the product will not overflow
         value0.num_edges() < kMaxBruteForceEdgeComparisons &&
         value1.num_edges() < kMaxBruteForceEdgeComparisons &&
         (value0.num_edges() * value1.num_edges()) <
             kMaxBruteForceEdgeComparisons;
}

void ClearanceLine(GeoArrowGeography& value0, GeoArrowGeography& value1,
                   EdgePair* out, int flags) {
  // If either argument is EMPTY, the default constructor of EdgePair() is the
  // correct output.
  if (value0.is_empty() || value1.is_empty()) {
    *out = {};
    return;
  }

  auto maybe_point0 = value0.Point();
  auto maybe_point1 = value1.Point();
  if (maybe_point0 && maybe_point1) {
    ClearanceLineFromPoints(*maybe_point0, *maybe_point1, out);
    return;
  } else if (maybe_point0 && IsAlreadyIndexedOrLargeOrHasPolygons(value1)) {
    ClearanceLineUsingShapeIndexAndPoint(value1.ShapeIndex(), *maybe_point0,
                                         out, flags);
    std::swap(out->shape_id0, out->shape_id1);
    std::swap(out->edge_id0, out->edge_id1);
    std::swap(out->closest_points.first, out->closest_points.second);
  } else if (maybe_point1 && IsAlreadyIndexedOrLargeOrHasPolygons(value0)) {
    ClearanceLineUsingShapeIndexAndPoint(value0.ShapeIndex(), *maybe_point1,
                                         out, flags);
  } else if (BothSmallWithoutPolygons(value0, value1)) {
    ClearanceLineOnlyEdgesBruteForce(value0, value1, out);
  } else if (IsAlreadyIndexedOrLargeOrHasPolygons(value0) &&
             HasNoPolygons(value1)) {
    ClearanceLineOnlyEdgesSemiBruteForce(value0.ShapeIndex(), value1, out,
                                         flags);
  } else if (IsAlreadyIndexedOrLargeOrHasPolygons(value1) &&
             HasNoPolygons(value0)) {
    ClearanceLineOnlyEdgesSemiBruteForce(value1.ShapeIndex(), value0, out,
                                         flags);
    std::swap(out->shape_id0, out->shape_id1);
    std::swap(out->edge_id0, out->edge_id1);
    std::swap(out->closest_points.first, out->closest_points.second);
  } else {
    ClearanceLineUsingShapeIndex(value0.ShapeIndex(), value1.ShapeIndex(), out,
                                 flags);
  }
}

struct S2ClosestPointExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    ClearanceLine(value0, value1, &edge_pair_, kFlagComputePoints);
    if (edge_pair_.shape_id0 == -1) {
      stashed_ = PointGeography();
    } else {
      stashed_ = PointGeography(edge_pair_.closest_points.first);
    }

    return stashed_;
  }

  PointGeography stashed_;
  EdgePair edge_pair_;
};

struct S2DistanceExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    ClearanceLine(value0, value1, &edge_pair_, kFlagComputeDistance);
    if (edge_pair_.shape_id0 == -1) {
      // TODO: return NULL
    }

    return edge_pair_.distance.radians() * S2Earth::RadiusMeters();
  }

  EdgePair edge_pair_;
};

struct S2MaxDistanceExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    return s2_max_distance(value0.ShapeIndex(), value1.ShapeIndex()) *
           S2Earth::RadiusMeters();
  }
};

struct S2ShortestLineExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    ClearanceLine(value0, value1, &edge_pair_, kFlagComputePoints);
    if (edge_pair_.shape_id0 == -1) {
      stashed_ = PolylineGeography();
    } else {
      stashed_ = PolylineGeography(std::make_unique<S2Polyline>(
          std::vector<S2Point>{edge_pair_.closest_points.first,
                               edge_pair_.closest_points.second},
          S2Debug::DISABLE));
    }

    return stashed_;
  }

  PolylineGeography stashed_;
  EdgePair edge_pair_;
};

void ClosestPointKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2ClosestPointExec>(out, "st_closestpoint");
}

void DistanceKernel(struct SedonaCScalarKernel* out, bool prepare_arg0_scalar,
                    bool prepare_arg1_scalar) {
  InitBinaryKernel<S2DistanceExec>(out, "st_distance", prepare_arg0_scalar,
                                   prepare_arg1_scalar);
}

void MaxDistanceKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<S2MaxDistanceExec>(out, "st_maxdistance");
}

void ShortestLineKernel(struct SedonaCScalarKernel* out,
                        bool prepare_arg0_scalar, bool prepare_arg1_scalar) {
  InitBinaryKernel<S2ShortestLineExec>(
      out, "st_shortestline", prepare_arg0_scalar, prepare_arg1_scalar);
}

}  // namespace sedona_udf

}  // namespace s2geography
