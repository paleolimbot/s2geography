
#include "s2geography/geoarrow.h"

#include <sstream>

#include "geoarrow/geoarrow.h"
#include "s2/s1angle.h"
#include "s2/s2edge_tessellator.h"
#include "s2geography/geography.h"

namespace s2geography {

namespace geoarrow {

const char* version() { return GeoArrowVersion(); }


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

/// \brief Construct Geography objects while visiting a GeoArrow array.
///
/// This class implements the visitor callbacks that construct Geography
/// objects. This visitor gets called by the Reader, which iterates over the
/// features in a GeoArrow ArrowArray (that can be either in WKT or WKB
/// serialized format or in native format).
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
    reader_.private_data = nullptr;
  }

  ~ReaderImpl() {
    if (reader_.private_data != nullptr) {
      GeoArrowArrayReaderReset(&reader_);
    }
  }

  void Init(const ArrowSchema* schema, const ImportOptions& options) {
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

    GeoArrowArrayReaderInit(&reader_);
  }

  void ReadGeography(const ArrowArray* array, int64_t offset, int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out) {
    int code = GeoArrowArrayViewSetArray(&array_view_, array, &error_);
    ThrowNotOk(code);

    if (length == 0) {
      return;
    }

    constructor_->SetOutput(out);
    code = GeoArrowArrayReaderVisit(&reader_, &array_view_, offset, length, &visitor_);
    ThrowNotOk(code);
  }

 private:
  ImportOptions options_;
  std::unique_ptr<FeatureConstructor> constructor_;
  GeoArrowArrayView array_view_;
  GeoArrowArrayReader reader_;
  GeoArrowVisitor visitor_;
  GeoArrowError error_;

  void ThrowNotOk(int code) {
    if (code != GEOARROW_OK) {
      throw Exception(error_.message);
    }
  }
};

Reader::Reader() : impl_(new ReaderImpl()) {}

Reader::~Reader() { impl_.reset(); }

void Reader::Init(const ArrowSchema* schema, const ImportOptions& options) {
  impl_->Init(schema, options);
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

void Reader::ReadGeography(const ArrowArray* array, int64_t offset,
                           int64_t length,
                           std::vector<std::unique_ptr<Geography>>* out) {
  impl_->ReadGeography(array, offset, length, out);
}

// Write Geography objects to a GeoArrow array.
//
// This class walks through the geographies and calls the visitor methods
// provided by the geoarrow-c GeoArrowArrayWriter, which then builds up the
// GeoArrow array.
class WriterImpl {
 public:
  WriterImpl() {
    error_.message[0] = '\0';
    writer_.private_data = nullptr;
  }

  ~WriterImpl() {
    if (writer_.private_data != nullptr) {
      GeoArrowArrayWriterReset(&writer_);
    }
  }

  void Init(const ArrowSchema* schema, const ExportOptions& options) {
    options_ = options;

    int code = GeoArrowArrayWriterInitFromSchema(&writer_, schema);
    ThrowNotOk(code);

    GeoArrowSchemaView schema_view;
    code = GeoArrowSchemaViewInit(&schema_view, schema, &error_);
    ThrowNotOk(code);
    type_ = schema_view.type;

    InitCommon();
  }

  void Init(GeoArrowType type, const ExportOptions& options) {
    options_ = options;
    type_ = type;

    int code = GeoArrowArrayWriterInitFromType(&writer_, type);
    ThrowNotOk(code);

    InitCommon();
  }

  void InitCommon() {
    int code;

    if (type_ == GEOARROW_TYPE_WKT || type_ == GEOARROW_TYPE_LARGE_WKT) {
      code = GeoArrowArrayWriterSetPrecision(&writer_, options_.precision());
      ThrowNotOk(code);
      code = GeoArrowArrayWriterSetFlatMultipoint(&writer_, false);
      ThrowNotOk(code);
    }

    visitor_.error = &error_;
    code = GeoArrowArrayWriterInitVisitor(&writer_, &visitor_);
    ThrowNotOk(code);

    if (options_.projection() != nullptr) {
      this->tessellator_ = absl::make_unique<S2EdgeTessellator>(
          options_.projection(), options_.tessellate_tolerance());
    }

    // Currently we always visit single coordinate pairs one by one, so set
    // up the appropiate view for that once, which is then reused
    coords_view_.n_coords = 1;
    coords_view_.n_values = 2;
    coords_view_.coords_stride = 2;
    coords_view_.values[0] = &coords_[0];
    coords_view_.values[1] = &coords_[1];
  }

  void WriteGeography(const Geography& geog) { VisitFeature(geog); }

  void Finish(struct ArrowArray* out) {
    int code = GeoArrowArrayWriterFinish(&writer_, out, &error_);
    ThrowNotOk(code);
  }

 private:
  ExportOptions options_;
  GeoArrowType type_;
  GeoArrowArrayWriter writer_;
  GeoArrowVisitor visitor_;
  GeoArrowCoordView coords_view_;
  double coords_[2];
  GeoArrowError error_;
  std::unique_ptr<S2EdgeTessellator> tessellator_;
  std::vector<R2Point> points_;

  void ProjectS2Point(const S2Point& pt) {
    R2Point out = options_.projection()->Project(pt);
    coords_[0] = out.x();
    coords_[1] = out.y();
  }

  int VisitPoints(const PointGeography& point) {

    if (point.Points().size() == 0) {
      // empty Point
      GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
          &visitor_, GEOARROW_GEOMETRY_TYPE_POINT, GEOARROW_DIMENSIONS_XY));
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    } else if (point.Points().size() == 1) {
      // Point
      GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
          &visitor_, GEOARROW_GEOMETRY_TYPE_POINT, GEOARROW_DIMENSIONS_XY));
      ProjectS2Point(point.Points()[0]);
      GEOARROW_RETURN_NOT_OK(visitor_.coords(&visitor_, &coords_view_));
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    } else {
      // MultiPoint
      GEOARROW_RETURN_NOT_OK(
          visitor_.geom_start(&visitor_, GEOARROW_GEOMETRY_TYPE_MULTIPOINT,
                              GEOARROW_DIMENSIONS_XY));

      for (const S2Point& pt : point.Points()) {
        GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
            &visitor_, GEOARROW_GEOMETRY_TYPE_POINT, GEOARROW_DIMENSIONS_XY));
        ProjectS2Point(pt);
        GEOARROW_RETURN_NOT_OK(visitor_.coords(&visitor_, &coords_view_));
        GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));
      }

      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));
    }
    return GEOARROW_OK;
  }

  int VisitPolylineEdges(const S2Polyline& poly) {

    if (poly.num_vertices() == 0) {
      throw Exception("Unexpected S2Polyline with 0 vertices");
    } else if (poly.num_vertices() == 1) {
      // this is an invalid case, but we handle it for printing
      ProjectS2Point(poly.vertex(0));
      GEOARROW_RETURN_NOT_OK(visitor_.coords(&visitor_, &coords_view_));
      return GEOARROW_OK;
    }

    for (int i = 1; i < poly.num_vertices(); i++) {
        const S2Point& pt0(poly.vertex(i - 1));
        const S2Point& pt1(poly.vertex(i));
        tessellator_->AppendProjected(pt0, pt1, &points_);
    }

    for (const auto& pt : points_) {
      coords_[0] = pt.x();
      coords_[1] = pt.y();
      GEOARROW_RETURN_NOT_OK(visitor_.coords(&visitor_, &coords_view_));
    }
    points_.clear();

    return GEOARROW_OK;
  }

  int VisitPolylines(const PolylineGeography& geog) {

    if (geog.Polylines().size() == 0) {
      // empty LineString
      GEOARROW_RETURN_NOT_OK(
          visitor_.geom_start(&visitor_, GEOARROW_GEOMETRY_TYPE_LINESTRING,
                              GEOARROW_DIMENSIONS_XY));
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    } else if (geog.Polylines().size() == 1) {
      // LineString
      const auto& poly = geog.Polylines()[0];
      GEOARROW_RETURN_NOT_OK(
          visitor_.geom_start(&visitor_, GEOARROW_GEOMETRY_TYPE_LINESTRING,
                              GEOARROW_DIMENSIONS_XY));
      GEOARROW_RETURN_NOT_OK(VisitPolylineEdges(*poly));
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    } else {
      // MultiLineString
      GEOARROW_RETURN_NOT_OK(
          visitor_.geom_start(&visitor_, GEOARROW_GEOMETRY_TYPE_MULTILINESTRING,
                              GEOARROW_DIMENSIONS_XY));

      for (const auto& poly : geog.Polylines()) {
        GEOARROW_RETURN_NOT_OK(
            visitor_.geom_start(&visitor_, GEOARROW_GEOMETRY_TYPE_LINESTRING,
                                GEOARROW_DIMENSIONS_XY));
        GEOARROW_RETURN_NOT_OK(VisitPolylineEdges(*poly));
        GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));
      }

      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));
    }

    return GEOARROW_OK;
  }

  int VisitLoopShell(const S2Loop* loop) {

    if (loop->num_vertices() == 0) {
      throw Exception("Unexpected S2Loop with 0 verties");
    }

    GEOARROW_RETURN_NOT_OK(visitor_.ring_start(&visitor_));

    // loop until `num_vertices()` instead of `num_vertices() - 1` to include
    // the closing edge from the last vertex to the first vertex
    for (int i = 1; i <= loop->num_vertices(); i++) {
      const S2Point& pt0(loop->vertex(i - 1));
      const S2Point& pt1(loop->vertex(i));
      tessellator_->AppendProjected(pt0, pt1, &points_);
    }

    for (const auto& pt : points_) {
      coords_[0] = pt.x();
      coords_[1] = pt.y();
      GEOARROW_RETURN_NOT_OK(visitor_.coords(&visitor_, &coords_view_));
    }
    points_.clear();

    GEOARROW_RETURN_NOT_OK(visitor_.ring_end(&visitor_));

    return GEOARROW_OK;
  }

  int VisitLoopHole(const S2Loop* loop) {

    if (loop->num_vertices() == 0) {
      throw Exception("Unexpected S2Loop with 0 verties");
    }

    GEOARROW_RETURN_NOT_OK(visitor_.ring_start(&visitor_));

    // For the hole, we use the vertices in reverse order to ensure the holes
    // have the opposite orientation of the shell
    for (int i = loop->num_vertices() - 2; i >= 0; i--) {
      const S2Point& pt0(loop->vertex(i + 1));
      const S2Point& pt1(loop->vertex(i));
      tessellator_->AppendProjected(pt0, pt1, &points_);
    }

    const S2Point& pt0(loop->vertex(0));
    const S2Point& pt1(loop->vertex(loop->num_vertices() - 1));
    tessellator_->AppendProjected(pt0, pt1, &points_);

    for (const auto& pt : points_) {
      coords_[0] = pt.x();
      coords_[1] = pt.y();
      GEOARROW_RETURN_NOT_OK(visitor_.coords(&visitor_, &coords_view_));
    }
    points_.clear();

    GEOARROW_RETURN_NOT_OK(visitor_.ring_end(&visitor_));
    return GEOARROW_OK;
  }

  int VisitPolygonShell(const S2Polygon& poly, int loop_start) {
    const S2Loop* loop0 = poly.loop(loop_start);
    GEOARROW_RETURN_NOT_OK(VisitLoopShell(loop0));
    for (int j = loop_start + 1; j <= poly.GetLastDescendant(loop_start); j++) {
      const S2Loop* loop = poly.loop(j);
      if (loop->depth() == (loop0->depth() + 1)) {
        GEOARROW_RETURN_NOT_OK(VisitLoopHole(loop));
      }
    }

    return GEOARROW_OK;
  }

  int VisitPolygons(const PolygonGeography& geog) {
    const S2Polygon& poly = *geog.Polygon();

    // find the outer shells (loop depth = 0, 2, 4, etc.)
    std::vector<int> outer_shell_loop_ids;

    outer_shell_loop_ids.reserve(poly.num_loops());
    for (int i = 0; i < poly.num_loops(); i++) {
      if ((poly.loop(i)->depth() % 2) == 0) {
        outer_shell_loop_ids.push_back(i);
      }
    }

    if (outer_shell_loop_ids.size() == 0) {
      // empty Polygon
      GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
          &visitor_, GEOARROW_GEOMETRY_TYPE_POLYGON, GEOARROW_DIMENSIONS_XY));
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    } else if (outer_shell_loop_ids.size() == 1) {
      // Polygon
      GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
          &visitor_, GEOARROW_GEOMETRY_TYPE_POLYGON, GEOARROW_DIMENSIONS_XY));
      GEOARROW_RETURN_NOT_OK(VisitPolygonShell(poly, outer_shell_loop_ids[0]));
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    } else {
      // MultiPolygon
      GEOARROW_RETURN_NOT_OK(
          visitor_.geom_start(&visitor_, GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON,
                              GEOARROW_DIMENSIONS_XY));
      for (size_t i = 0; i < outer_shell_loop_ids.size(); i++) {
        GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
            &visitor_, GEOARROW_GEOMETRY_TYPE_POLYGON, GEOARROW_DIMENSIONS_XY));
        GEOARROW_RETURN_NOT_OK(
            VisitPolygonShell(poly, outer_shell_loop_ids[i]));
        GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));
      }
      GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));
    }

    return GEOARROW_OK;
  }

  int VisitCollection(const GeographyCollection& geog) {
    GEOARROW_RETURN_NOT_OK(visitor_.geom_start(
        &visitor_, GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION,
        GEOARROW_DIMENSIONS_XY));

    for (const auto& child_geog : geog.Features()) {
      auto child_point = dynamic_cast<const PointGeography*>(child_geog.get());
      if (child_point != nullptr) {
        GEOARROW_RETURN_NOT_OK(VisitPoints(*child_point));
      } else {
        auto child_polyline =
            dynamic_cast<const PolylineGeography*>(child_geog.get());
        if (child_polyline != nullptr) {
          GEOARROW_RETURN_NOT_OK(VisitPolylines(*child_polyline));
        } else {
          auto child_polygon =
              dynamic_cast<const PolygonGeography*>(child_geog.get());
          if (child_polygon != nullptr) {
            GEOARROW_RETURN_NOT_OK(VisitPolygons(*child_polygon));
          } else {
            auto child_collection =
                dynamic_cast<const GeographyCollection*>(child_geog.get());
            if (child_collection != nullptr) {
              GEOARROW_RETURN_NOT_OK(VisitCollection(*child_collection));
            } else {
              throw Exception("Unsupported Geography subclass");
            }
          }
        }
      }
    }
    GEOARROW_RETURN_NOT_OK(visitor_.geom_end(&visitor_));

    return GEOARROW_OK;
  }

  int VisitFeature(const Geography& geog) {
    GEOARROW_RETURN_NOT_OK(visitor_.feat_start(&visitor_));

    auto child_point = dynamic_cast<const PointGeography*>(&geog);
    if (child_point != nullptr) {
      GEOARROW_RETURN_NOT_OK(VisitPoints(*child_point));
    } else {
      auto child_polyline = dynamic_cast<const PolylineGeography*>(&geog);
      if (child_polyline != nullptr) {
        GEOARROW_RETURN_NOT_OK(VisitPolylines(*child_polyline));
      } else {
        auto child_polygon = dynamic_cast<const PolygonGeography*>(&geog);
        if (child_polygon != nullptr) {
          GEOARROW_RETURN_NOT_OK(VisitPolygons(*child_polygon));
        } else {
          auto child_collection =
              dynamic_cast<const GeographyCollection*>(&geog);
          if (child_collection != nullptr) {
            GEOARROW_RETURN_NOT_OK(VisitCollection(*child_collection));
          } else {
            throw Exception("Unsupported Geography subclass");
          }
        }
      }
    }
    GEOARROW_RETURN_NOT_OK(visitor_.feat_end(&visitor_));
    return GEOARROW_OK;
  }

  void ThrowNotOk(int code) {
    if (code != GEOARROW_OK) {
      throw Exception(error_.message);
    }
  }
};

Writer::Writer() : impl_(new WriterImpl()) {}

Writer::~Writer() { impl_.reset(); }

void Writer::Init(const ArrowSchema* schema, const ExportOptions& options) {
  impl_->Init(schema, options);
}

void Writer::Init(OutputType output_type, const ExportOptions& options) {
  switch (output_type) {
    case OutputType::kWKT:
      impl_->Init(GEOARROW_TYPE_WKT, options);
      break;
    case OutputType::kWKB:
      impl_->Init(GEOARROW_TYPE_WKB, options);
      break;
    default:
      throw Exception("Output type not supported");
  }
}

void Writer::WriteGeography(const Geography& geog) {
  impl_->WriteGeography(geog);
}

void Writer::Finish(struct ArrowArray* out) { impl_->Finish(out); }

}  // namespace geoarrow

}  // namespace s2geography
