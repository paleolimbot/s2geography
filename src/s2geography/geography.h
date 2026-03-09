
#pragma once

#include <s2/s2cell_id.h>
#include <s2/s2latlng.h>
#include <s2/s2point.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>
#include <s2/s2region.h>
#include <s2/s2shape.h>
#include <s2/s2shape_index.h>

#include <vector>

#include "s2geography/geography_interface.h"

namespace s2geography {

/// \brief S2Point Geography implementation
///
/// An Geography representing zero or more points using a std::vector<S2Point>
/// as the underlying representation.
class PointGeography : public Geography {
 public:
  PointGeography() : Geography(GeographyKind::POINT) {}
  explicit PointGeography(S2Point point) : Geography(GeographyKind::POINT) {
    points_.push_back(point);
  }
  explicit PointGeography(std::vector<S2Point> points)
      : Geography(GeographyKind::POINT), points_(std::move(points)) {}

  PointGeography(const PointGeography&) = delete;
  PointGeography& operator=(const PointGeography&) = delete;
  PointGeography(PointGeography&&) = default;
  PointGeography& operator=(PointGeography&&) = default;

  const std::vector<S2Point>& Points() const { return points_; }

  void Decode(Decoder* decoder, const EncodeTag& tag);

  int dimension() const override { return 0; }
  int num_shapes() const override {
    if (points_.empty()) {
      return 0;
    } else {
      return 1;
    }
  }
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const override;
  void EncodeTagged(Encoder* encoder,
                    const EncodeOptions& options) const override;
  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  std::vector<S2Point> points_;
};

/// \brief S2Polyline Geography implementation
///
/// An Geography representing zero or more polylines using the S2Polyline class
/// as the underlying representation.
class PolylineGeography : public Geography {
 public:
  PolylineGeography() : Geography(GeographyKind::POLYLINE) {}
  explicit PolylineGeography(std::unique_ptr<S2Polyline> polyline)
      : Geography(GeographyKind::POLYLINE) {
    polylines_.push_back(std::move(polyline));
  }
  explicit PolylineGeography(std::vector<std::unique_ptr<S2Polyline>> polylines)
      : Geography(GeographyKind::POLYLINE), polylines_(std::move(polylines)) {}

  PolylineGeography(const PolylineGeography&) = delete;
  PolylineGeography& operator=(const PolylineGeography&) = delete;
  PolylineGeography(PolylineGeography&&) = default;
  PolylineGeography& operator=(PolylineGeography&&) = default;

  void Decode(Decoder* decoder, const EncodeTag& tag);

  const std::vector<std::unique_ptr<S2Polyline>>& Polylines() const {
    return polylines_;
  }

  int dimension() const override { return 1; }
  int num_shapes() const override;
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const override;
  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  std::vector<std::unique_ptr<S2Polyline>> polylines_;
};

/// \brief S2Polygon Geography implementation
///
/// An Geography representing zero or more polygons using the S2Polygon class
/// as the underlying representation. Note that a single S2Polygon (from the S2
/// perspective) can represent zero or more polygons (from the simple features
/// perspective).
class PolygonGeography : public Geography {
 public:
  PolygonGeography();
  explicit PolygonGeography(std::unique_ptr<S2Polygon> polygon)
      : Geography(GeographyKind::POLYGON), polygon_(std::move(polygon)) {}

  PolygonGeography(const PolygonGeography&) = delete;
  PolygonGeography& operator=(const PolygonGeography&) = delete;
  PolygonGeography(PolygonGeography&&) = default;
  PolygonGeography& operator=(PolygonGeography&&) = default;

  void Decode(Decoder* decoder, const EncodeTag& tag);

  const std::unique_ptr<S2Polygon>& Polygon() const { return polygon_; }

  int dimension() const override { return 2; }
  int num_shapes() const override;
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const override;
  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  std::unique_ptr<S2Polygon> polygon_;
};

/// \brief Collection of arbitrary Geographies
///
/// An Geography wrapping zero or more Geography objects. These objects
/// can be used to represent a simple features GEOMETRYCOLLECTION.
class GeographyCollection : public Geography {
 public:
  GeographyCollection()
      : Geography(GeographyKind::GEOGRAPHY_COLLECTION), total_shapes_(0) {}

  GeographyCollection(std::vector<std::unique_ptr<Geography>> features)
      : Geography(GeographyKind::GEOGRAPHY_COLLECTION),
        features_(std::move(features)),
        total_shapes_(0) {
    CountShapes();
  }

  GeographyCollection(const GeographyCollection&) = delete;
  GeographyCollection& operator=(const GeographyCollection&) = delete;
  GeographyCollection(GeographyCollection&&) = default;
  GeographyCollection& operator=(GeographyCollection&&) = default;

  void Decode(Decoder* decoder, const EncodeTag& tag);

  const std::vector<std::unique_ptr<Geography>>& Features() const {
    return features_;
  }

  int num_shapes() const override;
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;
  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  std::vector<std::unique_ptr<Geography>> features_;
  std::vector<int> num_shapes_;
  int total_shapes_;

  inline void CountShapes() {
    for (const auto& feature : features_) {
      num_shapes_.push_back(feature->num_shapes());
      total_shapes_ += feature->num_shapes();
    }
  }
};

/// \brief MutableS2ShapeIndex Geoography
///
/// A Geography with a MutableS2ShapeIndex as the underlying data.
/// These are used as inputs for operations that are implemented in S2
/// using the S2ShapeIndex (e.g., boolean operations). If an Geography
/// instance will be used repeatedly, it will be faster to construct
/// one ShapeIndexGeography and use it repeatedly. This class does not
/// own any Geography objects that are added do it and thus is only
/// valid for the scope of those objects.
class ShapeIndexGeography : public Geography {
 public:
  ShapeIndexGeography();
  ShapeIndexGeography(int max_edges_per_cell);

  explicit ShapeIndexGeography(const Geography& geog);

  ShapeIndexGeography(const ShapeIndexGeography&) = delete;
  ShapeIndexGeography& operator=(const ShapeIndexGeography&) = delete;
  ShapeIndexGeography(ShapeIndexGeography&&) = default;
  ShapeIndexGeography& operator=(ShapeIndexGeography&&) = default;

  /// \brief Add a Geography to the index
  ///
  /// Returns the last shape_id that was added to the index or -1 if no shapes
  /// were added to the index.
  int Add(const Geography& geog);

  const S2ShapeIndex& ShapeIndex() const { return *shape_index_; }

  int num_shapes() const override;
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;

  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  std::unique_ptr<MutableS2ShapeIndex> shape_index_;
};

/// \brief EncodedS2ShapeIndex Geoography
///
/// A Geography with a EncodedS2ShapeIndex as the underlying data.
/// This is to facilitate decoding, whereas a MutableS2ShapeIndex is
/// used to construct the S2ShapeIndex required for many S2 operations.
class EncodedShapeIndexGeography : public Geography {
 public:
  EncodedShapeIndexGeography();

  EncodedShapeIndexGeography(const EncodedShapeIndexGeography&) = delete;
  EncodedShapeIndexGeography& operator=(const EncodedShapeIndexGeography&) =
      delete;
  EncodedShapeIndexGeography(EncodedShapeIndexGeography&&) = default;
  EncodedShapeIndexGeography& operator=(EncodedShapeIndexGeography&&) = default;

  void Decode(Decoder* decoder, const EncodeTag& tag);

  const S2ShapeIndex& ShapeIndex() const { return *shape_index_; }

  int num_shapes() const override;
  std::unique_ptr<S2Shape> Shape(int id) const override;
  std::unique_ptr<S2Region> Region() const override;
  void Encode(Encoder* encoder, const EncodeOptions& options) const override;

 private:
  std::unique_ptr<S2ShapeIndex> shape_index_;
  std::unique_ptr<S2ShapeIndex::ShapeFactory> shape_factory_;
};

}  // namespace s2geography
