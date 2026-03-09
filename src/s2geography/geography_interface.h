
#pragma once

#include <s2/s2cap.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2region.h>
#include <s2/s2shape.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace s2geography {

/// \brief Base exception class thrown by S2Geography functions
class Exception : public std::runtime_error {
 public:
  Exception(std::string what) : std::runtime_error(what.c_str()) {}
};

// enum to tag concrete Geography implementations. Note that
// CELL_CENTER does not currently represent a concrete subclass
// but is used to mark a compact encoding method for small numbers of points.
enum class GeographyKind {
  UNINITIALIZED = 0,
  POINT = 1,
  POLYLINE = 2,
  POLYGON = 3,
  GEOGRAPHY_COLLECTION = 4,
  SHAPE_INDEX = 5,
  ENCODED_SHAPE_INDEX = 6,
  CELL_CENTER = 7,
  GEOARROW = 8,
};

class EncodeOptions;
struct EncodeTag;

/// \brief Abstract Geography base class
///
/// An Geography is an abstraction of S2 types that is designed to closely
/// match the scope of a GEOS Geometry. Its methods are limited to those needed
/// to implement C API functions. From an S2 perspective, an Geography is an
/// S2Region that can be represented by zero or more S2Shape objects. Current
/// implementations of Geography own their data (i.e., the coordinate vectors
/// and underlying S2 objects), however, the interface is designed to allow
/// future abstractions where this is not the case.
class Geography {
 public:
  explicit Geography(GeographyKind kind) : kind_(kind) {}
  virtual ~Geography() {}

  /// \brief Identify the concerete geometry implementation
  GeographyKind kind() const { return kind_; }

  /// \brief Identify the geometry dimension
  ///
  /// Returns 0, 1, or 2 if all Shape()s that are returned will have
  /// the same dimension (i.e., they are all points, all lines, or
  /// all polygons). Returns -1 if this geography contains mixed
  /// dimensions.
  virtual int dimension() const;

  /// \brief The number of S2Shape objects needed to represent this Geography
  virtual int num_shapes() const = 0;

  /// \brief Get an owning S2Shape
  ///
  /// Returns the given S2Shape (where 0 <= id < num_shapes()). The
  /// caller retains ownership of the S2Shape but the data pointed to
  /// by the object requires that the underlying Geography outlives
  /// the returned object.
  virtual std::unique_ptr<S2Shape> Shape(int id) const = 0;

  /// \brief Get an owning S2Region
  ///
  /// Returns an S2Region that represents the object. The caller retains
  /// ownership of the S2Region but the data pointed to by the object
  /// requires that the underlying Geography outlives the returned
  /// object.
  virtual std::unique_ptr<S2Region> Region() const = 0;

  /// \brief Add an unnormalized set of S2CellIds that cover this geography
  ///
  /// Adds an unnormalized set of S2CellIDs to `cell_ids`. This is intended
  /// to be faster than using Region().GetCovering() directly and to
  /// return a small number of cells that can be used to compute a possible
  /// intersection quickly.
  virtual void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const;

  /// \brief Serialize this geography
  ///
  /// Serialize this geography to an encoder. This does not include any
  /// encapsulating information (e.g., which geography type or flags).
  virtual void Encode(Encoder* encoder, const EncodeOptions& options) const = 0;

  /// \brief Serialized this geography with identifying information
  ///
  /// Serialize this geography to an encoder such that it can roundtrip
  /// with DecodeTagged(). EXPERIMENTAL.
  virtual void EncodeTagged(Encoder* encoder,
                            const EncodeOptions& options) const;

  /// \brief Create a geography from output written with EncodeTagged.
  /// EXPERIMENTAL.
  static std::unique_ptr<Geography> DecodeTagged(Decoder* decoder);

 protected:
  // Helper for subclasses to write a covering. Subclasses must call this
  // or encode their own covering when implementing Encode().
  void EncodeCoveringDefault(Encoder* encoder);

 private:
  GeographyKind kind_;
};

/// \brief Options for serializing geographies using Geography::EncodeTagged()
class EncodeOptions {
 public:
  /// \brief Default options
  ///
  /// Create options with default values, which optimize for the
  /// scenario where a geography is about to be fully deserialized
  /// in another process. Set the appropriate options for smaller
  /// encoded size and/or better query performance when running queries
  /// directly on encoded data.
  EncodeOptions() = default;

  /// \brief Coding hint
  ///
  /// Control whether to optimize for speed (by writing vertices as
  /// doubles) or space (by writing cell identifiers for vertices that
  /// are snapped to a cell center). For vertices that are snapped to a
  /// cell center at a lower zoom level, the encoder can encode each
  /// vertex with 4 or fewer bytes.
  void set_coding_hint(s2coding::CodingHint hint) { hint_ = hint; }
  s2coding::CodingHint coding_hint() const { return hint_; }

  /// \brief Lazy decode
  ///
  /// Control whether to spend extra effort converting shapes that
  /// aren't able to be lazily decoded (e.g., S2Polyline::Shape and
  /// S2Polygon::Shape to S2LaxPolylineShape and S2LaxPolygonShape,
  /// respectively).
  void set_enable_lazy_decode(bool enable_lazy_decode) {
    enable_lazy_decode_ = enable_lazy_decode;
  }
  bool enable_lazy_decode() const { return enable_lazy_decode_; }

  /// \brief Embedded covering
  ///
  /// Control whether to prefix the serialized geography with a covering
  /// to more rapidy check for possible intersection. The covering that is
  /// written is currently the normalized result of GetCellUnionBound().
  void set_include_covering(bool include_covering) {
    include_covering_ = include_covering;
  }
  bool include_covering() const { return include_covering_; }

 private:
  s2coding::CodingHint hint_{s2coding::CodingHint::FAST};
  bool enable_lazy_decode_{false};
  bool include_covering_{false};
};

/// \brief Encoded data prefix
///
/// A 4 byte prefix for encoded geographies. 4 bytes is essential so that
/// German-style strings store these bytes in their prefix (i.e., don't have
/// to load any auxiliary buffers to inspect this information).
struct EncodeTag {
  /// \brief Geography subclass whose Decode() method will be called. Encoded as
  /// a uint8_t.
  GeographyKind kind{GeographyKind::UNINITIALIZED};

  /// \brief Flags
  ///
  /// Currently supported are kFlagEmpty (set if and only if there are zero
  /// shapes in the geography).
  uint8_t flags{};

  /// \brief Embedded covering size
  ///
  /// Number of cells identifiers that follow this tag. Note that zero cells
  /// (i.e., an empty covering) indicates that no covering was written and does
  /// NOT imply an empty geography.
  uint8_t covering_size{};

  /// \brief Reserved byte
  ///
  /// Reserved byte (must be 0)
  uint8_t reserved{};

  void Encode(Encoder* encoder) const;
  void Decode(Decoder* decoder);
  void DecodeCovering(Decoder* decoder, std::vector<S2CellId>* cell_ids) const;
  void SkipCovering(Decoder* decoder) const;
  void Validate();

  static constexpr uint8_t kFlagEmpty = 1;
};

/// \brief Non-owning wrapper around an S2Shape
///
/// This class is a shim to allow a class to return a
/// std::unique_ptr<S2Shape>(), which is required by MutableS2ShapeIndex::Add(),
/// without copying the underlying data. S2Shape instances do not typically own
/// their data (e.g., S2Polygon::Shape), so this does not change the general
/// relationship (that anything returned by Geography::Shape() is only valid
/// within the scope of the Geography). Note that this class is also available
/// (but not exposed) in s2/s2shapeutil_coding.cc.
class S2ShapeWrapper : public S2Shape {
 public:
  explicit S2ShapeWrapper(const S2Shape* shape) : shape_(shape) {}

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

/// \brief Non-owning wrapper around an S2Region
///
/// Just like the S2ShapeWrapper, the S2RegionWrapper helps reconcile the
/// differences in lifecycle expectation between S2 and Geography. We often
/// need access to a S2Region to generalize algorithms; however, there are some
/// operations that need ownership of the region (e.g., the S2RegionUnion). In
/// Geography the assumption is that anything returned by a Geography is only
/// valid for the lifetime of the underlying Geography. A different design of
/// the algorithms implemented here might well make this unnecessary.
class S2RegionWrapper : public S2Region {
 public:
  explicit S2RegionWrapper(S2Region* region) : region_(region) {}

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

}  // namespace s2geography
