
#include "s2geography/geography.h"

#include <s2/encoded_s2shape_index.h>
#include <s2/mutable_s2shape_index.h>
#include <s2/s2lax_polygon_shape.h>
#include <s2/s2lax_polyline_shape.h>
#include <s2/s2point_region.h>
#include <s2/s2point_vector_shape.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>
#include <s2/s2region.h>
#include <s2/s2region_union.h>
#include <s2/s2shape.h>
#include <s2/s2shape_index_region.h>
#include <s2/s2shapeutil_coding.h>

using namespace s2geography;

// This class is a shim to allow a class to return a std::unique_ptr<S2Shape>(),
// which is required by MutableS2ShapeIndex::Add(), without copying the
// underlying data. S2Shape instances do not typically own their data (e.g.,
// S2Polygon::Shape), so this does not change the general relationship (that
// anything returned by Geography::Shape() is only valid within the scope of
// the Geography). Note that this class is also available (but not exposed) in
// s2/s2shapeutil_coding.cc.
class S2ShapeWrapper : public S2Shape {
 public:
  S2ShapeWrapper(const S2Shape* shape) : shape_(shape) {}
  int num_edges() const { return shape_->num_edges(); }
  Edge edge(int edge_id) const { return shape_->edge(edge_id); }
  int dimension() const { return shape_->dimension(); }
  ReferencePoint GetReferencePoint() const {
    return shape_->GetReferencePoint();
  }
  int num_chains() const { return shape_->num_chains(); }
  Chain chain(int chain_id) const { return shape_->chain(chain_id); }
  Edge chain_edge(int chain_id, int offset) const {
    return shape_->chain_edge(chain_id, offset);
  }
  ChainPosition chain_position(int edge_id) const {
    return shape_->chain_position(edge_id);
  }

 private:
  const S2Shape* shape_;
};

// Just like the S2ShapeWrapper, the S2RegionWrapper helps reconcile the
// differences in lifecycle expectation between S2 and Geography. We often
// need access to a S2Region to generalize algorithms; however, there are some
// operations that need ownership of the region (e.g., the S2RegionUnion). In
// Geography the assumption is that anything returned by a Geography is only
// valid for the lifetime of the underlying Geography. A different design of
// the algorithms implemented here might well make this unnecessary.
class S2RegionWrapper : public S2Region {
 public:
  S2RegionWrapper(S2Region* region) : region_(region) {}
  S2Region* Clone() const { return region_->Clone(); }
  S2Cap GetCapBound() const { return region_->GetCapBound(); }
  S2LatLngRect GetRectBound() const { return region_->GetRectBound(); }
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const {
    return region_->GetCellUnionBound(cell_ids);
  }
  bool Contains(const S2Cell& cell) const { return region_->Contains(cell); }
  bool MayIntersect(const S2Cell& cell) const {
    return region_->MayIntersect(cell);
  }
  bool Contains(const S2Point& p) const { return region_->Contains(p); }

 private:
  S2Region* region_;
};

void Geography::GetCellUnionBound(std::vector<S2CellId>* cell_ids) const {
  MutableS2ShapeIndex index;
  for (int i = 0; i < num_shapes(); i++) {
    index.Add(Shape(i));
  }

  MakeS2ShapeIndexRegion<MutableS2ShapeIndex>(&index).GetCellUnionBound(
      cell_ids);
}

std::unique_ptr<S2Shape> PointGeography::Shape(int id) const {
  DCHECK(id == 0);
  return absl::make_unique<S2PointVectorShape>(points_);
}

std::unique_ptr<S2Region> PointGeography::Region() const {
  auto region = absl::make_unique<S2RegionUnion>();
  for (const S2Point& point : points_) {
    region->Add(absl::make_unique<S2PointRegion>(point));
  }

  // because Rtools for R 3.6 on Windows complains about a direct
  // return region
  return std::unique_ptr<S2Region>(region.release());
}

void PointGeography::GetCellUnionBound(std::vector<S2CellId>* cell_ids) const {
  if (points_.size() < 10) {
    for (const S2Point& point : points_) {
      cell_ids->push_back(S2CellId(point));
    }
  } else {
    Geography::GetCellUnionBound(cell_ids);
  }
}

int PolylineGeography::num_shapes() const {
  return static_cast<int>(polylines_.size());
}

std::unique_ptr<S2Shape> PolylineGeography::Shape(int id) const {
  return absl::make_unique<S2Polyline::Shape>(polylines_[id].get());
}

std::unique_ptr<S2Region> PolylineGeography::Region() const {
  auto region = absl::make_unique<S2RegionUnion>();
  for (const auto& polyline : polylines_) {
    region->Add(absl::make_unique<S2RegionWrapper>(polyline.get()));
  }
  // because Rtools for R 3.6 on Windows complains about a direct
  // return region
  return std::unique_ptr<S2Region>(region.release());
}

void PolylineGeography::GetCellUnionBound(
    std::vector<S2CellId>* cell_ids) const {
  for (const auto& polyline : polylines_) {
    polyline->GetCellUnionBound(cell_ids);
  }
}

PolygonGeography::PolygonGeography()
    : Geography(GeographyKind::POLYGON),
      polygon_(std::make_unique<S2Polygon>()) {}

int PolygonGeography::num_shapes() const {
  if (polygon_->is_empty()) {
    return 0;
  } else {
    return 1;
  }
}

std::unique_ptr<S2Shape> PolygonGeography::Shape(int /*id*/) const {
  return absl::make_unique<S2Polygon::Shape>(polygon_.get());
}

std::unique_ptr<S2Region> PolygonGeography::Region() const {
  return absl::make_unique<S2RegionWrapper>(polygon_.get());
}

void PolygonGeography::GetCellUnionBound(
    std::vector<S2CellId>* cell_ids) const {
  polygon_->GetCellUnionBound(cell_ids);
}

int GeographyCollection::num_shapes() const { return total_shapes_; }

std::unique_ptr<S2Shape> GeographyCollection::Shape(int id) const {
  int sum_shapes_ = 0;
  for (int i = 0; i < static_cast<int>(features_.size()); i++) {
    sum_shapes_ += num_shapes_[i];
    if (id < sum_shapes_) {
      return features_[i]->Shape(id - sum_shapes_ + num_shapes_[i]);
    }
  }

  throw Exception("shape id out of bounds");
}

std::unique_ptr<S2Region> GeographyCollection::Region() const {
  auto region = absl::make_unique<S2RegionUnion>();
  for (const auto& feature : features_) {
    region->Add(feature->Region());
  }
  // because Rtools for R 3.6 on Windows complains about a direct
  // return region
  return std::unique_ptr<S2Region>(region.release());
}

ShapeIndexGeography::ShapeIndexGeography(const Geography& geog)
    : Geography(GeographyKind::SHAPE_INDEX) {
  shape_index_ = absl::make_unique<MutableS2ShapeIndex>();
  Add(geog);
}

ShapeIndexGeography::ShapeIndexGeography()
    : Geography(GeographyKind::SHAPE_INDEX) {
  shape_index_ = absl::make_unique<MutableS2ShapeIndex>();
}

ShapeIndexGeography::ShapeIndexGeography(int max_edges_per_cell)
    : Geography(GeographyKind::SHAPE_INDEX) {
  MutableS2ShapeIndex::Options options;
  options.set_max_edges_per_cell(max_edges_per_cell);
  shape_index_ = absl::make_unique<MutableS2ShapeIndex>(options);
}

int ShapeIndexGeography::num_shapes() const {
  return shape_index_->num_shape_ids();
}

std::unique_ptr<S2Shape> ShapeIndexGeography::Shape(int id) const {
  const S2Shape* shape = shape_index_->shape(id);
  return std::unique_ptr<S2Shape>(new S2ShapeWrapper(shape));
}

std::unique_ptr<S2Region> ShapeIndexGeography::Region() const {
  return absl::make_unique<S2ShapeIndexRegion<MutableS2ShapeIndex>>(
      shape_index_.get());
}

int ShapeIndexGeography::Add(const Geography& geog) {
  int id = -1;
  for (int i = 0; i < geog.num_shapes(); i++) {
    id = shape_index_->Add(geog.Shape(i));
  }
  return id;
}

EncodedShapeIndexGeography::EncodedShapeIndexGeography()
    : Geography(GeographyKind::ENCODED_SHAPE_INDEX) {
  shape_index_ = absl::make_unique<EncodedS2ShapeIndex>();
}

int EncodedShapeIndexGeography::num_shapes() const {
  return shape_index_->num_shape_ids();
}

std::unique_ptr<S2Shape> EncodedShapeIndexGeography::Shape(int id) const {
  const S2Shape* shape = shape_index_->shape(id);
  if (shape == nullptr) {
    throw Exception("Error decoding shape at with id " + std::to_string(id));
  }

  return std::unique_ptr<S2Shape>(new S2ShapeWrapper(shape));
}

std::unique_ptr<S2Region> EncodedShapeIndexGeography::Region() const {
  auto mutable_index =
      reinterpret_cast<EncodedS2ShapeIndex*>(shape_index_.get());
  return absl::make_unique<S2ShapeIndexRegion<EncodedS2ShapeIndex>>(
      mutable_index);
}

void PointGeography::Encode(Encoder* encoder,
                            const EncodeOptions& options) const {
  s2coding::CodingHint hint;
  if (options.flags & EncodeOptions::kFlagCompact) {
    hint = s2coding::CodingHint::COMPACT;
  } else {
    hint = s2coding::CodingHint::FAST;
  }

  s2coding::EncodeS2PointVector(points_, hint, encoder);
}

void PointGeography::Decode(Decoder* decoder, const EncodeOptions& options) {
  s2coding::EncodedS2PointVector encoded;
  if (!encoded.Init(decoder)) {
    throw Exception("PointGeography::Decode error");
  }

  points_ = encoded.Decode();
}

void PolylineGeography::Encode(Encoder* encoder,
                               const EncodeOptions& options) const {
  encoder->Ensure(sizeof(uint32_t));
  encoder->put32(static_cast<uint32_t>(polylines_.size()));

  s2coding::CodingHint hint;
  if (options.flags & EncodeOptions::kFlagCompact) {
    hint = s2coding::CodingHint::COMPACT;
  } else {
    hint = s2coding::CodingHint::FAST;
  }

  for (const auto& polyline : polylines_) {
    polyline->Encode(encoder, hint);
  }
}

void PolylineGeography::Decode(Decoder* decoder, const EncodeOptions& options) {
  if (decoder->avail() < sizeof(uint32_t)) {
    throw Exception(
        "PolylineGeography::Decode error: insufficient header bytes");
  }

  uint32_t n_polylines = decoder->get32();
  for (uint32_t i = 0; i < n_polylines; i++) {
    auto polyline = absl::make_unique<S2Polyline>();
    if (!polyline->Decode(decoder)) {
      throw Exception("PolylineGeography::Decode error at item " +
                      std::to_string(i));
    }

    polylines_.push_back(std::move(polyline));
  }
}

void PolygonGeography::Encode(Encoder* encoder,
                              const EncodeOptions& options) const {
  s2coding::CodingHint hint;
  if (options.flags & EncodeOptions::kFlagCompact) {
    hint = s2coding::CodingHint::COMPACT;
  } else {
    hint = s2coding::CodingHint::FAST;
  }

  polygon_->Encode(encoder, hint);
}

void PolygonGeography::Decode(Decoder* decoder, const EncodeOptions& options) {
  polygon_->Decode(decoder);
}

void GeographyCollection::Encode(Encoder* encoder,
                                 const EncodeOptions& options) const {
  encoder->Ensure(sizeof(uint32_t));
  encoder->put32(static_cast<uint32_t>(features_.size()));
  for (const auto& feature : features_) {
    feature->EncodeTagged(encoder, options);
  }
}

void GeographyCollection::Decode(Decoder* decoder,
                                 const EncodeOptions& options) {
  uint32_t n_features = decoder->get32();
  for (uint32_t i = 0; i < n_features; i++) {
    features_.push_back(Geography::DecodeTagged(decoder));
  }
}

namespace {

bool CustomCompactTaggedShapeEncoder(const S2Shape& shape, Encoder* encoder) {
  if (shape.type_tag() == S2Polygon::Shape::kTypeTag) {
    // There is probably a better way to go about this than copy all vertices
    std::vector<std::vector<S2Point>> loops;
    for (int i = 0; i < shape.num_chains(); i++) {
      auto vertices = shape.vertices(i);
      loops.emplace_back(vertices.begin(), vertices.end());
    }

    S2LaxPolygonShape new_shape(std::move(loops));

    encoder->put_varint32(new_shape.type_tag());
    return s2shapeutil::CompactEncodeShape(new_shape, encoder);
  } else if (shape.type_tag() == S2Polyline::Shape::kTypeTag &&
             shape.num_chains() == 1) {
    // There is probably a better way to go about this than copy all vertices
    auto vertices = shape.vertices(0);
    std::vector<S2Point> vertices_copy(vertices.begin(), vertices.end());
    S2LaxPolylineShape new_shape(std::move(vertices_copy));

    encoder->put_varint32(new_shape.type_tag());
    return s2shapeutil::CompactEncodeShape(new_shape, encoder);
  } else {
    encoder->put_varint32(shape.type_tag());
    return s2shapeutil::CompactEncodeShape(shape, encoder);
  }
}
}  // namespace

void ShapeIndexGeography::Encode(Encoder* encoder,
                                 const EncodeOptions& options) const {
  if (options.flags & EncodeOptions::kFlagCompact) {
    if (!(options.flags & EncodeOptions::kFlagCompact)) {
      throw Exception("Lazy output only supported with the compact option");
    }

    s2coding::StringVectorEncoder shape_vector;
    for (S2Shape* shape : *shape_index_) {
      Encoder* sub_encoder = shape_vector.AddViaEncoder();
      if (shape == nullptr) continue;  // Encode as zero bytes.

      sub_encoder->Ensure(Encoder::kVarintMax32);

      if (!CustomCompactTaggedShapeEncoder(*shape, sub_encoder)) {
        throw Exception("Error encoding shape");
      }
    }
    shape_vector.Encode(encoder);
  } else if (options.flags & EncodeOptions::kFlagCompact) {
    s2shapeutil::CompactEncodeTaggedShapes(*shape_index_, encoder);
  } else {
    s2shapeutil::FastEncodeTaggedShapes(*shape_index_, encoder);
  }

  shape_index_->Encode(encoder);
}

void EncodedShapeIndexGeography::Encode(Encoder* encoder,
                                        const EncodeOptions& options) const {
  throw Exception("Encode() not implemented for EncodedShapeIndexGeography()");
}

void EncodedShapeIndexGeography::Decode(Decoder* decoder,
                                        const EncodeOptions& options) {
  auto new_index = absl::make_unique<EncodedS2ShapeIndex>();
  S2Error error;

#if defined(S2_TAGGED_SHAPE_FACTORY_WITH_ERROR)
  shape_factory_ = absl::make_unique<s2shapeutil::TaggedShapeFactory>(
      s2shapeutil::LazyDecodeShape, decoder, error);
#else
  shape_factory_ = absl::make_unique<s2shapeutil::TaggedShapeFactory>(
      s2shapeutil::LazyDecodeShape, decoder);
#endif

  bool success = new_index->Init(decoder, *shape_factory_);
  if (!success || !error.ok()) {
    throw Exception("EncodedShapeIndexGeography decoding error: " +
                    error.text());
  }

  shape_index_ = std::move(new_index);
}

void Geography::EncodeTagged(Encoder* encoder,
                             const EncodeOptions& options) const {
  encoder->Ensure(sizeof(uint16_t) + sizeof(uint16_t));
  encoder->put16(static_cast<uint16_t>(kind()));
  encoder->put16(options.flags);
  Encode(encoder, options);
}

std::unique_ptr<Geography> Geography::DecodeTagged(Decoder* decoder) {
  if (decoder->avail() < (sizeof(uint16_t) + sizeof(uint16_t))) {
    throw Exception(
        "Geography::EncodeTagged(): insufficient tag bytes in decoder");
  }

  EncodeOptions options;
  uint16_t geography_type = decoder->get16();
  options.flags = decoder->get16();

  uint16_t flags_check =
      options.flags & ~EncodeOptions::kFlagCompact & ~EncodeOptions::kFlagLazy;
  if (flags_check != 0) {
    throw Exception("Geography::EncodeTagged(): unknown flag");
  }

  if (geography_type == static_cast<uint16_t>(GeographyKind::POINT)) {
    auto geog = std::make_unique<PointGeography>();
    geog->Decode(decoder, options);
    return geog;
  } else if (geography_type == static_cast<uint16_t>(GeographyKind::POLYLINE)) {
    auto geog = std::make_unique<PolylineGeography>();
    geog->Decode(decoder, options);
    return geog;
  } else if (geography_type == static_cast<uint16_t>(GeographyKind::POLYGON)) {
    auto geog = std::make_unique<PolygonGeography>();
    geog->Decode(decoder, options);
    return geog;
  } else if (geography_type ==
             static_cast<uint16_t>(GeographyKind::GEOGRAPHY_COLLECTION)) {
    auto geog = std::make_unique<GeographyCollection>();
    geog->Decode(decoder, options);
    return geog;
  } else if (geography_type ==
                 static_cast<uint16_t>(GeographyKind::SHAPE_INDEX) ||
             geography_type ==
                 static_cast<uint16_t>(GeographyKind::SHAPE_INDEX)) {
    auto geog = std::make_unique<EncodedShapeIndexGeography>();
    geog->Decode(decoder, options);
    return geog;
  } else {
    throw Exception(
        "Geography::DecodeTagged(): Unknown geography type identifier " +
        std::to_string(geography_type));
  }
}
