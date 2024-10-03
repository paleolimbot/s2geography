
#include <sstream>

#include "s2/s1angle.h"
#include "s2/s2edge_tessellator.h"
#include "s2/s2projections.h"
#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

#include "vendored/geoarrow/geoarrow.h"

namespace s2geography {

namespace geoarrow {

const char* version() { return GeoArrowVersion(); }

S2::Projection* lnglat() {
  static S2::PlateCarreeProjection projection(180);
  return &projection;
}

// This should really be in nanoarrow or geoarrow
// https://github.com/geoarrow/geoarrow-c-geos/blob/33ad0ba21c76c09e9d72fc4e4ae0b9ff9da61848/src/geoarrow_geos/geoarrow_geos.c#L323-L360
struct GeoArrowBitmapReader {
  const uint8_t* bits;
  int64_t byte_i;
  int bit_i;
  uint8_t byte;
};

static inline void GeoArrowBitmapReaderInit(
    struct GeoArrowBitmapReader* bitmap_reader, const uint8_t* bits,
    int64_t offset) {
  std::memset(bitmap_reader, 0, sizeof(struct GeoArrowBitmapReader));
  bitmap_reader->bits = bits;

  if (bits != NULL) {
    bitmap_reader->byte_i = offset / 8;
    bitmap_reader->bit_i = offset % 8;
    if (bitmap_reader->bit_i == 0) {
      bitmap_reader->bit_i = 7;
      bitmap_reader->byte_i--;
    } else {
      bitmap_reader->bit_i--;
    }
  }
}

static inline int8_t GeoArrowBitmapReaderNextIsNull(
    struct GeoArrowBitmapReader* bitmap_reader) {
  if (bitmap_reader->bits == NULL) {
    return 0;
  }

  if (++bitmap_reader->bit_i == 8) {
    bitmap_reader->byte = bitmap_reader->bits[++bitmap_reader->byte_i];
    bitmap_reader->bit_i = 0;
  }

  return (bitmap_reader->byte & (1 << bitmap_reader->bit_i)) == 0;
}

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
    v->null_feat = &CNullFeat;
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
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->feat_start();
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CFeatEnd(GeoArrowVisitor* v) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->feat_end();
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CNullFeat(GeoArrowVisitor* v) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->null_feat();
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CGeomStart(GeoArrowVisitor* v, GeoArrowGeometryType geometry_type,
                        GeoArrowDimensions dimensions) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->geom_start(geometry_type, -1);
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CGeomEnd(GeoArrowVisitor* v) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->geom_end();
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CRingStart(GeoArrowVisitor* v) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->ring_start(-1);
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CRingEnd(GeoArrowVisitor* v) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->ring_end();
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
  }

  static int CCoords(GeoArrowVisitor* v, const GeoArrowCoordView* coords) {
    try {
      auto constructor = reinterpret_cast<Constructor*>(v->private_data);
      return constructor->coords(coords);
    } catch (std::exception& e) {
      GeoArrowErrorSet(v->error, "%s", e.what());
      return EINVAL;
    }
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
        this->active_constructor_ = &point_constructor_;
        break;
      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
      case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
        this->active_constructor_ = &polyline_constructor_;
        break;
      case GEOARROW_GEOMETRY_TYPE_POLYGON:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
        this->active_constructor_ = &polygon_constructor_;
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

class FeatureConstructor : public CollectionConstructor {
 public:
  FeatureConstructor(const ImportOptions& options)
      : CollectionConstructor(options) {}

  void SetOutput(std::vector<std::unique_ptr<Geography>>* out) { out_ = out; }

  GeoArrowErrorCode feat_start() override {
    active_constructor_ = nullptr;
    level_ = 0;
    features_.clear();
    feat_null_ = false;
    geom_start(GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION, 1);
    return GEOARROW_OK;
  }

  GeoArrowErrorCode null_feat() override {
    feat_null_ = true;
    return GEOARROW_OK;
  }

  GeoArrowErrorCode feat_end() override {
    if (feat_null_) {
      out_->push_back(std::unique_ptr<Geography>(nullptr));
    } else {
      out_->push_back(finish_feature());
    }

    return GEOARROW_OK;
  }

 private:
  bool feat_null_;
  std::vector<std::unique_ptr<Geography>>* out_;

  std::unique_ptr<Geography> finish_feature() {
    geom_end();

    if (features_.empty()) {
      return absl::make_unique<GeographyCollection>();
    } else {
      std::unique_ptr<Geography> feature = std::move(features_.back());
      if (feature == nullptr) {
        throw Exception("finish_feature() generated nullptr");
      }

      features_.pop_back();
      return feature;
    }
  }
};

class ReaderImpl {
 public:
  ReaderImpl() {
    error_.message[0] = '\0';
    wkt_reader_.private_data = nullptr;
    wkb_reader_.private_data = nullptr;
  }

  ~ReaderImpl() {
    if (wkt_reader_.private_data != nullptr) {
      GeoArrowWKTReaderReset(&wkt_reader_);
    }

    if (wkb_reader_.private_data != nullptr) {
      GeoArrowWKBReaderReset(&wkb_reader_);
    }
  }

  void Init(const struct ArrowSchema* schema, const ImportOptions& options) {
    options_ = options;

    int code = GeoArrowArrayViewInitFromSchema(&array_view_, schema, &error_);
    ThrowNotOk(code);
    InitCommon();
  }

  void Init(GeoArrowType type, const ImportOptions& options) {
    options_ = options;

    int code = GeoArrowArrayViewInitFromType(&array_view_, type);
    ThrowNotOk(code);
    InitCommon();
  }

  void InitCommon() {
    constructor_ = absl::make_unique<FeatureConstructor>(options_);
    constructor_->InitVisitor(&visitor_);
    visitor_.error = &error_;

    if (array_view_.schema_view.type == GEOARROW_TYPE_WKT) {
      GeoArrowWKTReaderInit(&wkt_reader_);
    }

    if (array_view_.schema_view.type == GEOARROW_TYPE_WKB) {
      GeoArrowWKBReaderInit(&wkb_reader_);
    }
  }

  void ReadGeography(const struct ArrowArray* array, int64_t offset, int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out) {
    int code = GeoArrowArrayViewSetArray(&array_view_, array, &error_);
    ThrowNotOk(code);

    if (length == 0) {
      return;
    }

    constructor_->SetOutput(out);

    switch (array_view_.schema_view.type) {
      case GEOARROW_TYPE_WKT:
        code = VisitWKT(offset, length);
        break;
      case GEOARROW_TYPE_WKB:
        code = VisitWKB(offset, length);
        break;
      default:
        code = GeoArrowArrayViewVisit(&array_view_, offset, length, &visitor_);
        break;
    }

    ThrowNotOk(code);
  }

 private:
  ImportOptions options_;
  std::unique_ptr<FeatureConstructor> constructor_;
  GeoArrowArrayView array_view_;
  GeoArrowWKTReader wkt_reader_;
  GeoArrowWKBReader wkb_reader_;
  GeoArrowVisitor visitor_;
  GeoArrowError error_;

  int VisitWKT(int64_t offset, int64_t length) {
    offset += array_view_.offset[0];
    const uint8_t* validity = array_view_.validity_bitmap;
    const int32_t* offsets = array_view_.offsets[0];
    const char* data = reinterpret_cast<const char*>(array_view_.data);
    GeoArrowStringView item;

    GeoArrowBitmapReader bitmap;
    GeoArrowBitmapReaderInit(&bitmap, validity, offset);

    for (int64_t i = 0; i < length; i++) {
      if (GeoArrowBitmapReaderNextIsNull(&bitmap)) {
        GEOARROW_RETURN_NOT_OK(visitor_.feat_start(&visitor_));
        GEOARROW_RETURN_NOT_OK(visitor_.null_feat(&visitor_));
        GEOARROW_RETURN_NOT_OK(visitor_.feat_end(&visitor_));
      } else {
        item.size_bytes = offsets[offset + i + 1] - offsets[offset + i];
        item.data = data + offsets[offset + i];
        GEOARROW_RETURN_NOT_OK(
            GeoArrowWKTReaderVisit(&wkt_reader_, item, &visitor_));
      }
    }

    return GEOARROW_OK;
  }

  int VisitWKB(int64_t offset, int64_t length) {
    offset += array_view_.offset[0];
    const uint8_t* validity = array_view_.validity_bitmap;
    const int32_t* offsets = array_view_.offsets[0];
    const uint8_t* data = array_view_.data;
    GeoArrowBufferView item;

    GeoArrowBitmapReader bitmap;
    GeoArrowBitmapReaderInit(&bitmap, validity, offset);

    for (int64_t i = 0; i < length; i++) {
      if (GeoArrowBitmapReaderNextIsNull(&bitmap)) {
        GEOARROW_RETURN_NOT_OK(visitor_.feat_start(&visitor_));
        GEOARROW_RETURN_NOT_OK(visitor_.null_feat(&visitor_));
        GEOARROW_RETURN_NOT_OK(visitor_.feat_end(&visitor_));
      } else {
        item.size_bytes = offsets[offset + i + 1] - offsets[offset + i];
        item.data = data + offsets[offset + i];
        GEOARROW_RETURN_NOT_OK(
            GeoArrowWKBReaderVisit(&wkb_reader_, item, &visitor_));
      }
    }

    return GEOARROW_OK;
  }

  void ThrowNotOk(int code) {
    if (code != GEOARROW_OK) {
      throw Exception(error_.message);
    }
  }
};

Reader::Reader() : impl_(new ReaderImpl()) {}

Reader::~Reader() { impl_.reset(); }

void Reader::Init(const void* schema, const ImportOptions& options) {
  impl_->Init(static_cast<const struct ArrowSchema*>(schema), options);
}

void Reader::Init(InputType input_type, const ImportOptions& options) {
  switch (input_type) {
    case InputType::kWKT:
      impl_->Init(GEOARROW_TYPE_WKT, options);
      break;
    case InputType::kWKB:
      impl_->Init(GEOARROW_TYPE_WKB, options);
      break;
    default:
      throw Exception("Input type not supported");
  }
}

void Reader::ReadGeography(const void* array, int64_t offset,
                           int64_t length,
                           std::vector<std::unique_ptr<Geography>>* out) {
  impl_->ReadGeography(static_cast<const struct ArrowArray*>(array), offset, length, out);
}

}  // namespace geoarrow

}  // namespace s2geography
