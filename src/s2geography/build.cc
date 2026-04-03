
#include "s2geography/build.h"

#include <s2/s2boolean_operation.h>
#include <s2/s2builder.h>
#include <s2/s2builderutil_closed_set_normalizer.h>
#include <s2/s2builderutil_s2point_vector_layer.h>
#include <s2/s2builderutil_s2polygon_layer.h>
#include <s2/s2builderutil_s2polyline_vector_layer.h>
#include <s2/s2builderutil_snap_functions.h>

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

struct EdgeTracker {
  EdgeTracker() { Clear(); }

  void Clear() {
    num_edges_.clear();
    num_edges_.push_back(0);
    edge_count_ = 0;
  }

  void Add(const S2Shape* shape) {
    shapes_.push_back(shape);
    edge_count_ += shape->num_edges();
    num_edges_.push_back(edge_count_);
  }

  std::pair<const S2Shape*, int> ResolveSource(int edge_id) {
    auto it = std::upper_bound(num_edges_.begin(), num_edges_.end(), edge_id);
    int shape_idx = static_cast<int>(std::distance(num_edges_.begin(), it)) - 1;
    return {shapes_[shape_idx], edge_id - num_edges_[shape_idx]};
  }

  internal::GeoArrowEdge ResolveEdge(int edge_id) {
    auto src = ResolveSource(edge_id);
    switch (src.first->dimension()) {
      case 0: {
        const auto* points =
            reinterpret_cast<const GeoArrowPointShape*>(src.first);
        return points->native_edge(src.second).Normalize(points->dimensions());
      }
      case 1: {
        const auto* lines =
            reinterpret_cast<const GeoArrowLaxPolylineShape*>(src.first);
        return lines->native_edge(src.second).Normalize(lines->dimensions());
      }
      case 2: {
        const auto* polygons =
            reinterpret_cast<const GeoArrowLaxPolygonShape*>(src.first);
        return polygons->native_edge(src.second)
            .Normalize(polygons->dimensions());
      }
      default:
        throw Exception("Unexpected value of dimension()");
    }
  }

  std::vector<int> num_edges_;
  int edge_count_;
  std::vector<const S2Shape*> shapes_;
};

/// \brief A reference to a source edge, used as a label payload
struct SourceEdgeRef {
  int shape_id;
  int edge_id;
};

/// \brief An S2Builder layer that collects degenerate edges (points) and
/// resolves them back to native GeoArrow vertices via labels.
///
/// Like s2builderutil::S2PointVectorLayer, this layer expects all edges to be
/// degenerate. It uses Graph::LabelFetcher to retrieve source (shape_id,
/// edge_id) references that were attached as labels when the edges were added
/// to the builder, then resolves those references against the source
/// GeoArrowGeography to recover lossless lon/lat/z/m coordinates.
class GeoArrowPointVectorLayer : public S2Builder::Layer {
 public:
  using GraphOptions = S2Builder::GraphOptions;
  using Graph = S2Builder::Graph;
  using EdgeId = Graph::EdgeId;
  using InputEdgeId = Graph::InputEdgeId;

  GeoArrowPointVectorLayer(EdgeTracker* edge_tracker,
                           std::vector<internal::GeoArrowVertex>* points_out)
      : edge_tracker_(edge_tracker), points_out_(points_out) {}

  GraphOptions graph_options() const override {
    return GraphOptions(
        S2Builder::EdgeType::DIRECTED, GraphOptions::DegenerateEdges::KEEP,
        GraphOptions::DuplicateEdges::MERGE, GraphOptions::SiblingPairs::KEEP);
  }

  void Build(const Graph& g, S2Error* error) override {
    for (EdgeId edge_id = 0; static_cast<size_t>(edge_id) < g.edges().size();
         ++edge_id) {
      // Resolve the edge
      const auto& edge = g.edge(edge_id);
      if (edge.first != edge.second) {
        *error = S2Error::InvalidArgument("Found non-degenerate edges");
        continue;
      }

      // Resolve the vertex as an S2Point
      S2Point pt = g.vertex(edge.first);
      internal::GeoArrowVertex vt;

      // GEOS seems to always return the first Z or M it encounters for point
      // output
      for (InputEdgeId input_edge_id : g.input_edge_ids(edge_id)) {
        auto e = edge_tracker_->ResolveEdge(input_edge_id);
        vt = e.v0;
        break;
      }

      // If this vertex was snapped to a new location, set its vertex
      if (pt != vt.ToPoint()) {
        vt.SetPoint(pt);
      }

      // Write to the output
      Write(vt);
    }
  }

  void Write(const internal::GeoArrowVertex& v) { points_out_->push_back(v); }

 private:
  EdgeTracker* edge_tracker_;
  std::vector<internal::GeoArrowVertex>* points_out_;
  GraphOptions::DuplicateEdges duplicate_edges_;
};

struct RebuildExec {
  using arg0_t = GeoArrowGeographyInputView;
  using out_t = GeoArrowOutputBuilder;

  RebuildExec(const S2Builder::Options& options = S2Builder::Options())
      : builder_options_(options) {
    builder_.Init(builder_options_);
  }

  void Exec(const GeoArrowGeography& value0, GeoArrowOutputBuilder* out) {
    builder_.Reset();
    native_points_.clear();
    edge_tracker_.Clear();

    // Start a layer that collects point vertices
    builder_.StartLayer(absl::make_unique<GeoArrowPointVectorLayer>(
        &edge_tracker_, &native_points_));

    edge_tracker_.Add(value0.points());
    value0.points()->geom().VisitVertices([&](const S2Point& v) {
      builder_.AddPoint(v);
      return true;
    });

    // TODO: add GeoArrow-aware layers for lines and polygons
    // builder_.AddShape(*value0.lines());
    // builder_.AddShape(*value0.polygons());

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

    // If there is only point output, write it
    if (!native_points_.empty()) {
      out->FeatureStart();
      WritePointOutput(out);
      out->FeatureEnd();
      return;
    }

    // TODO: check for only polyline output

    // TODO: check for only polgon output

    // Otherwise, write a GEOMETRYCOLLECTION with any of the available output
    out->FeatureStart();
    out->GeomStart(GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION);
    WritePointOutput(out);
    out->GeomEnd();
    out->FeatureEnd();
  }

  void WritePointOutput(out_t* out) {
    if (native_points_.size() == 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
      out->WriteCoord(native_points_[0]);
      out->GeomEnd();
    } else if (native_points_.size() > 1) {
      out->GeomStart(GEOARROW_GEOMETRY_TYPE_MULTIPOINT);
      for (const auto& pt : native_points_) {
        out->GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
        out->WriteCoord(pt);
        out->GeomEnd();
      }
      out->GeomEnd();
    }
  }

  S2Builder builder_;
  S2Builder::Options builder_options_;
  EdgeTracker edge_tracker_;
  std::vector<internal::GeoArrowVertex> native_points_;
};

struct UnaryUnionGridSizeExec {
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

void UnaryUnionGridSizeKernel(struct SedonaCScalarKernel* out) {
  InitBinaryKernel<UnaryUnionGridSizeExec>(out, "st_unaryunion");
}

}  // namespace sedona_udf

}  // namespace s2geography
