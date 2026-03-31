
#include "s2geography/accessors-geog.h"

#include <s2/s2centroids.h>
#include <s2/s2edge_distances.h>

#include "s2geography/accessors.h"
#include "s2geography/build.h"
#include "s2geography/geography_interface.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

S2Point s2_centroid(const Geography& geog) {
  S2Point centroid(0, 0, 0);

  if (geog.dimension() == 0) {
    for (int i = 0; i < geog.num_shapes(); i++) {
      auto shape = geog.Shape(i);
      for (int j = 0; j < shape->num_edges(); j++) {
        centroid += shape->edge(j).v0;
      }
    }

    return centroid.Normalize();
  }

  if (geog.dimension() == 1) {
    for (int i = 0; i < geog.num_shapes(); i++) {
      auto shape = geog.Shape(i);
      for (int j = 0; j < shape->num_edges(); j++) {
        S2Shape::Edge e = shape->edge(j);
        centroid += S2::TrueCentroid(e.v0, e.v1);
      }
    }

    return centroid.Normalize();
  }

  if (geog.dimension() == 2) {
    auto polygon_ptr = dynamic_cast<const PolygonGeography*>(&geog);
    if (polygon_ptr != nullptr) {
      centroid = polygon_ptr->Polygon()->GetCentroid();
    } else {
      std::unique_ptr<PolygonGeography> built = s2_build_polygon(geog);
      centroid = built->Polygon()->GetCentroid();
    }

    return centroid.Normalize();
  }

  auto collection_ptr = dynamic_cast<const GeographyCollection*>(&geog);
  if (collection_ptr == nullptr) {
    throw Exception(
        "Can't compute s2_centroid() on custom collection geography");
  }

  for (auto& feat : collection_ptr->Features()) {
    centroid += s2_centroid(*feat);
  }

  return centroid.Normalize();
}

std::unique_ptr<Geography> s2_boundary(const Geography& geog) {
  int dimension = s2_dimension(geog);

  if (dimension == 1) {
    std::vector<S2Point> endpoints;
    for (int i = 0; i < geog.num_shapes(); i++) {
      auto shape = geog.Shape(i);
      if (shape->dimension() < 1) {
        continue;
      }

      endpoints.reserve(endpoints.size() + shape->num_chains() * 2);
      for (int j = 0; j < shape->num_chains(); j++) {
        S2Shape::Chain chain = shape->chain(j);
        if (chain.length > 0) {
          endpoints.push_back(shape->edge(chain.start).v0);
          endpoints.push_back(shape->edge(chain.start + chain.length - 1).v1);
        }
      }
    }

    return absl::make_unique<PointGeography>(std::move(endpoints));
  }

  if (dimension == 2) {
    std::vector<std::unique_ptr<S2Polyline>> polylines;
    polylines.reserve(geog.num_shapes());

    for (int i = 0; i < geog.num_shapes(); i++) {
      auto shape = geog.Shape(i);
      if (shape->dimension() != 2) {
        throw Exception("Can't extract boundary from heterogeneous collection");
      }

      for (int j = 0; j < shape->num_chains(); j++) {
        S2Shape::Chain chain = shape->chain(j);
        if (chain.length > 0) {
          std::vector<S2Point> points(chain.length + 1);

          points[0] = shape->edge(chain.start).v0;
          for (int k = 0; k < chain.length; k++) {
            points[k + 1] = shape->edge(chain.start + k).v1;
          }

          auto polyline = absl::make_unique<S2Polyline>(std::move(points));
          polylines.push_back(std::move(polyline));
        }
      }
    }

    return absl::make_unique<PolylineGeography>(std::move(polylines));
  }

  return absl::make_unique<GeographyCollection>();
}

std::unique_ptr<Geography> s2_convex_hull(const Geography& geog) {
  S2ConvexHullAggregator agg;
  agg.Add(geog);
  return agg.Finalize();
}

void CentroidAggregator::Add(const Geography& geog) {
  S2Point centroid = s2_centroid(geog);
  if (centroid.Norm2() > 0) {
    centroid_ += centroid.Normalize();
  }
}

void CentroidAggregator::Merge(const CentroidAggregator& other) {
  centroid_ += other.centroid_;
}

S2Point CentroidAggregator::Finalize() {
  if (centroid_.Norm2() > 0) {
    return centroid_.Normalize();
  } else {
    return centroid_;
  }
}

void S2ConvexHullAggregator::Add(const Geography& geog) {
  if (geog.dimension() == 0) {
    auto point_ptr = dynamic_cast<const PointGeography*>(&geog);
    if (point_ptr != nullptr) {
      for (const auto& point : point_ptr->Points()) {
        query_.AddPoint(point);
      }
    } else {
      keep_alive_.push_back(s2_rebuild(geog, GlobalOptions()));
      Add(*keep_alive_.back());
    }

    return;
  }

  if (geog.dimension() == 1) {
    auto poly_ptr = dynamic_cast<const PolylineGeography*>(&geog);
    if (poly_ptr != nullptr) {
      for (const auto& polyline : poly_ptr->Polylines()) {
        query_.AddPolyline(*polyline);
      }
    } else {
      keep_alive_.push_back(s2_rebuild(geog, GlobalOptions()));
      Add(*keep_alive_.back());
    }

    return;
  }

  if (geog.dimension() == 2) {
    auto poly_ptr = dynamic_cast<const PolygonGeography*>(&geog);
    if (poly_ptr != nullptr) {
      query_.AddPolygon(*poly_ptr->Polygon());
    } else {
      keep_alive_.push_back(s2_rebuild(geog, GlobalOptions()));
      Add(*keep_alive_.back());
    }

    return;
  }

  auto collection_ptr = dynamic_cast<const GeographyCollection*>(&geog);
  if (collection_ptr != nullptr) {
    for (const auto& feature : collection_ptr->Features()) {
      Add(*feature);
    }
  } else {
    keep_alive_.push_back(s2_rebuild(geog, GlobalOptions()));
    Add(*keep_alive_.back());
  }
}

std::unique_ptr<PolygonGeography> S2ConvexHullAggregator::Finalize() {
  auto polygon = absl::make_unique<S2Polygon>();
  polygon->Init(query_.GetConvexHull());
  return absl::make_unique<PolygonGeography>(std::move(polygon));
}

namespace sedona_udf {

struct S2CentroidExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    S2Point pt;

    // Compute in decreasing dimensionality and return early, because
    // centroids of lower dimension do not count towards the final value
    // if the geography has mixed dimension.
    if (!value.polygons()->is_empty()) {
      value.polygons()->geom().VisitLoops(&scratch_, [&](GeoArrowLoop loop) {
        pt += loop.GetCentroid();
        return true;
      });

      out->Append(PointGeography(pt.Normalize()));
      return;
    }

    if (!value.lines()->is_empty()) {
      value.lines()->geom().VisitEdges([&](const S2Shape::Edge& e) {
        pt += S2::TrueCentroid(e.v0, e.v1);
        return true;
      });

      out->Append(PointGeography(pt.Normalize()));
      return;
    }

    if (!value.points()->is_empty()) {
      value.points()->geom().VisitVertices([&](const S2Point& v) {
        pt += v;
        return true;
      });

      out->Append(PointGeography(pt.Normalize()));
      return;
    }

    out->Append(GeographyCollection());
  }

  std::vector<S2Point> scratch_;
};

struct S2ConvexHullExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    if (value.is_empty()) {
      out->Append(GeographyCollection());
      return;
    }

    auto maybe_point = value.Point();
    if (maybe_point) {
      out->Append(PointGeography(*maybe_point));
      return;
    }

    // This query could be more efficient if we vendored the S2ConvexHullQuery
    // because there are a lot of unnecessary copies involved here.
    S2ConvexHullQuery query;

    // Points and lines are added purely on the basis of their vertices
    // (in the internals of the S2ConvexHullQuery as well).
    value.points()->geom().VisitVertices([&](const S2Point& v) {
      query.AddPoint(v);
      return true;
    });

    value.lines()->geom().VisitVertices([&](const S2Point& v) {
      query.AddPoint(v);
      return true;
    });

    value.polygons()->geom().VisitLoops(&scratch_, [&](GeoArrowLoop loop) {
      // Holes don't contribute to convex hulls
      if (loop.is_hole()) {
        return true;
      }

      loop.VisitVertices([&](const S2Point& v) {
        query.AddPoint(v);
        return true;
      });

      return true;
    });

    auto hull_loop = query.GetConvexHull();

    // If we have a very skinny loop this means the output should be a line.
    // TODO: make less verbose.
    if (hull_loop->num_vertices() == 3) {
      S1ChordAngle perp_limit(S2::kProjectPerpendicularError);
      const S2Point& v0 = hull_loop->vertex(0);
      const S2Point& v1 = hull_loop->vertex(1);
      const S2Point& v2 = hull_loop->vertex(2);

      // Check each vertex against its opposite edge. If any vertex lies
      // on the opposite edge, the three points are collinear and the
      // convex hull is really a line segment (the longest edge).
      S2Point edge_vertices[3][2] = {
          {v1, v2},  // edge opposite v0
          {v0, v2},  // edge opposite v1
          {v0, v1},  // edge opposite v2
      };

      for (int i = 0; i < 3; i++) {
        const S2Point& x = hull_loop->vertex(i);
        const S2Point& a = edge_vertices[i][0];
        const S2Point& b = edge_vertices[i][1];
        if (S2::IsDistanceLess(x, a, b, perp_limit)) {
          // Find the longest edge to use as the polyline
          S1ChordAngle d01(v0, v1);
          S1ChordAngle d02(v0, v2);
          S1ChordAngle d12(v1, v2);
          S2Point pa, pb;
          if (d01 >= d02 && d01 >= d12) {
            pa = v0;
            pb = v1;
          } else if (d02 >= d01 && d02 >= d12) {
            pa = v0;
            pb = v2;
          } else {
            pa = v1;
            pb = v2;
          }
          auto polyline =
              std::make_unique<S2Polyline>(std::vector<S2Point>{pa, pb});
          out->Append(PolylineGeography(std::move(polyline)));
          return;
        }
      }
    }

    auto polygon = std::make_unique<S2Polygon>();
    polygon->Init(std::move(hull_loop));
    out->Append(PolygonGeography(std::move(polygon)));
  }

  std::vector<S2Point> scratch_;
};

struct S2PointOnSurfaceExec {
  using arg0_t = GeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Exec(arg0_t::c_type value, out_t* out) {
    S2Point pt = s2_point_on_surface(value, coverer_);
    out->Append(PointGeography(pt));
  }

  S2RegionCoverer coverer_;
};

void CentroidKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<S2CentroidExec>(out, "st_centroid");
}

void ConvexHullKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<S2ConvexHullExec>(out, "st_convexhull");
}

void PointOnSurfaceKernel(struct SedonaCScalarKernel* out) {
  InitUnaryKernel<S2PointOnSurfaceExec>(out, "st_pointonsurface");
}

}  // namespace sedona_udf

}  // namespace s2geography
