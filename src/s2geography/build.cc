
#include "s2geography/build.h"

#include <s2/s2boolean_operation.h>
#include <s2/s2builder.h>
#include <s2/s2builderutil_closed_set_normalizer.h>
#include <s2/s2builderutil_s2point_vector_layer.h>
#include <s2/s2builderutil_s2polygon_layer.h>
#include <s2/s2builderutil_s2polyline_vector_layer.h>
#include <s2/s2builderutil_snap_functions.h>
#include <s2/s2loop.h>

#include <sstream>

#include "s2geography/accessors.h"
#include "s2geography/geography_interface.h"
#include "s2geography/macros.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

std::unique_ptr<Geography> s2_geography_from_layers(
    std::vector<S2Point> points,
    std::vector<std::unique_ptr<S2Polyline>> polylines,
    std::unique_ptr<S2Polygon> polygon,
    GlobalOptions::OutputAction point_layer_action,
    GlobalOptions::OutputAction polyline_layer_action,
    GlobalOptions::OutputAction polygon_layer_action) {
  // count non-empty dimensions
  bool has_polygon = !polygon->is_empty();
  bool has_polylines = polylines.size() > 0;
  bool has_points = points.size() > 0;

  // use the requstested dimensions to produce the right kind of EMPTY
  bool include_polygon =
      polygon_layer_action == GlobalOptions::OUTPUT_ACTION_INCLUDE;
  bool include_polylines =
      polyline_layer_action == GlobalOptions::OUTPUT_ACTION_INCLUDE;
  bool include_points =
      point_layer_action == GlobalOptions::OUTPUT_ACTION_INCLUDE;

  if (has_polygon &&
      polygon_layer_action == GlobalOptions::OUTPUT_ACTION_ERROR) {
    throw Exception("Output contained unexpected polygon");
  } else if (has_polygon &&
             polygon_layer_action == GlobalOptions::OUTPUT_ACTION_IGNORE) {
    has_polygon = false;
  }

  if (has_polylines &&
      polyline_layer_action == GlobalOptions::OUTPUT_ACTION_ERROR) {
    throw Exception("Output contained unexpected polylines");
  } else if (has_polylines &&
             polyline_layer_action == GlobalOptions::OUTPUT_ACTION_IGNORE) {
    has_polylines = false;
  }

  if (has_points && point_layer_action == GlobalOptions::OUTPUT_ACTION_ERROR) {
    throw Exception("Output contained unexpected points");
  } else if (has_points &&
             point_layer_action == GlobalOptions::OUTPUT_ACTION_IGNORE) {
    has_points = false;
  }

  int non_empty_dimensions = has_polygon + has_polylines + has_points;
  int included_dimensions =
      include_polygon + include_polylines + include_points;

  // return mixed dimension output
  if (non_empty_dimensions > 1) {
    std::vector<std::unique_ptr<Geography>> features;

    if (has_points) {
      features.push_back(absl::make_unique<PointGeography>(std::move(points)));
    }

    if (has_polylines) {
      features.push_back(
          absl::make_unique<PolylineGeography>(std::move(polylines)));
    }

    if (has_polygon) {
      features.push_back(
          absl::make_unique<PolygonGeography>(std::move(polygon)));
    }

    return absl::make_unique<GeographyCollection>(std::move(features));
  }

  // return single dimension output
  if (has_polygon || (included_dimensions == 1 && include_polygon)) {
    return absl::make_unique<PolygonGeography>(std::move(polygon));
  } else if (has_polylines || (included_dimensions == 1 && include_polylines)) {
    return absl::make_unique<PolylineGeography>(std::move(polylines));
  } else if (has_points || (included_dimensions == 1 && include_points)) {
    return absl::make_unique<PointGeography>(std::move(points));
  } else {
    return absl::make_unique<GeographyCollection>();
  }
}

static std::unique_ptr<Geography> s2_boolean_operation(
    const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
    S2BooleanOperation::OpType op_type, const GlobalOptions& options,
    GlobalOptions::OutputAction point_layer_action,
    GlobalOptions::OutputAction polyline_layer_action,
    GlobalOptions::OutputAction polygon_layer_action) {
  // Create the data structures that will contain the output.
  std::vector<S2Point> points;
  std::vector<std::unique_ptr<S2Polyline>> polylines;
  std::unique_ptr<S2Polygon> polygon = absl::make_unique<S2Polygon>();

  s2builderutil::LayerVector layers(3);
  layers[0] = absl::make_unique<s2builderutil::S2PointVectorLayer>(
      &points, options.point_layer);
  layers[1] = absl::make_unique<s2builderutil::S2PolylineVectorLayer>(
      &polylines, options.polyline_layer);
  layers[2] = absl::make_unique<s2builderutil::S2PolygonLayer>(
      polygon.get(), options.polygon_layer);

  // specify the boolean operation
  S2BooleanOperation op(op_type,
                        // Normalizing the closed set here is required for line
                        // intersections to work in the same way as GEOS
                        s2builderutil::NormalizeClosedSet(std::move(layers)),
                        options.boolean_operation);

  // do the boolean operation, build layers, and check for errors
  S2Error error;
  if (!op.Build(geog1, geog2, &error)) {
    std::stringstream ss;
    ss << error;
    throw Exception(ss.str());
  }

  // construct output
  return s2_geography_from_layers(std::move(points), std::move(polylines),
                                  std::move(polygon), point_layer_action,
                                  polyline_layer_action, polygon_layer_action);
}

std::unique_ptr<Geography> s2_boolean_operation(
    const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
    S2BooleanOperation::OpType op_type, const GlobalOptions& options) {
  return s2_boolean_operation(
      geog1, geog2, op_type, options, options.point_layer_action,
      options.polygon_layer_action, options.polygon_layer_action);
}

std::unique_ptr<PolygonGeography> s2_unary_union(const PolygonGeography& geog,
                                                 const GlobalOptions& options) {
  // A geography with invalid loops won't work with the S2BooleanOperation
  // we will use to accumulate (i.e., union) valid polygons,
  // so we need to rebuild each loop as its own polygon,
  // splitting crossed edges along the way.

  // Not exposing these options as an argument (except snap function)
  // because a particular combination of them is required for this to work
  S2Builder::Options builder_options;
  builder_options.set_split_crossing_edges(true);
  builder_options.set_snap_function(options.boolean_operation.snap_function());
  s2builderutil::S2PolygonLayer::Options layer_options;
  layer_options.set_edge_type(S2Builder::EdgeType::UNDIRECTED);
  layer_options.set_validate(false);

  // Rebuild all loops as polygons using the S2Builder()
  std::vector<std::unique_ptr<S2Polygon>> loops;
  for (int i = 0; i < geog.Polygon()->num_loops(); i++) {
    std::unique_ptr<S2Polygon> loop = absl::make_unique<S2Polygon>();
    S2Builder builder(builder_options);
    builder.StartLayer(absl::make_unique<s2builderutil::S2PolygonLayer>(
        loop.get(), layer_options));
    builder.AddShape(S2Loop::Shape(geog.Polygon()->loop(i)));
    S2Error error;
    if (!builder.Build(&error)) {
      std::stringstream ss;
      ss << error;
      throw Exception(ss.str());
    }

    // Check if the builder created a polygon whose boundary contained more than
    // half the earth (and invert it if so)
    if (loop->GetArea() > (2 * M_PI)) {
      loop->Invert();
    }

    loops.push_back(std::move(loop));
  }

  // Accumulate the union of outer loops (but difference of inner loops)
  std::unique_ptr<S2Polygon> accumulated_polygon =
      absl::make_unique<S2Polygon>();
  for (int i = 0; i < geog.Polygon()->num_loops(); i++) {
    std::unique_ptr<S2Polygon> polygon_result = absl::make_unique<S2Polygon>();

    // Use original nesting to suggest if this loop should be unioned or diffed.
    // For valid polygons loops are arranged such that the biggest loop is on
    // the outside followed by holes such that the below strategy should work
    // (since we are just iterating along the original loop structure)
    if ((geog.Polygon()->loop(i)->depth() % 2) == 0) {
      polygon_result->InitToUnion(*accumulated_polygon, *loops[i]);
    } else {
      polygon_result->InitToDifference(*accumulated_polygon, *loops[i]);
    }

    accumulated_polygon.swap(polygon_result);
  }

  return absl::make_unique<PolygonGeography>(std::move(accumulated_polygon));
}

std::unique_ptr<Geography> s2_unary_union(const ShapeIndexGeography& geog,
                                          const GlobalOptions& options) {
  // Empty input -> empty output
  // The best we can do since we don't know the dimension of the input
  if (s2_is_empty(geog)) {
    return std::make_unique<GeographyCollection>();
  }

  // Complex union only needed when a polygon is involved
  int highest_input_dimension = s2_dimension(geog);
  bool simple_union_ok = highest_input_dimension < 2;

  // Valid polygons that are not part of a collection can also use a
  // simple(r) union (common)
  if (geog.dimension() == 2) {
    S2Error validation_error;
    if (!s2_find_validation_error(geog, &validation_error)) {
      simple_union_ok = true;
    }
  }

  if (simple_union_ok) {
    ShapeIndexGeography empty;
    return s2_boolean_operation(geog, empty, S2BooleanOperation::OpType::UNION,
                                options);
  }

  // If we've made it here we have an invalid polygon on our hands.
  if (geog.kind() == GeographyKind::POLYGON) {
    auto poly = reinterpret_cast<const PolygonGeography*>(&geog);
    return s2_unary_union(*poly, options);
  } else if (geog.dimension() == 2) {
    auto poly = s2_build_polygon(geog);
    return s2_unary_union(*poly, options);
  }

  throw Exception(
      "s2_unary_union() for multidimensional collections not implemented");
}

std::unique_ptr<Geography> s2_rebuild(
    const Geography& geog, const GlobalOptions& options,
    GlobalOptions::OutputAction point_layer_action,
    GlobalOptions::OutputAction polyline_layer_action,
    GlobalOptions::OutputAction polygon_layer_action) {
  // create the builder
  S2Builder builder(options.builder);

  // create the data structures that will contain the output
  std::vector<S2Point> points;
  std::vector<std::unique_ptr<S2Polyline>> polylines;
  std::unique_ptr<S2Polygon> polygon = absl::make_unique<S2Polygon>();

  // add shapes to the layer with the appropriate dimension
  builder.StartLayer(absl::make_unique<s2builderutil::S2PointVectorLayer>(
      &points, options.point_layer));
  for (int i = 0; i < geog.num_shapes(); i++) {
    auto shape = geog.Shape(i);
    if (shape->dimension() == 0) {
      builder.AddShape(*shape);
    }
  }

  builder.StartLayer(absl::make_unique<s2builderutil::S2PolylineVectorLayer>(
      &polylines, options.polyline_layer));
  for (int i = 0; i < geog.num_shapes(); i++) {
    auto shape = geog.Shape(i);
    if (shape->dimension() == 1) {
      builder.AddShape(*shape);
    }
  }

  builder.StartLayer(absl::make_unique<s2builderutil::S2PolygonLayer>(
      polygon.get(), options.polygon_layer));
  for (int i = 0; i < geog.num_shapes(); i++) {
    auto shape = geog.Shape(i);
    if (shape->dimension() == 2) {
      builder.AddShape(*shape);
    }
  }

  // build the output
  S2Error error;
  if (!builder.Build(&error)) {
    std::stringstream ss;
    ss << error;
    throw Exception(ss.str());
  }

  // construct output
  return s2_geography_from_layers(std::move(points), std::move(polylines),
                                  std::move(polygon), point_layer_action,
                                  polyline_layer_action, polygon_layer_action);
}

std::unique_ptr<Geography> s2_rebuild(const Geography& geog,
                                      const GlobalOptions& options) {
  return s2_rebuild(geog, options, options.point_layer_action,
                    options.polyline_layer_action,
                    options.polygon_layer_action);
}

std::unique_ptr<PointGeography> s2_build_point(const Geography& geog) {
  std::unique_ptr<Geography> geog_out = s2_rebuild(
      geog, GlobalOptions(), GlobalOptions::OutputAction::OUTPUT_ACTION_INCLUDE,
      GlobalOptions::OutputAction::OUTPUT_ACTION_ERROR,
      GlobalOptions::OutputAction::OUTPUT_ACTION_ERROR);

  if (s2_is_empty(*geog_out)) {
    return absl::make_unique<PointGeography>();
  } else {
    S2GEOGRAPHY_DCHECK(geog_out->kind() == GeographyKind::POINT);
    return std::unique_ptr<PointGeography>(
        reinterpret_cast<PointGeography*>(geog_out.release()));
  }
}

std::unique_ptr<PolylineGeography> s2_build_polyline(const Geography& geog) {
  std::unique_ptr<Geography> geog_out = s2_rebuild(
      geog, GlobalOptions(), GlobalOptions::OutputAction::OUTPUT_ACTION_ERROR,
      GlobalOptions::OutputAction::OUTPUT_ACTION_INCLUDE,
      GlobalOptions::OutputAction::OUTPUT_ACTION_ERROR);

  if (s2_is_empty(*geog_out)) {
    return absl::make_unique<PolylineGeography>();
  } else {
    S2GEOGRAPHY_DCHECK(geog_out->kind() == GeographyKind::POLYLINE);
    return std::unique_ptr<PolylineGeography>(
        reinterpret_cast<PolylineGeography*>(geog_out.release()));
  }
}

std::unique_ptr<PolygonGeography> s2_build_polygon(const Geography& geog) {
  std::unique_ptr<Geography> geog_out = s2_rebuild(
      geog, GlobalOptions(), GlobalOptions::OutputAction::OUTPUT_ACTION_ERROR,
      GlobalOptions::OutputAction::OUTPUT_ACTION_ERROR,
      GlobalOptions::OutputAction::OUTPUT_ACTION_INCLUDE);

  if (s2_is_empty(*geog_out)) {
    return absl::make_unique<PolygonGeography>();
  } else {
    S2GEOGRAPHY_DCHECK(geog_out->kind() == GeographyKind::POLYGON);
    return std::unique_ptr<PolygonGeography>(
        reinterpret_cast<PolygonGeography*>(geog_out.release()));
  }
}

void RebuildAggregator::Add(const Geography& geog) { index_.Add(geog); }

std::unique_ptr<Geography> RebuildAggregator::Finalize() {
  return s2_rebuild(index_, options_);
}

void S2CoverageUnionAggregator::Add(const Geography& geog) { index_.Add(geog); }

std::unique_ptr<Geography> S2CoverageUnionAggregator::Finalize() {
  ShapeIndexGeography empty_index_;
  return s2_boolean_operation(index_, empty_index_,
                              S2BooleanOperation::OpType::UNION, options_);
}

void S2UnionAggregator::Add(const Geography& geog) {
  if (geog.dimension() == 0 || geog.dimension() == 1) {
    root_.index1.Add(geog);
    return;
  }

  if (other_.size() == 0) {
    other_.push_back(absl::make_unique<Node>());
    other_.back()->index1.Add(geog);
    return;
  }

  Node* last = other_.back().get();
  if (last->index1.num_shapes() == 0) {
    last->index1.Add(geog);
  } else if (last->index2.num_shapes() == 0) {
    last->index2.Add(geog);
  } else {
    other_.push_back(absl::make_unique<Node>());
    other_.back()->index1.Add(geog);
  }
}

std::unique_ptr<Geography> S2UnionAggregator::Node::Merge(
    const GlobalOptions& options) {
  return s2_boolean_operation(index1, index2, S2BooleanOperation::OpType::UNION,
                              options);
}

std::unique_ptr<Geography> S2UnionAggregator::Finalize() {
  for (int j = 0; j < 100; j++) {
    if (other_.size() <= 1) {
      break;
    }

    for (int64_t i = static_cast<int64_t>(other_.size()) - 1; i >= 1;
         i = i - 2) {
      // merge other_[i] with other_[i - 1]
      std::unique_ptr<Geography> merged = other_[i]->Merge(options_);
      std::unique_ptr<Geography> merged_prev = other_[i - 1]->Merge(options_);

      // erase the last two nodes
      other_.erase(other_.begin() + i - 1, other_.begin() + i + 1);

      // ..and replace it with a single node
      other_.push_back(absl::make_unique<Node>());
      other_.back()->index1.Add(*merged);
      other_.back()->index2.Add(*merged_prev);

      // making sure to keep the underlying data alive
      other_.back()->data.push_back(std::move(merged));
      other_.back()->data.push_back(std::move(merged_prev));
    }
  }

  if (other_.size() == 0) {
    return root_.Merge(options_);
  } else {
    std::unique_ptr<Geography> merged = other_[0]->Merge(options_);
    root_.index2.Add(*merged);
    return root_.Merge(options_);
  }
}

namespace sedona_udf {

/// \brief Tracker to help obtain source edges from S2Builder::Graph edges
struct EdgeTracker {
  /// \brief Create an edge tracker with empty state
  EdgeTracker() { Clear(); }

  /// \brief Clear this tracker of any existing state
  void Clear() {
    num_edges_.clear();
    num_edges_.push_back(0);
    edge_count_ = 0;
  }

  /// \brief Add a tracked shape
  ///
  /// The number of edges of this shape is used when mapping the global
  /// (S2Builder) edge ID back to the source. This should be called paired with
  /// each addition of a shape to the builder.
  ///
  /// In general, whenever the S2Builder has to be invoked, performance is not
  /// driven by the reading and writing of coordinates, so the the overhead of
  /// resolving source edges for potentially better writes is minimal.
  void Add(const S2Shape* shape) {
    shapes_.push_back(shape);
    edge_count_ += shape->num_edges();
    num_edges_.push_back(edge_count_);
  }

  /// \brief Resolve the input source shape and local edge ID
  std::pair<const S2Shape*, int> ResolveSource(int edge_id) const {
    auto it = std::upper_bound(num_edges_.begin(), num_edges_.end(), edge_id);
    int shape_idx = static_cast<int>(std::distance(num_edges_.begin(), it)) - 1;
    return {shapes_[shape_idx], edge_id - num_edges_[shape_idx]};
  }

  /// \brief Resolve the source edge associated with the input
  ///
  /// This edge has its vertices normalized to XYZM order such that the output
  /// handling need not consider the input dimensions after the edge has been
  /// resolved.
  internal::GeoArrowEdge ResolveEdge(int edge_id) const {
    auto src = ResolveSource(edge_id);
    switch (src.first->type_tag()) {
      case GeoArrowPointShape::kTypeTag: {
        const auto* points =
            reinterpret_cast<const GeoArrowPointShape*>(src.first);
        return points->native_edge(src.second).Normalize(points->dimensions());
      }
      case GeoArrowLaxPolylineShape::kTypeTag: {
        const auto* lines =
            reinterpret_cast<const GeoArrowLaxPolylineShape*>(src.first);
        return lines->native_edge(src.second).Normalize(lines->dimensions());
      }
      case GeoArrowLaxPolygonShape::kTypeTag: {
        const auto* polygons =
            reinterpret_cast<const GeoArrowLaxPolygonShape*>(src.first);
        return polygons->native_edge(src.second)
            .Normalize(polygons->dimensions());
      }
      default:
        throw Exception("Unexpected value of type_tag()");
    }
  }

  std::vector<int> num_edges_;
  int edge_count_;
  std::vector<const S2Shape*> shapes_;
};

/// \brief Output geometry collector
///
/// When using the S2Builder or S2Builder::Layer via the S2BooleanOperation, it
/// is not generally possible to predict the exact output type in advance such
/// that output could be written directly whilst walking the graph to obtain the
/// output. To mediate this, we use the OutputGeometry to buffer one output
/// feature at a time.
///
/// This strucutre also takes care of reordering polygon rings to align with
/// shell/hole expectations of simple features output (i.e., oriented rings are
/// written to this object and reordering only occurs on output).
///
/// This object makes an attempt to reuse scratch space between iterations to
/// mitigate the fact that it has to buffer a whole geometry at a time; however,
/// this might lead to increased memory usage if a lot of threads are building
/// large output at once.
struct OutputGeometry {
  /// \brief Clear this object
  void Clear() {
    points_.clear();
    line_lengths_.clear();
    line_vertices_.clear();
    ring_offsets_.assign(1, 0);
    ring_order_.clear();
    polygon_lengths_.clear();
    polygon_vertices_.clear();
    current_line_length_ = 0;
  }

  /// \brief Add a point to the output
  ///
  /// This vertex must have been normalized before this call to correctly write
  /// ZM information to the output.
  void AddPoint(const internal::GeoArrowVertex& v) { points_.push_back(v); }

  /// \brief Add a vertex to the current line output
  ///
  /// This vertex must have been normalized before this call to correctly write
  /// ZM information to the output.
  void AddLineVertex(const internal::GeoArrowVertex& v) {
    line_vertices_.push_back(v);
    ++current_line_length_;
  }

  /// \brief Finish the current line output
  void FinishLine() {
    line_lengths_.push_back(current_line_length_);
    current_line_length_ = 0;
  }

  /// \brief Add a vertex to the current ring
  ///
  /// This vertex must have been normalized before this call to correctly write
  /// ZM information to the output.
  void AddRingVertex(const internal::GeoArrowVertex& v) {
    polygon_vertices_.push_back(v);
  }

  /// \brief Finish the current ring output
  void FinishRing() {
    ring_offsets_.push_back(static_cast<int>(polygon_vertices_.size()));
  }

  /// \brief Return true if any points were added to the output
  bool has_points() const { return !points_.empty(); }

  /// \brief Return true if any lines were added to the output
  bool has_lines() const { return !line_lengths_.empty(); }

  /// \brief Return true of any rings were added to the output
  bool has_polygons() const { return ring_offsets_.size() > 1; }

  /// \brief Return the number of output geometry dimensions
  int num_types() const { return has_points() + has_lines() + has_polygons(); }

  /// \brief Write this output to a builder
  ///
  /// This method uses the following heuristic to determine what to write:
  ///
  /// - empty output is written as empty according to geometry_type_if_empty
  /// (typically
  ///   the input geometry type is propagated)
  /// - Single points are written as POINT; multiple points are written as
  /// MULTIPOINT
  /// - Single linestrings are written as LINESTRING; multiple linestrings are
  /// written as
  ///   MULTILINESTRING
  /// - Single polygons are written as POLYGON; multiple polygons are written as
  /// MULTIPOLYGON
  /// - More than one of the above is written as a GEOMETRYCOLLECTION
  ///
  /// This is the point at which the nesting of any output polygon rings are
  /// calculated.
  void WriteTo(GeoArrowOutputBuilder* out,
               uint8_t geometry_type_if_empty =
                   GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION) {
    if (num_types() == 0) {
      out->AppendEmpty(geometry_type_if_empty);
      return;
    }

    if (num_types() == 1) {
      out->FeatureStart();
      if (has_points()) WritePointOutput(out);
      if (has_lines()) WriteLinesOutput(out);
      if (has_polygons()) WritePolygonOutput(out);
      out->FeatureEnd();
      return;
    }

    out->FeatureStart();
    out->GeomStart(GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION);
    if (has_points()) WritePointOutput(out);
    if (has_lines()) WriteLinesOutput(out);
    if (has_polygons()) WritePolygonOutput(out);
    out->GeomEnd();
    out->FeatureEnd();
  }

 private:
  void WritePointOutput(GeoArrowOutputBuilder* out) {
    if (points_.size() == 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
      out->WriteCoord(points_[0]);
      out->GeomEnd();
    } else if (points_.size() > 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_MULTIPOINT);
      for (const auto& pt : points_) {
        out->GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
        out->WriteCoord(pt);
        out->GeomEnd();
      }
      out->GeomEnd();
    }
  }

  void WriteLinesOutput(GeoArrowOutputBuilder* out) {
    if (line_lengths_.size() == 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_LINESTRING);
      for (int i = 0; i < line_lengths_[0]; ++i) {
        out->WriteCoord(line_vertices_[i]);
      }
      out->GeomEnd();
    } else if (line_lengths_.size() > 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_MULTILINESTRING);
      int line_vertex_id = 0;
      for (int line_length : line_lengths_) {
        out->GeomStart(GEOARROW_GEOMETRY_TYPE_LINESTRING);
        for (int i = 0; i < line_length; ++i) {
          out->WriteCoord(line_vertices_[line_vertex_id++]);
        }
        out->GeomEnd();
      }
      out->GeomEnd();
    }
  }

  void WritePolygonOutput(GeoArrowOutputBuilder* out) {
    GroupRings();

    int order_id = 0;
    if (polygon_lengths_.size() == 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_POLYGON);
      for (int r = 0; r < polygon_lengths_[0]; ++r) {
        int ri = ring_order_[order_id++];
        out->RingStart();
        for (int i = ring_offsets_[ri]; i < ring_offsets_[ri + 1]; ++i) {
          out->WriteCoord(polygon_vertices_[i]);
        }
        out->RingEnd();
      }
      out->GeomEnd();
    } else if (polygon_lengths_.size() > 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON);
      for (int polygon_length : polygon_lengths_) {
        out->GeomStart(GEOARROW_GEOMETRY_TYPE_POLYGON);
        for (int r = 0; r < polygon_length; ++r) {
          int ri = ring_order_[order_id++];
          out->RingStart();
          for (int i = ring_offsets_[ri]; i < ring_offsets_[ri + 1]; ++i) {
            out->WriteCoord(polygon_vertices_[i]);
          }
          out->RingEnd();
        }
        out->GeomEnd();
      }
      out->GeomEnd();
    }
  }

  void BuildRingNodes() {
    int num_rings = static_cast<int>(ring_offsets_.size()) - 1;
    ring_nodes_.resize(num_rings);
    const uint8_t* base =
        reinterpret_cast<const uint8_t*>(polygon_vertices_.data());
    for (int i = 0; i < num_rings; ++i) {
      struct GeoArrowBufferView coords;
      coords.data = base + ring_offsets_[i] * sizeof(internal::GeoArrowVertex);
      coords.size_bytes = (ring_offsets_[i + 1] - ring_offsets_[i]) *
                          sizeof(internal::GeoArrowVertex);
      GeoArrowGeometryNodeSetInterleaved(&ring_nodes_[i],
                                         GEOARROW_GEOMETRY_TYPE_LINESTRING,
                                         GEOARROW_DIMENSIONS_XYZM, coords);
    }
  }

  void GroupRings() {
    polygon_lengths_.clear();
    int num_rings = static_cast<int>(ring_offsets_.size()) - 1;

    if (num_rings == 0) {
      ring_order_.clear();
      return;
    }

    // Fast path: single ring -> single polygon
    if (num_rings == 1) {
      polygon_lengths_.push_back(1);
      ring_order_ = {0};
      return;
    }

    // Build ring nodes so we can use GeoArrowLoop for utilities
    BuildRingNodes();

    // Classify shells vs holes using signed area.
    // S2Builder outputs directed edges with interior to the left, so
    // shells are CCW (positive signed area) and holes are CW (negative).
    ring_signed_areas_.resize(num_rings);
    std::vector<int> shells;
    std::vector<int> holes;

    for (int i = 0; i < num_rings; ++i) {
      GeoArrowLoop loop(&ring_nodes_[i], &scratch_);
      ring_signed_areas_[i] = loop.GetSignedArea();
      if (ring_signed_areas_[i] >= 0) {
        shells.push_back(i);
      } else {
        holes.push_back(i);
      }
    }

    // Common case: exactly one shell with zero or more holes
    if (shells.size() == 1) {
      polygon_lengths_.push_back(num_rings);
      ring_order_.clear();
      ring_order_.reserve(num_rings);
      ring_order_.push_back(shells[0]);
      for (int h : holes) ring_order_.push_back(h);
      return;
    }

    // Multiple shells: build S2Loops for containment checks, which are
    // more efficient than brute force when checking multiple holes.
    std::vector<std::unique_ptr<S2Loop>> s2loops(shells.size());
    for (size_t s = 0; s < shells.size(); ++s) {
      GeoArrowChain loop(&ring_nodes_[shells[s]]);
      scratch_.clear();
      scratch_.reserve(loop.size() - 1);
      loop.VisitVertices(0, loop.size() - 1, [&](const S2Point& pt) {
        scratch_.push_back(pt);
        return true;
      });
      s2loops[s] = absl::make_unique<S2Loop>(scratch_, S2Debug::DISABLE);
      S2GEOGRAPHY_DCHECK(s2loops[s]->IsValid());
    }

    // Match each hole to its containing shell.
    // Among all containing shells, pick the smallest (by area).
    std::vector<int> hole_parent(holes.size(), -1);
    for (size_t j = 0; j < holes.size(); ++j) {
      GeoArrowLoop hole_loop(&ring_nodes_[holes[j]], &scratch_);
      S2Point test_point = hole_loop.vertex(0);
      int best_shell = -1;
      double best_area = std::numeric_limits<double>::max();
      for (size_t s = 0; s < shells.size(); ++s) {
        if (s2loops[s]->Contains(test_point)) {
          double area = ring_signed_areas_[shells[s]];
          if (area < best_area) {
            best_area = area;
            best_shell = static_cast<int>(s);
          }
        }
      }
      hole_parent[j] = best_shell;
    }

    // Build polygon_lengths_ and ring_order_: each shell followed by its
    // holes
    ring_order_.clear();
    ring_order_.reserve(num_rings);
    for (size_t s = 0; s < shells.size(); ++s) {
      int ring_count = 1;
      ring_order_.push_back(shells[s]);
      for (size_t j = 0; j < holes.size(); ++j) {
        if (hole_parent[j] == static_cast<int>(s)) {
          ring_order_.push_back(holes[j]);
          ++ring_count;
        }
      }
      polygon_lengths_.push_back(ring_count);
    }
  }

  std::vector<internal::GeoArrowVertex> points_;
  std::vector<int> line_lengths_;
  std::vector<internal::GeoArrowVertex> line_vertices_;
  std::vector<int> ring_offsets_;
  std::vector<int> ring_order_;
  std::vector<int> polygon_lengths_;
  std::vector<internal::GeoArrowVertex> polygon_vertices_;
  int current_line_length_{0};
  std::vector<struct GeoArrowGeometryNode> ring_nodes_;
  std::vector<double> ring_signed_areas_;
  std::vector<S2Point> scratch_;
};

// Common utilities for building output from the edge tracker and the
// S2Builder::Graph
void PopulateVertex(const S2Builder::Graph& g, const EdgeTracker& tracker,
                    int edge_id, const S2Point& v,
                    internal::GeoArrowVertex* vt) {
  // For now, always pick the first piece of edge information to propagate
  // from the source. This could also be averaged or otherwise aggregated
  // (e.g., min, max, median). GEOS seems to propagate the first piece of
  // information for points but averages ZM information of edge crossings.
  for (int input_edge_id : g.input_edge_ids(edge_id)) {
    *vt = tracker.ResolveEdge(input_edge_id).Interpolate(v);
    break;
  }

  // If this vertex was snapped to a new location, use the snapped output.
  // Otherwise, use the original input to avoid rounding errors from the
  // roundtrip to S2Point.
  if (vt->ToPoint() != v) {
    vt->SetPoint(v);
  }
}

template <typename Visit>
void VisitTrackedVertices(const S2Builder::Graph& g, const EdgeTracker& tracker,
                          const std::vector<int>& edge_loop, Visit&& visit) {
  internal::GeoArrowVertex vt;

  S2Point first_pt = g.vertex(g.edge(edge_loop[0]).first);
  PopulateVertex(g, tracker, edge_loop[0], first_pt, &vt);
  visit(vt);

  for (int edge_id : edge_loop) {
    S2Point pt = g.vertex(g.edge(edge_id).second);
    PopulateVertex(g, tracker, edge_loop[0], pt, &vt);
    visit(vt);
  }
}

template <typename Visit>
void VisitVertices(const S2Builder::Graph& g, const std::vector<int>& edge_loop,
                   Visit&& visit) {
  internal::GeoArrowVertex vt;

  S2Point first_pt = g.vertex(g.edge(edge_loop[0]).first);
  vt.SetPoint(first_pt);
  visit(vt);

  for (int edge_id : edge_loop) {
    S2Point pt = g.vertex(g.edge(edge_id).second);
    vt.SetPoint(pt);
    visit(vt);
  }
}

/// \brief Output layer that collects degenerate edges in the graph as point
/// output
class GeoArrowPointVectorLayer : public S2Builder::Layer {
 public:
  using GraphOptions = S2Builder::GraphOptions;
  using Graph = S2Builder::Graph;
  using EdgeId = Graph::EdgeId;
  using InputEdgeId = Graph::InputEdgeId;

  GeoArrowPointVectorLayer(OutputGeometry* output,
                           EdgeTracker* edge_tracker = nullptr)
      : edge_tracker_(edge_tracker), output_(output) {}

  GraphOptions graph_options() const override {
    return GraphOptions(
        S2Builder::EdgeType::DIRECTED, GraphOptions::DegenerateEdges::KEEP,
        GraphOptions::DuplicateEdges::MERGE, GraphOptions::SiblingPairs::KEEP);
  }

  void Build(const Graph& g, S2Error* error) override {
    internal::GeoArrowVertex vt;

    for (EdgeId edge_id = 0; static_cast<size_t>(edge_id) < g.edges().size();
         ++edge_id) {
      // Resolve the edge
      const auto& edge = g.edge(edge_id);

      // We only collect degenerate edges to output as points
      if (edge.first != edge.second) {
        continue;
      }

      // Resolve the vertex as an S2Point
      S2Point pt = g.vertex(edge.first);

      // If we have the ability to propagate input edge information to the
      // output, use the EdgeTracker to resolve input edges. Otherwise, just
      // write 2D points based on the builder-derived vertex.
      if (edge_tracker_) {
        PopulateVertex(g, *edge_tracker_, edge.first, pt, &vt);
      } else {
        vt.SetPoint(pt);
      }

      // Write to the output
      output_->AddPoint(vt);
    }
  }

 private:
  EdgeTracker* edge_tracker_;
  OutputGeometry* output_;
};

/// \brief Output layer that collects line edge collections in the graph
class GeoArrowPolylinesLayer : public S2Builder::Layer {
 public:
  using GraphOptions = S2Builder::GraphOptions;
  using Graph = S2Builder::Graph;
  using EdgeId = Graph::EdgeId;
  using InputEdgeId = Graph::InputEdgeId;

  GeoArrowPolylinesLayer(OutputGeometry* output,
                         EdgeTracker* edge_tracker = nullptr)
      : edge_tracker_(edge_tracker), output_(output) {}

  GraphOptions graph_options() const override {
    return GraphOptions(S2Builder::EdgeType::DIRECTED,
                        GraphOptions::DegenerateEdges::DISCARD,
                        GraphOptions::DuplicateEdges::MERGE,
                        GraphOptions::SiblingPairs::DISCARD);
  }

  void Build(const Graph& g, S2Error* error) override {
    std::vector<Graph::EdgePolyline> edge_polylines =
        g.GetPolylines(Graph::PolylineType::WALK);

    // If we can track input edge information, do so now (or else
    // use to a simpler output path)
    if (edge_tracker_) {
      for (const auto& edge_polyline : edge_polylines) {
        VisitTrackedVertices(g, *edge_tracker_, edge_polyline,
                             [&](const internal::GeoArrowVertex& vt) {
                               output_->AddLineVertex(vt);
                             });
        output_->FinishLine();
      }
    } else {
      for (const auto& edge_polyline : edge_polylines) {
        VisitVertices(g, edge_polyline,
                      [&](const internal::GeoArrowVertex& vt) {
                        output_->AddLineVertex(vt);
                      });
        output_->FinishLine();
      }
    }
  }

 private:
  EdgeTracker* edge_tracker_;
  OutputGeometry* output_;
};

/// \brief Output layer that collects polygon edge collections in the graph
class GeoArrowPolygonLayer : public S2Builder::Layer {
 public:
  using GraphOptions = S2Builder::GraphOptions;
  using Graph = S2Builder::Graph;
  using EdgeId = Graph::EdgeId;
  using InputEdgeId = Graph::InputEdgeId;

  GeoArrowPolygonLayer(OutputGeometry* output,
                       EdgeTracker* edge_tracker = nullptr)
      : edge_tracker_(edge_tracker), output_(output) {}

  GraphOptions graph_options() const override {
    return GraphOptions(S2Builder::EdgeType::DIRECTED,
                        GraphOptions::DegenerateEdges::DISCARD,
                        GraphOptions::DuplicateEdges::MERGE,
                        GraphOptions::SiblingPairs::DISCARD);
  }

  void Build(const Graph& g, S2Error* error) override {
    edge_loops_.clear();
    if (!g.GetDirectedLoops(Graph::LoopType::SIMPLE, &edge_loops_, error)) {
      return;
    }

    if (edge_tracker_) {
      for (const auto& edge_loop : edge_loops_) {
        VisitTrackedVertices(g, *edge_tracker_, edge_loop,
                             [&](const internal::GeoArrowVertex& vt) {
                               output_->AddRingVertex(vt);
                             });
        output_->FinishRing();
      }
    } else {
      for (const auto& edge_loop : edge_loops_) {
        VisitVertices(g, edge_loop, [&](const internal::GeoArrowVertex& vt) {
          output_->AddRingVertex(vt);
        });
        output_->FinishRing();
      }
    }
  }

 private:
  EdgeTracker* edge_tracker_;
  OutputGeometry* output_;
  std::vector<Graph::EdgeLoop> edge_loops_;
};

struct RebuildExec {
  RebuildExec(const S2Builder::Options& options = S2Builder::Options())
      : builder_options_(options) {
    builder_.Init(builder_options_);
  }

  void Exec(const GeoArrowGeography& value0, GeoArrowOutputBuilder* out) {
    builder_.Reset();
    edge_tracker_.Clear();
    output_.Clear();

    // Start a layer that collects point vertices
    builder_.StartLayer(
        absl::make_unique<GeoArrowPointVectorLayer>(&output_, &edge_tracker_));

    edge_tracker_.Add(value0.points());
    value0.points()->geom().VisitVertices([&](const S2Point& v) {
      builder_.AddPoint(v);
      return true;
    });

    // Start a layer that collects polyline vertices
    builder_.StartLayer(
        absl::make_unique<GeoArrowPolylinesLayer>(&output_, &edge_tracker_));

    edge_tracker_.Add(value0.lines());
    builder_.AddShape(*value0.lines());

    // Start a layer that collects polygon vertices
    builder_.StartLayer(
        absl::make_unique<GeoArrowPolygonLayer>(&output_, &edge_tracker_));

    edge_tracker_.Add(value0.polygons());
    builder_.AddShape(*value0.polygons());

    // build the output
    S2Error error;
    if (!builder_.Build(&error)) {
      std::stringstream ss;
      ss << error;
      throw Exception(ss.str());
    }

    // Write the output. For unary input we write the same output dimensions as
    // the input
    out->SetDimensions(value0.dimensions());
    output_.WriteTo(out, value0.geometry_type());
  }

  S2Builder builder_;
  S2Builder::Options builder_options_;
  EdgeTracker edge_tracker_;
  OutputGeometry output_;
};

struct ReducePrecisionExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = GeoArrowOutputBuilder;

  void Exec(arg0_t::c_type value, double grid_size, out_t* out) {
    // If the grid size changed since the last iteration, we need to recreate
    // the snap function and reinitialize the builder with the new options
    if (grid_size != last_grid_size_) {
      if (grid_size > 0) {
        int exponent = static_cast<int>(std::round(-std::log10(grid_size)));
        exponent = std::max(
            s2builderutil::IntLatLngSnapFunction::kMinExponent,
            std::min(s2builderutil::IntLatLngSnapFunction::kMaxExponent,
                     exponent));
        s2builderutil::IntLatLngSnapFunction snap(exponent);
        rebuild_.builder_options_.set_snap_function(snap);
      } else {
        s2builderutil::IdentitySnapFunction snap;
        rebuild_.builder_options_.set_snap_function(snap);
      }

      rebuild_.builder_.Init(rebuild_.builder_options_);
      last_grid_size_ = grid_size;
    }

    rebuild_.Exec(value, out);
  }

  RebuildExec rebuild_;
  double last_grid_size_{-100};
};

template <S2BooleanOperation::OpType op_type>
struct BooleanOperationExec {
  using arg0_t = GeoArrowGeographyInputView;
  using arg1_t = GeoArrowGeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Exec(arg0_t::c_type value0, arg1_t::c_type value1, out_t* out) {
    out->Append(*s2_boolean_operation(value0.ShapeIndex(), value1.ShapeIndex(),
                                      op_type, options_));
  }

  GlobalOptions options_;
};

void DifferenceKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<
      BooleanOperationExec<S2BooleanOperation::OpType::DIFFERENCE>>(
      out, "st_difference");
}

void SymDifferenceKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<
      BooleanOperationExec<S2BooleanOperation::OpType::SYMMETRIC_DIFFERENCE>>(
      out, "st_symdifference");
}

void IntersectionKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<
      BooleanOperationExec<S2BooleanOperation::OpType::INTERSECTION>>(
      out, "st_intersection");
}

void UnionKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<BooleanOperationExec<S2BooleanOperation::OpType::UNION>>(
      out, "st_union");
}

void ReducePrecisionKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<ReducePrecisionExec>(out, "st_reduceprecision");
}

}  // namespace sedona_udf

}  // namespace s2geography
