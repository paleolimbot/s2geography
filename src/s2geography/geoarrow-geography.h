#pragma once

#include <s2/mutable_s2shape_index.h>
#include <s2/s2shape.h>
#include <s2/s2shape_index.h>

#include <algorithm>
#include <vector>

#include "geoarrow/geoarrow.hpp"
#include "s2/util/coding/coder.h"
#include "s2geography/geography_interface.h"

namespace s2geography {

/// \defgroup geoarrow-shapes GeoArrow S2Shape implementations
/// \brief S2Shape implementations backed by GeoArrowGeometryView
///
/// These classes provide lightweight S2Shape wrappers around
/// GeoArrowGeometryView, allowing direct use of GeoArrow-encoded geometry
/// (e.g., WKB buffers) in S2 spatial indexes without copying vertex data
/// into S2-native types. Each shape is only valid for the lifetime of the
/// underlying data pointed to by the wrapped GeoArrowGeometryView.
///
/// These structures are intended to be reused: the `Init()` method can be
/// called again once any derivative data structure is no longer needed. Using
/// this pattern should effectively reuse internal scratch space allocated
/// during initialization.
///
/// @{

/// \brief Point S2Shape implementation backed by a GeoArrowGeometryView
///
/// This shape represents zero or more points and can be initialized
/// from a POINT or MULTPOINT. It is based on the S2PointVectorShape.
/// The shape is only valid for the lifetime of the data pointed to
/// by the wrapped GeoArrowGeometryView (e.g., the WKB buffer containing
/// points).
class GeoArrowPointShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48492;

  /// \brief Create an empty point shape representing zero points
  GeoArrowPointShape() = default;

  /// \brief Convenience constructor that initializes a point shape
  ///
  /// Throws if geom neither a POINT nor a MULTIPOINT, or is a MULTIPOINT
  /// that contains EMPTY children.
  explicit GeoArrowPointShape(struct GeoArrowGeometryView geom);

  /// \brief Reset internal state such that this shape represents zero edges
  void Clear();

  /// \brief (Re)Initialize an existing shape
  ///
  /// Throws if geom neither a POINT nor a MULTIPOINT, or is a MULTIPOINT
  /// that contains EMPTY children.
  void Init(struct GeoArrowGeometryView geom);

  int num_vertices() const;
  S2Point vertex(int v) const;

  int num_edges() const override;
  Edge edge(int e) const override;
  int dimension() const override;
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const override;
  Chain chain(int i) const override;
  Edge chain_edge(int i, int j) const override;
  ChainPosition chain_position(int e) const override;
  TypeTag type_tag() const override;

 private:
  struct GeoArrowGeometryView geom_{};
};

class GeoArrowPointShapeIndex : public S2ShapeIndex {
 public:
  ~GeoArrowPointShapeIndex() override {
    for (auto* cell : index_cells_) delete cell;
  }

  void Init(GeoArrowPointShape* shape) {
    cells_.resize(shape->num_edges());
    int i = 0;

    // Visit nodes

    shape_ = shape;
  }

  void Build() {
    std::sort(cells_.begin(), cells_.end());

    for (auto* cell : index_cells_) delete cell;
    cell_ids_.clear();
    index_cells_.clear();

    size_t i = 0;
    while (i < cells_.size()) {
      S2CellId cellid = cells_[i].first;
      cell_ids_.push_back(cellid);

      // Collect sorted edge ids for this cell
      size_t start = i;
      while (i < cells_.size() && cells_[i].first == cellid) ++i;
      int n = static_cast<int>(i - start);

      // Encode the cell (num_shape_ids==1, contains_center=false)
      Encoder encoder;
      encoder.Ensure(4 + n * 5);  // conservative
      if (n == 1) {
        encoder.put_varint64(static_cast<uint64>(cells_[start].second) << 3 |
                             1);
      } else {
        encoder.put_varint64(static_cast<uint64>(n) << 3 | 3);
        // EncodeEdges: delta-encoded with run-length
        int edge_id_base = 0;
        for (size_t j = start; j < start + n;) {
          int edge_id = cells_[j].second;
          int delta = edge_id - edge_id_base;
          if (j + 1 == start + n) {
            encoder.put_varint32(delta);
            ++j;
          } else {
            int count = 1;
            while (j + count < start + n &&
                   cells_[j + count].second == edge_id + count)
              ++count;
            if (count < 8) {
              encoder.put_varint32(delta << 3 | (count - 1));
            } else {
              encoder.put_varint32((count - 8) << 3 | 7);
              encoder.put_varint32(delta);
            }
            j += count;
            edge_id_base = edge_id + count;
          }
        }
      }

      auto* cell = new S2ShapeIndexCell;
      Decoder decoder(encoder.base(), encoder.length());
      cell->Decode(1, &decoder);
      index_cells_.push_back(cell);
    }
  }

  int num_shape_ids() const override { return 1; }
  S2Shape* shape(int id) const override { return shape_; }
  size_t SpaceUsed() const override {
    return sizeof(*this) + sizeof(S2CellId) * cell_ids_.capacity() +
           sizeof(S2ShapeIndexCell*) * index_cells_.capacity() +
           sizeof(std::pair<S2CellId, int>) * cells_.capacity();
  }
  void Minimize() override {}

  class Iterator final : public IteratorBase {
   public:
    Iterator() = default;

    explicit Iterator(const GeoArrowPointShapeIndex* index,
                      InitialPosition pos = UNPOSITIONED)
        : index_(index), pos_(0) {
      if (pos == BEGIN) {
        Begin();
      } else {
        Finish();
      }
    }

    void Begin() override {
      pos_ = 0;
      Refresh();
    }

    void Finish() override {
      pos_ = index_->cell_ids_.size();
      set_finished();
    }

    void Next() override {
      ++pos_;
      Refresh();
    }

    bool Prev() override {
      if (pos_ == 0) return false;
      --pos_;
      Refresh();
      return true;
    }

    void Seek(S2CellId target) override {
      pos_ = std::lower_bound(index_->cell_ids_.begin(),
                              index_->cell_ids_.end(), target) -
             index_->cell_ids_.begin();
      Refresh();
    }

    bool Locate(const S2Point& target) override {
      return LocateImpl(*this, target);
    }

    S2CellRelation Locate(S2CellId target) override {
      return LocateImpl(*this, target);
    }

   protected:
    const S2ShapeIndexCell* GetCell() const override {
      return index_->index_cells_[pos_];
    }

    std::unique_ptr<IteratorBase> Clone() const override {
      return std::make_unique<Iterator>(*this);
    }

    void Copy(const IteratorBase& other) override {
      *this = static_cast<const Iterator&>(other);
    }

   private:
    void Refresh() {
      if (pos_ >= index_->cell_ids_.size()) {
        set_finished();
      } else {
        set_state(index_->cell_ids_[pos_], index_->index_cells_[pos_]);
      }
    }

    const GeoArrowPointShapeIndex* index_ = nullptr;
    size_t pos_ = 0;
  };

 protected:
  std::unique_ptr<IteratorBase> NewIterator(
      InitialPosition pos) const override {
    return std::make_unique<Iterator>(this, pos);
  }

 private:
  GeoArrowPointShape* shape_ = nullptr;
  std::vector<std::pair<S2CellId, int>> cells_;
  std::vector<S2CellId> cell_ids_;
  std::vector<S2ShapeIndexCell*> index_cells_;

  friend class GeoArrowPointShape;
};

/// \brief Linestring S2Shape implementation backed by a GeoArrowGeometryView
///
/// This shape represents zero or more linestrings and can be initialized
/// from a LINESTRING or MULTILINESTRING. It is based on the S2LaxPolylineShape.
/// The shape is only valid for the lifetime of the data pointed to
/// by the wrapped GeoArrowGeometryView (e.g., the WKB buffer containing
/// LINESTRING/MULTILINESTRING geometries).
class GeoArrowLaxPolylineShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48493;

  /// \brief Create an empty polyline shape containing zero polylines
  GeoArrowLaxPolylineShape() { num_edges_.push_back(0); }

  /// \brief Convenience constructor that initializes a linestring shape
  ///
  /// Throws if geom neither a LINESTRING nor a MULTILINESTRING.
  explicit GeoArrowLaxPolylineShape(struct GeoArrowGeometryView geom);

  /// \brief Reset internal state such that this shape represents zero edges
  void Clear();

  /// \brief (Re)initialize an existing linestring shape
  ///
  /// Throws if geom neither a LINESTRING nor a MULTILINESTRING.
  void Init(struct GeoArrowGeometryView geom);

  int num_edges() const override;
  Edge edge(int e) const override;
  int dimension() const override;
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const override;
  Chain chain(int i) const override;
  Edge chain_edge(int i, int j) const override;
  ChainPosition chain_position(int e) const override;
  TypeTag type_tag() const override;

 private:
  struct GeoArrowGeometryView geom_{};
  int num_chains_{};
  std::vector<int> num_edges_;
};

/// \brief Polygon S2Shape implementation backed by a GeoArrowGeometryView
///
/// This shape represents zero or more polygons and can be initialized
/// from a POLYGON or MULTIPOLYGON. It is based on the S2LaxPolygonShape.
/// The shape is only valid for the lifetime of the data pointed to
/// by the wrapped GeoArrowGeometryView (e.g., the WKB buffer containing
/// polygons).
///
/// Like the S2LaxPolygonShape, this class depends on the rings being
/// oriented. Use NormalizeOrientation() after Init() for input where the
/// winding order might be invalid; this ensures that "shell"s are oriented
/// counterclockwise and "holes" are oriented clockwise. This check may be
/// expensive.
///
/// Note that S2's internal representation of a polygon is substantially
/// different than the simple features idiom. Briefly, polygons are composed
/// of zero or more "loops" whose vertex order defines which points are
/// contained by the loop. The NormalizeOrientation() helper is intended to
/// bridge these two idioms.
class GeoArrowLaxPolygonShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48494;

  /// \brief Create an empty polygon shape containing zero loops
  GeoArrowLaxPolygonShape() { num_edges_.push_back(0); }

  /// \brief Convenience constructor that initializes a polygon shape
  ///
  /// Throws if geometry is not a POLYGON or MULTIPOLYGON.
  explicit GeoArrowLaxPolygonShape(struct GeoArrowGeometryView geom);

  /// \brief Reset internal state such that this shape represents zero edges
  void Clear();

  /// \brief (Re)initialize a polygon shape
  ///
  /// Throws if geometry is not a POLYGON or MULTIPOLYGON.
  void Init(struct GeoArrowGeometryView geom);

  /// \brief Update loop orientations to ensure that "shell"s are wound
  /// counterclockwise and "hole"s are wound clockwise.
  ///
  /// The "shell" or "hole" of each loop is derived from its position
  /// within each input POLYGON, where the first child ring is considered a
  /// shell and subsequent child rings are considered holes.
  ///
  /// This does not modify the input geometry nodes nor does it force a copy
  /// of the data (but may force a copy of the entire GeoArrowGeometryNode
  /// array).
  void NormalizeOrientation();

  int num_edges() const override;
  Edge edge(int e) const override;
  int dimension() const override;
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const override;
  Chain chain(int i) const override;
  Edge chain_edge(int i, int j) const override;
  ChainPosition chain_position(int e) const override;
  TypeTag type_tag() const override;

 private:
  struct GeoArrowGeometryView geom_{};
  int num_loops_{};
  // Cumulative edge counts per loop: num_edges_[0] = 0,
  // num_edges_[i+1] = total edges in loops 0..i
  std::vector<int> num_edges_;
  // Owned loops for O(1) lookup
  std::vector<struct GeoArrowGeometryNode> loops_;
  std::vector<S2Point> point_scratch_;
};

class GeoArrowGeography : public Geography {
 public:
  GeoArrowGeography() : Geography(GeographyKind::GEOARROW) {}

  GeoArrowGeography(const GeoArrowGeography&) = delete;
  GeoArrowGeography& operator=(const GeoArrowGeography&) = delete;
  GeoArrowGeography(GeoArrowGeography&& other);

  GeoArrowGeography& operator=(GeoArrowGeography&& other);

  void Init(struct GeoArrowGeometryView geom);
  void InitOriented(struct GeoArrowGeometryView geom);

  const S2ShapeIndex& ShapeIndex() const;

  int dimension() const override;
  int num_shapes() const override;
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;
  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  struct GeoArrowGeometryView geom_{};
  GeoArrowPointShape points_;
  GeoArrowLaxPolylineShape lines_;
  GeoArrowLaxPolygonShape polygons_;
  MutableS2ShapeIndex index_;
  GeoArrowPointShapeIndex point_index_;

  void AddShapesToIndex();
};

/// @}

}  // namespace s2geography
