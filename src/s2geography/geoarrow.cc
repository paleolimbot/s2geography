
#include "geoarrow/geoarrow.h"

#include "s2/s1angle.h"
#include "s2/s2edge_tessellator.h"
#include "s2/s2projections.h"
#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

namespace s2geography {

namespace geoarrow {

const char* version() { return GeoArrowVersion(); }

class Constructor {
 public:
  Constructor(const ImportOptions& options) : options_(options) {
    if (options.projection() != nullptr) {
      this->tessellator_ = absl::make_unique<S2EdgeTessellator>(
          options.projection(), options.tessellate_tolerance());
    }
  }

  virtual ~Constructor() {}

  void InitVisitor(GeoArrowVisitor* v) {
    v->feat_start = &CFeatStart;
    v->feat_end = &CFeatEnd;
    v->geom_start = &CGeomStart;
    v->geom_end = &CGeomEnd;
    v->ring_start = &CRingStart;
    v->ring_end = &CRingEnd;
    v->coords = &CCoords;
    v->private_data = this;
  }

  virtual GeoArrowErrorCode feat_start() { return GEOARROW_OK; }
  virtual GeoArrowErrorCode null_feat() { return GEOARROW_OK; }
  virtual GeoArrowErrorCode geom_start(GeoArrowGeometryType geometry_type,
                                       int64_t size) {
    return GEOARROW_OK;
  }
  virtual GeoArrowErrorCode ring_start(int64_t size) { return GEOARROW_OK; }
  virtual GeoArrowErrorCode ring_end() { return GEOARROW_OK; }
  virtual GeoArrowErrorCode geom_end() { return GEOARROW_OK; }
  virtual GeoArrowErrorCode feat_end() { return GEOARROW_OK; }

  virtual GeoArrowErrorCode coords(const GeoArrowCoordView* view) {
    int coord_size = view->n_values;
    int64_t n = view->n_coords;

    if (coord_size == 3) {
      for (int64_t i = 0; i < n; i++) {
        input_points_.push_back(S2Point(GEOARROW_COORD_VIEW_VALUE(view, i, 0),
                                        GEOARROW_COORD_VIEW_VALUE(view, i, 1),
                                        GEOARROW_COORD_VIEW_VALUE(view, i, 2)));
      }
    } else {
      for (int64_t i = 0; i < n; i++) {
        input_points_.push_back(S2Point(GEOARROW_COORD_VIEW_VALUE(view, i, 0),
                                        GEOARROW_COORD_VIEW_VALUE(view, i, 1),
                                        0));
      }
    }

    return GEOARROW_OK;
  }

  virtual std::unique_ptr<Geography> finish() = 0;

 protected:
  std::vector<S2Point> input_points_;
  std::vector<S2Point> points_;
  ImportOptions options_;
  std::unique_ptr<S2EdgeTessellator> tessellator_;

  void finish_points() {
    points_.clear();
    points_.reserve(input_points_.size());

    if (options_.projection() == nullptr) {
      for (const auto& pt : input_points_) {
        points_.push_back(pt);
      }
    } else if (options_.tessellate_tolerance() != S1Angle::Infinity()) {
      for (size_t i = 1; i < input_points_.size(); i++) {
        const S2Point& pt0(input_points_[i - 1]);
        const S2Point& pt1(input_points_[i]);
        tessellator_->AppendUnprojected(R2Point(pt0.x(), pt0.y()),
                                        R2Point(pt1.x(), pt1.y()), &points_);
      }
    } else {
      for (const auto& pt : input_points_) {
        points_.push_back(
            options_.projection()->Unproject(R2Point(pt.x(), pt.y())));
      }
    }

    input_points_.clear();
  }

 private:
  static int CFeatStart(GeoArrowVisitor* v) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->feat_start();
  }

  static int CFeatEnd(GeoArrowVisitor* v) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->feat_end();
  }

  static int CNullFeat(GeoArrowVisitor* v) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->null_feat();
  }

  static int CGeomStart(GeoArrowVisitor* v, GeoArrowGeometryType geometry_type,
                        GeoArrowDimensions dimensions) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->geom_start(geometry_type, -1);
  }

  static int CGeomEnd(GeoArrowVisitor* v) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->geom_end();
  }

  static int CRingStart(GeoArrowVisitor* v) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->ring_start(-1);
  }

  static int CRingEnd(GeoArrowVisitor* v) {
    auto constructor = reinterpret_cast<Constructor*>(v->private_data);
    return constructor->ring_end();
  }

  static int CCoords(GeoArrowVisitor* v, const GeoArrowCoordView* coords) {
    return EINVAL;
  }
};

class PointConstructor : public Constructor {
 public:
  PointConstructor(const ImportOptions& options) : Constructor(options) {}

  GeoArrowErrorCode geom_start(GeoArrowGeometryType geometry_type,
                               int64_t size) override {
    if (size != 0 && geometry_type != GEOARROW_GEOMETRY_TYPE_POINT &&
        geometry_type != GEOARROW_GEOMETRY_TYPE_MULTIPOINT &&
        geometry_type != GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION) {
      throw Exception(
          "PointConstructor input must be empty, point, multipoint, or "
          "collection");
    }

    if (size > 0) {
      points_.reserve(points_.size() + size);
    }

    return GEOARROW_OK;
  }

  GeoArrowErrorCode coords(const GeoArrowCoordView* view) override {
    int64_t n = view->n_coords;
    int coord_size = view->n_values;

    for (int64_t i = 0; i < n; i++) {
      if (coord_empty(view, i)) {
        continue;
      }

      if (options_.projection() == nullptr) {
        S2Point pt(GEOARROW_COORD_VIEW_VALUE(view, i, 0),
                   GEOARROW_COORD_VIEW_VALUE(view, i, 1),
                   GEOARROW_COORD_VIEW_VALUE(view, i, 2));
        points_.push_back(pt);
      } else {
        R2Point pt(GEOARROW_COORD_VIEW_VALUE(view, i, 0),
                   GEOARROW_COORD_VIEW_VALUE(view, i, 1));
        points_.push_back(options_.projection()->Unproject(pt));
      }
    }

    return GEOARROW_OK;
  }

  std::unique_ptr<Geography> finish() override {
    auto result = absl::make_unique<PointGeography>(std::move(points_));
    points_.clear();
    return std::unique_ptr<Geography>(result.release());
  }

 private:
  bool coord_empty(const GeoArrowCoordView* view, int64_t i) {
    for (int j = 0; j < view->n_values; j++) {
      if (!std::isnan(GEOARROW_COORD_VIEW_VALUE(view, i, j))) {
        return false;
      }
    }

    return true;
  }
};

class PolylineConstructor : public Constructor {
 public:
  PolylineConstructor(const ImportOptions& options) : Constructor(options) {}

  GeoArrowErrorCode geom_start(GeoArrowGeometryType geometry_type,
                               int64_t size) override {
    if (size != 0 && geometry_type != GEOARROW_GEOMETRY_TYPE_LINESTRING &&
        geometry_type != GEOARROW_GEOMETRY_TYPE_MULTILINESTRING &&
        geometry_type != GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION) {
      throw Exception(
          "PolylineConstructor input must be empty, linestring, "
          "multilinestring, or collection");
    }

    if (size > 0 && geometry_type == GEOARROW_GEOMETRY_TYPE_LINESTRING) {
      input_points_.reserve(size);
    }

    return GEOARROW_OK;
  }

  GeoArrowErrorCode geom_end() override {
    finish_points();

    if (!points_.empty()) {
      auto polyline = absl::make_unique<S2Polyline>();
      polyline->Init(std::move(points_));

      // Previous version of s2 didn't check for this, so in
      // this check is temporarily disabled to avoid mayhem in
      // reverse dependency checks.
      if (options_.check() && !polyline->IsValid()) {
        polyline->FindValidationError(&error_);
        throw Exception(error_.text());
      }

      polylines_.push_back(std::move(polyline));
    }

    return GEOARROW_OK;
  }

  std::unique_ptr<Geography> finish() override {
    std::unique_ptr<PolylineGeography> result;

    if (polylines_.empty()) {
      result = absl::make_unique<PolylineGeography>();
    } else {
      result = absl::make_unique<PolylineGeography>(std::move(polylines_));
      polylines_.clear();
    }

    return std::unique_ptr<Geography>(result.release());
  }

 private:
  std::vector<std::unique_ptr<S2Polyline>> polylines_;
  S2Error error_;
};

class PolygonConstructor : public Constructor {
 public:
  PolygonConstructor(const ImportOptions& options) : Constructor(options) {}

  GeoArrowErrorCode ring_start(int64_t size) override {
    input_points_.clear();
    if (size > 0) {
      input_points_.reserve(size);
    }

    return GEOARROW_OK;
  }

  GeoArrowErrorCode ring_end() override {
    finish_points();

    if (points_.empty()) {
      return GEOARROW_OK;
    }

    points_.pop_back();
    auto loop = absl::make_unique<S2Loop>();
    loop->set_s2debug_override(S2Debug::DISABLE);
    loop->Init(std::move(points_));

    if (!options_.oriented()) {
      loop->Normalize();
    }

    if (options_.check() && !loop->IsValid()) {
      std::stringstream err;
      err << "Loop " << (loops_.size()) << " is not valid: ";
      loop->FindValidationError(&error_);
      err << error_.text();
      throw Exception(err.str());
    }

    loops_.push_back(std::move(loop));
    points_.clear();
    return GEOARROW_OK;
  }

  std::unique_ptr<Geography> finish() override {
    auto polygon = absl::make_unique<S2Polygon>();
    polygon->set_s2debug_override(S2Debug::DISABLE);
    if (options_.oriented()) {
      polygon->InitOriented(std::move(loops_));
    } else {
      polygon->InitNested(std::move(loops_));
    }

    loops_.clear();

    if (options_.check() && !polygon->IsValid()) {
      polygon->FindValidationError(&error_);
      throw Exception(error_.text());
    }

    auto result = absl::make_unique<PolygonGeography>(std::move(polygon));
    return std::unique_ptr<Geography>(result.release());
  }

 private:
  std::vector<std::unique_ptr<S2Loop>> loops_;
  S2Error error_;
};

class CollectionConstructor : public Constructor {
 public:
  CollectionConstructor(const ImportOptions& options)
      : Constructor(options),
        point_constructor_(options),
        polyline_constructor_(options),
        polygon_constructor_(options),
        collection_constructor_(nullptr),
        level_(0) {}

  GeoArrowErrorCode geom_start(GeoArrowGeometryType geometry_type,
                               int64_t size) override {
    level_++;
    if (level_ == 1 &&
        geometry_type == GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION) {
      active_constructor_ = nullptr;
      return GEOARROW_OK;
    }

    if (active_constructor_ != nullptr) {
      active_constructor_->geom_start(geometry_type, size);
      return GEOARROW_OK;
    }

    switch (geometry_type) {
      case GEOARROW_GEOMETRY_TYPE_POINT:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
        active_constructor_ = &point_constructor_;
        break;
      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
      case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
        active_constructor_ = &polyline_constructor_;
        break;
      case GEOARROW_GEOMETRY_TYPE_POLYGON:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
        active_constructor_ = &polygon_constructor_;
        break;
      case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
        this->collection_constructor_ =
            absl::make_unique<CollectionConstructor>(options_);
        this->active_constructor_ = this->collection_constructor_.get();
        break;
      default:
        throw Exception("CollectionConstructor: unsupported geometry type");
    }

    active_constructor_->geom_start(geometry_type, size);
    return GEOARROW_OK;
  }

  GeoArrowErrorCode ring_start(int64_t size) override {
    active_constructor_->ring_start(size);
    return GEOARROW_OK;
  }

  GeoArrowErrorCode coords(const GeoArrowCoordView* view) override {
    active_constructor_->coords(view);
    return GEOARROW_OK;
  }

  GeoArrowErrorCode ring_end() override {
    active_constructor_->ring_end();
    return GEOARROW_OK;
  }

  GeoArrowErrorCode geom_end() override {
    level_--;

    if (level_ >= 1) {
      active_constructor_->geom_end();
    }

    if (level_ == 1) {
      auto feature = active_constructor_->finish();
      features_.push_back(std::move(feature));
      active_constructor_ = nullptr;
    }

    return GEOARROW_OK;
  }

  std::unique_ptr<Geography> finish() override {
    auto result = absl::make_unique<GeographyCollection>(std::move(features_));
    features_.clear();
    return std::unique_ptr<Geography>(result.release());
  }

 private:
  PointConstructor point_constructor_;
  PolylineConstructor polyline_constructor_;
  PolygonConstructor polygon_constructor_;
  std::unique_ptr<CollectionConstructor> collection_constructor_;

 protected:
  Constructor* active_constructor_;
  int level_;
  std::vector<std::unique_ptr<Geography>> features_;
};

}  // namespace geoarrow

}  // namespace s2geography
