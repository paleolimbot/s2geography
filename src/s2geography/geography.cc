
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

#include "s2geography/geography_interface.h"
#include "s2geography/macros.h"

using namespace s2geography;

void Geography::GetCellUnionBound(std::vector<S2CellId>* cell_ids) const {
  MutableS2ShapeIndex index;
  for (int i = 0; i < num_shapes(); i++) {
    index.Add(Shape(i));
  }

  MakeS2ShapeIndexRegion<MutableS2ShapeIndex>(&index).GetCellUnionBound(
      cell_ids);
}

std::unique_ptr<S2Shape> PointGeography::Shape(int id) const {
  S2GEOGRAPHY_UNUSED(id);
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

// ---- Encode/Decode implementations ----

void PointGeography::EncodeTagged(Encoder* encoder,
                                  const EncodeOptions& options) const {
  // Special case encoding for exactly one point in compact mode
  if (points_.size() != 1 ||
      options.coding_hint() != s2coding::CodingHint::COMPACT) {
    Geography::EncodeTagged(encoder, options);
    return;
  }

  int face;
  uint32_t si, ti;
  int level = S2::XYZtoFaceSiTi(points_[0], &face, &si, &ti);

  // Only encode this for very high levels: because the covering *is* the
  // representation, we will have a very loose covering if the level is low.
  // Level 23 has a cell size of ~1 meter
  // (http://s2geometry.io/resources/s2cell_statistics)
  if (level < 23) {
    // Not exactly encodable as a cell center
    Geography::EncodeTagged(encoder, options);
    return;
  }

  // For a cell center, the covering *is* the representation and there is
  // no additional encoding. If or when there is a true CellCenterGeography,
  // we would need to do something different if there are more than 256 points.
  EncodeTag tag;
  tag.kind = GeographyKind::CELL_CENTER;
  tag.covering_size = 1;
  tag.Encode(encoder);

  encoder->Ensure(sizeof(uint64_t));
  encoder->put64(S2CellId(points_[0]).id());
}

void PointGeography::Encode(Encoder* encoder,
                            const EncodeOptions& options) const {
  s2coding::EncodeS2PointVector(points_, options.coding_hint(), encoder);
}

void PointGeography::Decode(Decoder* decoder, const EncodeTag& tag) {
  if (tag.flags & EncodeTag::kFlagEmpty) {
    return;
  }

  // The snapped point encoding we currently route through the PointGeography
  // because we have some hard-coded dynamic_cast<>s for some s2_xxxx()
  // functions and introducing another subclass might cause unintended
  // consequences.
  if (tag.kind == GeographyKind::CELL_CENTER) {
    std::vector<S2CellId> cell_ids;
    tag.DecodeCovering(decoder, &cell_ids);
    points_.reserve(cell_ids.size());
    for (const auto cell_id : cell_ids) {
      points_.push_back(cell_id.ToPoint());
    }

    return;
  }

  // Otherwise, this was encoded using an EncodedS2PointVector
  tag.SkipCovering(decoder);
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

  for (const auto& polyline : polylines_) {
    polyline->Encode(encoder, options.coding_hint());
  }
}

void PolylineGeography::Decode(Decoder* decoder, const EncodeTag& tag) {
  if (tag.flags & EncodeTag::kFlagEmpty) {
    return;
  }

  tag.SkipCovering(decoder);

  if (decoder->avail() < sizeof(uint32_t)) {
    throw Exception(
        "PolylineGeography::Decode error: insufficient header bytes");
  }

  uint32_t n_polylines = decoder->get32();
  for (uint32_t i = 0; i < n_polylines; i++) {
    auto polyline = absl::make_unique<S2Polyline>();
    polyline->set_s2debug_override(S2Debug::DISABLE);
    if (!polyline->Decode(decoder)) {
      throw Exception("PolylineGeography::Decode error at item " +
                      std::to_string(i));
    }

    polylines_.push_back(std::move(polyline));
  }
}

void PolygonGeography::Encode(Encoder* encoder,
                              const EncodeOptions& options) const {
  polygon_->Encode(encoder, options.coding_hint());
}

void PolygonGeography::Decode(Decoder* decoder, const EncodeTag& tag) {
  if (tag.flags & EncodeTag::kFlagEmpty) {
    return;
  }

  tag.SkipCovering(decoder);
  polygon_->set_s2debug_override(S2Debug::DISABLE);
  polygon_->Decode(decoder);
}

void GeographyCollection::Encode(Encoder* encoder,
                                 const EncodeOptions& options) const {
  // Never include coverings for children (only a top-level concept)
  EncodeOptions child_options = options;
  child_options.set_include_covering(false);

  encoder->Ensure(sizeof(uint32_t));
  encoder->put32(static_cast<uint32_t>(features_.size()));
  for (const auto& feature : features_) {
    feature->EncodeTagged(encoder, options);
  }
}

void GeographyCollection::Decode(Decoder* decoder, const EncodeTag& tag) {
  if (tag.flags & EncodeTag::kFlagEmpty) {
    return;
  }

  tag.SkipCovering(decoder);
  uint32_t n_features = decoder->get32();
  for (uint32_t i = 0; i < n_features; i++) {
    features_.push_back(Geography::DecodeTagged(decoder));
  }

  CountShapes();
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
  if (options.enable_lazy_decode()) {
    if (options.coding_hint() == s2coding::CodingHint::FAST) {
      throw Exception("Lazy output only supported with the compact option");
    }

    s2coding::StringVectorEncoder shape_vector;
    for (const S2Shape* shape : *shape_index_) {
      Encoder* sub_encoder = shape_vector.AddViaEncoder();
      if (shape == nullptr) continue;  // Encode as zero bytes.

      sub_encoder->Ensure(Encoder::kVarintMax32);

      if (!CustomCompactTaggedShapeEncoder(*shape, sub_encoder)) {
        throw Exception("Error encoding shape");
      }
    }
    shape_vector.Encode(encoder);
  } else if (options.coding_hint() == s2coding::CodingHint::COMPACT) {
    s2shapeutil::CompactEncodeTaggedShapes(*shape_index_, encoder);
  } else {
    s2shapeutil::FastEncodeTaggedShapes(*shape_index_, encoder);
  }

  shape_index_->Encode(encoder);
}

void EncodedShapeIndexGeography::Encode(Encoder* encoder,
                                        const EncodeOptions& options) const {
  S2GEOGRAPHY_UNUSED(encoder);
  S2GEOGRAPHY_UNUSED(options);
  throw Exception("Encode() not implemented for EncodedShapeIndexGeography()");
}

void EncodedShapeIndexGeography::Decode(Decoder* decoder,
                                        const EncodeTag& tag) {
  if (tag.flags & EncodeTag::kFlagEmpty) {
    return;
  }

  tag.SkipCovering(decoder);

  // TaggedShapeFactory constructors are incompatible between 0.11 and 0.12
#if defined(S2_VERSION_MAJOR) && \
    (S2_VERSION_MAJOR == 0 && S2_VERSION_MINOR <= 11)
  shape_factory_ = absl::make_unique<s2shapeutil::TaggedShapeFactory>(
      s2shapeutil::LazyDecodeShape, decoder);
#else
  S2Error error;
  shape_factory_ = absl::make_unique<s2shapeutil::TaggedShapeFactory>(
      s2shapeutil::LazyDecodeShape, decoder, error);
  if (!error.ok()) {
    std::stringstream ss;
    ss << "EncodedShapeIndexGeography decoding error: " << error;
    throw Exception(ss.str());
  }
#endif

  auto new_index = absl::make_unique<EncodedS2ShapeIndex>();
  bool success = new_index->Init(decoder, *shape_factory_);
  if (!success) {
    throw Exception("EncodedShapeIndexGeography decoding error");
  }

  shape_index_ = std::move(new_index);
}

void Geography::EncodeTagged(Encoder* encoder,
                             const EncodeOptions& options) const {
  EncodeTag tag;
  std::vector<S2CellId> covering;
  tag.kind = kind();

  // For empty geographies, set the flag and don't call Encode()
  if (num_shapes() == 0) {
    tag.flags |= EncodeTag::kFlagEmpty;
    tag.Encode(encoder);
    return;
  }

  if (options.include_covering()) {
    // Get the union and normalize it. A normalized union is slightly more
    // expensive to compute but is faster to compare for possible intersection.

    GetCellUnionBound(&covering);
    S2CellUnion::Normalize(&covering);

    // The serialization format can't handle more than UINT8_MAX items
    // (geographies usually return ~4 cells from GetCellUnionBound()).
    if (covering.size() > 256) {
      covering.clear();
    }
  }

  // Encode the tag
  tag.covering_size = static_cast<uint8_t>(covering.size());
  tag.Encode(encoder);

  // Encode the covering (1 byte for the number of cells, cells as little endian
  // uint64_t).
  encoder->Ensure(covering.size() * sizeof(uint64_t));
  for (const auto cell_id : covering) {
    encoder->put64(cell_id.id());
  }

  // Encode the geography
  Encode(encoder, options);
}

std::unique_ptr<Geography> Geography::DecodeTagged(Decoder* decoder) {
  EncodeTag tag;
  tag.Decode(decoder);

  switch (tag.kind) {
    case GeographyKind::CELL_CENTER:
    case GeographyKind::POINT: {
      auto geog = std::make_unique<PointGeography>();
      geog->Decode(decoder, tag);
      return geog;
    }
    case GeographyKind::POLYLINE: {
      auto geog = std::make_unique<PolylineGeography>();
      geog->Decode(decoder, tag);
      return geog;
    }
    case GeographyKind::POLYGON: {
      auto geog = std::make_unique<PolygonGeography>();
      geog->Decode(decoder, tag);
      return geog;
    }
    case GeographyKind::GEOGRAPHY_COLLECTION: {
      auto geog = std::make_unique<GeographyCollection>();
      geog->Decode(decoder, tag);
      return geog;
    }
    case GeographyKind::SHAPE_INDEX: {
      auto geog = std::make_unique<EncodedShapeIndexGeography>();
      geog->Decode(decoder, tag);
      return geog;
    }
    default: {
      throw Exception("DecodeTagged(): kind not implemented");
    }
  }
}
