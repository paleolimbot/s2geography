
#pragma once

#include <s2/s2cell_id.h>
#include <s2/s2latlng.h>
#include <s2/s2point.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>
#include <s2/s2region.h>
#include <s2/s2shape.h>
#include <s2/s2shape_index.h>
#include <stdint.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace s2geography {

class Exception : public std::runtime_error {
 public:
  Exception(std::string what) : std::runtime_error(what.c_str()) {}
};

// enum to tag concrete Geography implementations
enum class GeographyKind {
  POINT = 1,
  POLYLINE = 2,
  POLYGON = 3,
  GEOGRAPHY_COLLECTION = 4,
  SHAPE_INDEX = 5,
  ENCODED_SHAPE_INDEX = 6,
  OTHER = 9999
};

struct EncodeOptions {
  static constexpr uint16_t kFlagCompact = 1;
  static constexpr uint16_t kFlagLazy = 2;
  uint16_t flags{kFlagCompact | kFlagLazy};
};

// An Geography is an abstraction of S2 types that is designed to closely
// match the scope of a GEOS Geometry. Its methods are limited to those needed
// to implement C API functions. From an S2 perspective, an Geography is an
// S2Region that can be represented by zero or more S2Shape objects. Current
// implementations of Geography own their data (i.e., the coordinate vectors
// and underlying S2 objects), however, the interface is designed to allow
// future abstractions where this is not the case.
class Geography {
 public:
  Geography(GeographyKind kind) : kind_(kind) {}
  virtual ~Geography() {}

  GeographyKind kind() const { return kind_; }

  // Returns 0, 1, or 2 if all Shape()s that are returned will have
  // the same dimension (i.e., they are all points, all lines, or
  // all polygons).
  virtual int dimension() const {
    if (num_shapes() == 0) {
      return -1;
    }

    int dim = Shape(0)->dimension();
    for (int i = 1; i < num_shapes(); i++) {
      if (dim != Shape(i)->dimension()) {
        return -1;
      }
    }

    return dim;
  }

  // The number of S2Shape objects needed to represent this Geography
  virtual int num_shapes() const = 0;

  // Returns the given S2Shape (where 0 <= id < num_shapes()). The
  // caller retains ownership of the S2Shape but the data pointed to
  // by the object requires that the underlying Geography outlives
  // the returned object.
  virtual std::unique_ptr<S2Shape> Shape(int id) const = 0;

  // Returns an S2Region that represents the object. The caller retains
  // ownership of the S2Region but the data pointed to by the object
  // requires that the underlying Geography outlives the returned
  // object.
  virtual std::unique_ptr<S2Region> Region() const = 0;

  // Adds an unnormalized set of S2CellIDs to `cell_ids`. This is intended
  // to be faster than using Region().GetCovering() directly and to
  // return a small number of cells that can be used to compute a possible
  // intersection quickly.
  virtual void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const;

  // Serialize this geography to an encoder. This does not include any
  // encapsulating information (e.g., which geography type or flags).
  virtual void Encode(Encoder* encoder, const EncodeOptions& options) const = 0;

  // Serialize this geography to an encoder such that it can roundtrip
  // with DecodeTagged(). EXPERIMENTAL.
  void EncodeTagged(Encoder* encoder,
                    const EncodeOptions& options = EncodeOptions()) const;

  // Create a geography from output written with EncodeTagged. EXPERIMENTAL.
  static std::unique_ptr<Geography> DecodeTagged(Decoder* decoder);

 private:
  GeographyKind kind_;
};

// An Geography representing zero or more points using a std::vector<S2Point>
// as the underlying representation.
class PointGeography : public Geography {
 public:
  PointGeography() : Geography(GeographyKind::POINT) {}
  PointGeography(S2Point point) : Geography(GeographyKind::POINT) {
    points_.push_back(point);
  }
  PointGeography(std::vector<S2Point> points)
      : Geography(GeographyKind::POINT), points_(std::move(points)) {}

  int dimension() const { return 0; }
  int num_shapes() const {
    if (points_.empty()) {
      return 0;
    } else {
      return 1;
    }
  }
  std::unique_ptr<S2Shape> Shape(int id) const;
  std::unique_ptr<S2Region> Region() const;
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const;

  const std::vector<S2Point>& Points() const { return points_; }

  void Encode(Encoder* encoder, const EncodeOptions& options) const;

  void Decode(Decoder* decoder, const EncodeOptions& options);

 private:
  std::vector<S2Point> points_;
};

// An Geography representing zero or more polylines using the S2Polyline class
// as the underlying representation.
class PolylineGeography : public Geography {
 public:
  PolylineGeography() : Geography(GeographyKind::POLYLINE) {}
  PolylineGeography(std::unique_ptr<S2Polyline> polyline)
      : Geography(GeographyKind::POLYLINE) {
    polylines_.push_back(std::move(polyline));
  }
  PolylineGeography(std::vector<std::unique_ptr<S2Polyline>> polylines)
      : Geography(GeographyKind::POLYLINE), polylines_(std::move(polylines)) {}

  int dimension() const { return 1; }
  int num_shapes() const;
  std::unique_ptr<S2Shape> Shape(int id) const;
  std::unique_ptr<S2Region> Region() const;
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const;

  const std::vector<std::unique_ptr<S2Polyline>>& Polylines() const {
    return polylines_;
  }

  void Encode(Encoder* encoder, const EncodeOptions& options) const;

  void Decode(Decoder* decoder, const EncodeOptions& options);

 private:
  std::vector<std::unique_ptr<S2Polyline>> polylines_;
};

// An Geography representing zero or more polygons using the S2Polygon class
// as the underlying representation. Note that a single S2Polygon (from the S2
// perspective) can represent zero or more polygons (from the simple features
// perspective).
class PolygonGeography : public Geography {
 public:
  PolygonGeography();
  PolygonGeography(std::unique_ptr<S2Polygon> polygon)
      : Geography(GeographyKind::POLYGON), polygon_(std::move(polygon)) {}

  int dimension() const { return 2; }
  int num_shapes() const;
  std::unique_ptr<S2Shape> Shape(int id) const;
  std::unique_ptr<S2Region> Region() const;
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids) const;

  const std::unique_ptr<S2Polygon>& Polygon() const { return polygon_; }

  void Encode(Encoder* encoder, const EncodeOptions& options) const;

  void Decode(Decoder* decoder, const EncodeOptions& options);

 private:
  std::unique_ptr<S2Polygon> polygon_;
};

// An Geography wrapping zero or more Geography objects. These objects
// can be used to represent a simple features GEOMETRYCOLLECTION.
class GeographyCollection : public Geography {
 public:
  GeographyCollection()
      : Geography(GeographyKind::GEOGRAPHY_COLLECTION), total_shapes_(0) {}

  GeographyCollection(std::vector<std::unique_ptr<Geography>> features)
      : Geography(GeographyKind::GEOGRAPHY_COLLECTION),
        features_(std::move(features)),
        total_shapes_(0) {
    for (const auto& feature : features_) {
      num_shapes_.push_back(feature->num_shapes());
      total_shapes_ += feature->num_shapes();
    }
  }

  int num_shapes() const;
  std::unique_ptr<S2Shape> Shape(int id) const;
  std::unique_ptr<S2Region> Region() const;

  const std::vector<std::unique_ptr<Geography>>& Features() const {
    return features_;
  }

  void Encode(Encoder* encoder, const EncodeOptions& options) const;

  void Decode(Decoder* decoder, const EncodeOptions& options);

 private:
  std::vector<std::unique_ptr<Geography>> features_;
  std::vector<int> num_shapes_;
  int total_shapes_;
};

// A Geography with a MutableS2ShapeIndex as the underlying data.
// These are used as inputs for operations that are implemented in S2
// using the S2ShapeIndex (e.g., boolean operations). If an Geography
// instance will be used repeatedly, it will be faster to construct
// one ShapeIndexGeography and use it repeatedly. This class does not
// own any Geography objects that are added do it and thus is only
// valid for the scope of those objects.
class ShapeIndexGeography : public Geography {
 public:
  ShapeIndexGeography();
  ShapeIndexGeography(int max_edges_per_cell);

  explicit ShapeIndexGeography(const Geography& geog);

  // Add a Geography to the index, returning the last shape_id
  // that was added to the index or -1 if no shapes were added
  // to the index.
  int Add(const Geography& geog);

  int num_shapes() const;
  std::unique_ptr<S2Shape> Shape(int id) const;
  std::unique_ptr<S2Region> Region() const;

  const S2ShapeIndex& ShapeIndex() const { return *shape_index_; }

  void Encode(Encoder* encoder, const EncodeOptions& options) const;

 private:
  std::unique_ptr<MutableS2ShapeIndex> shape_index_;
};

// A Geography with a EncodedS2ShapeIndex as the underlying data.
// This is to facilitate decoding, whereas a MutableS2ShapeIndex is
// used to construct the S2ShapeIndex required for many S2 operations.
class EncodedShapeIndexGeography : public Geography {
 public:
  EncodedShapeIndexGeography();

  int num_shapes() const;
  std::unique_ptr<S2Shape> Shape(int id) const;
  std::unique_ptr<S2Region> Region() const;

  const S2ShapeIndex& ShapeIndex() const { return *shape_index_; }

  void Encode(Encoder* encoder, const EncodeOptions& options) const;

  void Decode(Decoder* decoder, const EncodeOptions& options);

 private:
  std::unique_ptr<S2ShapeIndex> shape_index_;
  std::unique_ptr<S2ShapeIndex::ShapeFactory> shape_factory_;
};

}  // namespace s2geography
