#include <s2/s1angle.h>
#include <s2/s2edge_tessellator.h>

#include "geoarrow/geoarrow.h"
#include "geoarrow/geoarrow.hpp"
#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

using XY = geoarrow::array_util::XY<double>;
using XYZ = geoarrow::array_util::XYZ<double>;
using XYM = geoarrow::array_util::XY<double>;
using XYZM = geoarrow::array_util::XYZ<double>;
using XYSequence = geoarrow::array_util::CoordSequence<XY>;
using XYZSequence = geoarrow::array_util::CoordSequence<XYZ>;
using WKBArray = geoarrow::wkb_util::WKBArray<int32_t>;
using geoarrow::wkb_util::WKBGeometry;
using geoarrow::wkb_util::WKBParser;
using geoarrow::wkb_util::WKBSequence;

namespace s2geography {

namespace geoarrow {

namespace {

template <typename T>
bool CoordIsEmpty(T pt) {
  uint32_t nan_count = 0;
  for (const auto item : pt) {
    nan_count += std::isnan(item);
  }
  return nan_count == pt.size();
}

/// \brief Constants to identify the coordinate translation strategy
enum class Translator { kLiteral, kProjected, kTessellated };

/// \brief Import/export points as literal XYZ values
///
/// This is the translator that most faithfully represents its input (by not
/// applying any projection whatsoever and keeping the unit vector point
/// representation).
///
/// This translator igores the value of tessellator and the projection.
template <typename Sequence>
struct LiteralTranslator {
  static void ImportPoints(const Sequence& seq, std::vector<S2Point>* out,
                           const S2::Projection* projection) {
    seq.template VisitVertices<XY>([&](XY pt) {
      if (!CoordIsEmpty(pt)) {
        out->push_back({pt.x(), pt.y(), pt.z()});
      }
    });
  }

  static void ImportSequence(const Sequence& seq, std::vector<S2Point>* out,
                             const S2EdgeTessellator& tessellator,
                             const S2::Projection* projection) {
    // Could maybe use a memcpy here
    out->reserve(out->size() + seq.size());
    seq.template VisitVertices<XYZ>([&](XYZ pt) {
      out->push_back({pt.x(), pt.y(), pt.z()});
    });
  }
};

/// \brief Import/export vertices according to projection
///
/// Translates between S2Points and projected space by applying a projection
/// to each point in isolation. This is the transformation, for example, that
/// would be appropriate for WKB or GeoArrow annotated with spherical edges.
///
/// This translator igores the value of tessellator.
template <typename Sequence>
struct ProjectedTranslator {
  static void ImportPoints(const Sequence& seq, std::vector<S2Point>* out,
                           const S2::Projection* projection) {
    seq.template VisitVertices<XY>([&](XY pt) {
      if (!CoordIsEmpty(pt)) {
        out->push_back(projection->Unproject({pt.x(), pt.y()}));
      }
    });
  }

  static void ImportSequence(const Sequence& seq, std::vector<S2Point>* out,
                             const S2EdgeTessellator& tessellator,
                             const S2::Projection* projection) {
    out->reserve(out->size() + seq.size());
    seq.template VisitVertices<XY>([&](XY pt) {
      out->push_back(projection->Unproject({pt.x(), pt.y()}));
    });
  }
};

/// \brief Import/export planar edges from/to projected space
///
/// Transforms vertices according to projection but also ensures that planar
/// edges are tessellated to keep them within a specified tolerance of their
/// original positions in projected space (when importing) or spherical space
/// (when exporting).
template <typename Sequence>
struct TessellatedTranslator {
  static void ImportPoints(const Sequence& seq, std::vector<S2Point>* out,
                           const S2::Projection* projection) {
    seq.template VisitVertices<XY>([&](XY pt) {
      if (!CoordIsEmpty(pt)) {
        out->push_back(projection->Unproject({pt.x(), pt.y()}));
      }
    });
  }

  static void ImportSequence(const Sequence& seq, std::vector<S2Point>* out,
                             const S2EdgeTessellator& tessellator,
                             const S2::Projection* projection) {
    out->reserve(out->size() + seq.size());
    seq.template VisitEdges<XY>([&](XY v0, XY v1) {
      tessellator.AppendUnprojected({v0.x(), v0.y()}, {v1.x(), v1.y()}, out);
    });
  }
};

Translator GetTranslator(const ImportOptions& options) {
  if (options.projection() == nullptr) {
    return Translator::kLiteral;
  } else if (options.tessellate_tolerance() == S1Angle::Infinity()) {
    return Translator::kProjected;
  } else {
    return Translator::kTessellated;
  }
}

template <typename T>
void SetArrayGeneric(T& array, const ArrowArray* arrow_array,
                     struct GeoArrowArrayView* array_view) {
  struct GeoArrowError error {};
  if (GeoArrowArrayViewSetArray(array_view, arrow_array, &error) !=
      GEOARROW_OK) {
    throw Exception(error.message);
  }

  array.Init(array_view);
}

}  // namespace

NewReaderImpl::NewReaderImpl(const ImportOptions& options)
    : options_(options), projection_(options_.projection()) {
  if (options_.projection()) {
    tessellator_ = absl::make_unique<S2EdgeTessellator>(
        options_.projection(), options_.tessellate_tolerance());
  }
}

class WKBReaderImpl : public NewReaderImpl {
 public:
  explicit WKBReaderImpl(const ImportOptions& options)
      : NewReaderImpl(options) {
    GeoArrowArrayViewInitFromType(&array_view_, GEOARROW_TYPE_WKB);
  }

  void VisitConst(const struct ArrowArray* array, GeographyVisitor& visitor) {
    SetArray(array);

    for (int64_t i = 0; i < wkb_array_.value.length; i++) {
      if (wkb_array_.is_null(i)) {
        visitor(nullptr);
        continue;
      }

      Parse(i);

      switch (ReadGeometryDispatch(geometry_)) {
        case GeographyKind::POINT:
          visitor(point_.get());
          break;
        case GeographyKind::POLYLINE:
          visitor(polyline_.get());
          break;
        case GeographyKind::POLYGON:
          visitor(polygon_.get());
          break;
        case GeographyKind::GEOGRAPHY_COLLECTION:
          visitor(collection_.get());
          break;
        default:
          throw Exception("Unexpected geography type output");
      }
    }
  }

  void ReadGeography(const struct ArrowArray* array, int64_t offset,
                     int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out) {
    SetArray(array);

    for (int64_t i = 0; i < length; i++) {
      if (wkb_array_.is_null(offset + i)) {
        out->push_back(nullptr);
        continue;
      }

      Parse(offset + i);
      out->push_back(ReadGeometryUniquePtrDispatch(geometry_));
    }
  }

 private:
  WKBGeometry geometry_;
  WKBArray wkb_array_;
  WKBParser wkb_parser_;
  struct GeoArrowArrayView array_view_ {};
  std::vector<S2Point> vertices_;
  std::vector<std::unique_ptr<S2Polyline>> polylines_;
  std::vector<std::unique_ptr<S2Loop>> loops_;
  std::vector<std::unique_ptr<Geography>> geographies_;
  std::unique_ptr<PointGeography> point_;
  std::unique_ptr<PolylineGeography> polyline_;
  std::unique_ptr<PolygonGeography> polygon_;
  std::unique_ptr<GeographyCollection> collection_;

  void SetArray(const ArrowArray* array) {
    SetArrayGeneric(wkb_array_, array, &array_view_);
  }

  void Parse(int64_t i) {
    WKBParser::Status status =
        wkb_parser_.Parse(wkb_array_.value.blob(i), &geometry_);
    if (status != WKBParser::Status::OK) {
      throw Exception(std::string("Error parsing WKB at index ") +
                      std::to_string(i) + ": " +
                      wkb_parser_.ErrorToString(status));
    }
  }

  std::unique_ptr<Geography> ReadGeometryUniquePtrDispatch(
      const WKBGeometry& geom) {
    if (options_.projection() == nullptr) {
      return ReadGeometryUniquePtr<LiteralTranslator<WKBSequence>>(geom);
    } else if (options_.tessellate_tolerance() == S1Angle::Infinity()) {
      return ReadGeometryUniquePtr<ProjectedTranslator<WKBSequence>>(geom);
    } else {
      return ReadGeometryUniquePtr<TessellatedTranslator<WKBSequence>>(geom);
    }
  }

  GeographyKind ReadGeometryDispatch(const WKBGeometry& geom) {
    if (options_.projection() == nullptr) {
      return ReadGeometry<LiteralTranslator<WKBSequence>>(geom);
    } else if (options_.tessellate_tolerance() == S1Angle::Infinity()) {
      return ReadGeometry<ProjectedTranslator<WKBSequence>>(geom);
    } else {
      return ReadGeometry<TessellatedTranslator<WKBSequence>>(geom);
    }
  }

  template <typename Translator>
  std::unique_ptr<Geography> ReadGeometryUniquePtr(const WKBGeometry& geom) {
    switch (ReadGeometry<Translator>(geom)) {
      case GeographyKind::POINT:
        return std::move(point_);
      case GeographyKind::POLYLINE:
        return std::move(polyline_);
      case GeographyKind::POLYGON:
        return std::move(polygon_);
      case GeographyKind::GEOGRAPHY_COLLECTION:
        return std::move(collection_);
      default:
        throw Exception("Unexpected geography type output");
    }
  }

  template <typename Translator>
  GeographyKind ReadGeometry(const WKBGeometry& geom) {
    switch (geom.geometry_type) {
      case GEOARROW_GEOMETRY_TYPE_POINT:
        vertices_.clear();
        ReadPoint<Translator>(geom);
        FinishPointGeography();
        return GeographyKind::POINT;
      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
        ReadLinestring<Translator>(geom);
        FinishPolylineGeography();
        return GeographyKind::POLYLINE;
      case GEOARROW_GEOMETRY_TYPE_POLYGON:
        ReadPolygon<Translator>(geom);
        FinishPolygonGeography();
        return GeographyKind::POLYGON;
      case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
        vertices_.clear();
        ReadMultiPoint<Translator>(geom);
        FinishPointGeography();
        return GeographyKind::POINT;
      case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
        ReadMultiLinestring<Translator>(geom);
        FinishPolylineGeography();
        return GeographyKind::POLYLINE;
      case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
        ReadMultiPolygon<Translator>(geom);
        FinishPolygonGeography();
        return GeographyKind::POLYGON;
      case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
        if (IsTopLevel(geom)) {
          geographies_.clear();
          ReadGeometryCollection<Translator>(geom);
          FinishGeographyCollection();
        } else {
          ReadGeometryCollection<Translator>(geom);
        }
      default:
        throw Exception("Unexpected WKB geometry type");
    }
  }

  template <typename Translator>
  void ReadSequence(const WKBSequence& seq) {
    vertices_.clear();
    Translator::ImportSequence(seq, &vertices_, *tessellator_, projection_);
  }

  template <typename Translator>
  void ReadPoint(const WKBGeometry& geom) {
    S2_DCHECK_EQ(geom.geometry_type, GEOARROW_GEOMETRY_TYPE_POINT);
    S2_DCHECK_EQ(geom.NumSequences(), 1);
    // Don't clear the current sequence
    Translator::ImportPoints(geom.Sequence(0), &vertices_, projection_);
  }

  template <typename Translator>
  void ReadLinestring(const WKBGeometry& geom) {
    S2_DCHECK_EQ(geom.NumSequences(), 1);
    ReadSequence<Translator>(geom.Sequence(0));
    auto polyline = absl::make_unique<S2Polyline>(vertices_, S2Debug::DISABLE);
    // Validate
    polylines_.push_back(std::move(polyline));
  }

  template <typename Translator>
  void ReadRings(const WKBGeometry& geom) {
    loops_.reserve(loops_.size() + geom.NumSequences());
    for (uint32_t i = 0; i < geom.NumSequences(); i++) {
      ReadSequence<Translator>(geom.Sequence(i));
      vertices_.pop_back();
      auto loop = absl::make_unique<S2Loop>(vertices_, S2Debug::DISABLE);
      // Validate
      loops_.push_back(std::move(loop));
    }
  }

  template <typename Translator>
  void ReadPolygon(const WKBGeometry& geom) {
    loops_.clear();
    ReadRings<Translator>(geom);
  }

  template <typename Translator>
  void ReadMultiPoint(const WKBGeometry& geom) {
    vertices_.clear();
    for (uint32_t i = 0; i < geom.NumGeometries(); i++) {
      CheckGeometryType(geom, GEOARROW_GEOMETRY_TYPE_POINT);
      ReadPoint<Translator>(geom);
    }
  }

  template <typename Translator>
  void ReadMultiLinestring(const WKBGeometry& geom) {
    polylines_.clear();
    for (uint32_t i = 0; i < geom.NumGeometries(); i++) {
      CheckGeometryType(geom, GEOARROW_GEOMETRY_TYPE_LINESTRING);
      ReadLinestring<Translator>(geom.Geometry(i));
    }
  }

  template <typename Translator>
  void ReadMultiPolygon(const WKBGeometry& geom) {
    loops_.clear();
    for (uint32_t i = 0; i < geom.NumGeometries(); i++) {
      CheckGeometryType(geom, GEOARROW_GEOMETRY_TYPE_POLYGON);
      ReadRings<Translator>(geom.Geometry(i));
    }
  }

  template <typename Translator>
  void ReadGeometryCollection(const WKBGeometry& geom) {
    for (uint32_t i = 0; i < geom.NumGeometries(); i++) {
      geographies_.push_back(
          ReadGeometryUniquePtr<Translator>(geom.Geometry(i)));
    }
  }

  void FinishPointGeography() {
    point_ = absl::make_unique<PointGeography>(std::move(vertices_));
  }

  void FinishPolylineGeography() {
    polyline_ = absl::make_unique<PolylineGeography>(std::move(polylines_));
  }

  void FinishPolygonGeography() {
    auto polygon =
        absl::make_unique<S2Polygon>(std::move(loops_), S2Debug::DISABLE);
    // Validate
    polygon_ = absl::make_unique<PolygonGeography>(std::move(polygon));
  }

  void FinishGeographyCollection() {
    collection_ =
        absl::make_unique<GeographyCollection>(std::move(geographies_));
  }

  void CheckGeometryType(const WKBGeometry& geom,
                         enum GeoArrowGeometryType geometry_type) {
    if (geom.geometry_type != geometry_type) {
      throw Exception(std::string("Expected WKB ") +
                      GeoArrowGeometryTypeString(geometry_type) + " but got " +
                      GeoArrowGeometryTypeString(geom.geometry_type));
    }
  }

  bool IsTopLevel(const WKBGeometry& geom) { return &geom == &geometry_; }
};

class GeoArrowPointReader : public NewReaderImpl {
 public:
  explicit GeoArrowPointReader(const ImportOptions& options,
                               enum GeoArrowType type)
      : NewReaderImpl(options) {
    GeoArrowArrayViewInitFromType(&array_view_, type);
  }

  void VisitConst(const struct ArrowArray* array, GeographyVisitor& visitor) {
    SetArray(array);

    switch (GetTranslator(options_)) {
      case Translator::kLiteral:
        VisitConstGeneric<ProjectedTranslator<XYZ>>(array_xyz_, array, visitor);
        break;
      default:
        VisitConstGeneric<ProjectedTranslator<XY>>(array_xy_, array, visitor);
        break;
    }
  }

  void ReadGeography(const struct ArrowArray* array, int64_t offset,
                     int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out) {
    SetArray(array);

    switch (GetTranslator(options_)) {
      case Translator::kLiteral:
        ReadGeographyGeneric<ProjectedTranslator<XYZ>>(array_xyz_, array,
                                                       offset, length, out);
        break;
      default:
        ReadGeographyGeneric<ProjectedTranslator<XY>>(array_xy_, array, offset,
                                                      length, out);
        break;
    }
  }

 private:
  ::geoarrow::array_util::PointArray<XY> array_xy_;
  ::geoarrow::array_util::PointArray<XYZ> array_xyz_;
  struct GeoArrowArrayView array_view_ {};

  void SetArray(const ArrowArray* array) {
    switch (GetTranslator(options_)) {
      case Translator::kLiteral:
        SetArrayGeneric(array_xyz_, array, &array_view_);
        break;
      default:
        SetArrayGeneric(array_xy_, array, &array_view_);
        break;
    }
  }

  template <typename Translator, typename T>
  void VisitConstGeneric(T& typed_array, const struct ArrowArray* array,
                         GeographyVisitor& visitor) {
    PointGeography geog;
    std::vector<S2Point>* pts = geog.mutable_points();

    if (!typed_array.validity) {
      for (auto coord : typed_array.Coords()) {
        pts->clear();
        Translator::ImportPoints(coord, pts, options_.projection());
        visitor(&geog);
      }
    } else {
      for (int64_t i = 0; i < typed_array.value.length; i++) {
        if (typed_array.is_null(i)) {
          visitor(nullptr);
          continue;
        }

        pts->clear();
        Translator::ImportPoints(typed_array.value.coord(i), pts,
                                 options_.projection());
      }
    }
  }

  template <typename Translator, typename T>
  void ReadGeographyGeneric(T& typed_array, const struct ArrowArray* array,
                            int64_t offset, int64_t length,
                            std::vector<std::unique_ptr<Geography>>* out) {
    for (int64_t i = 0; i < length; i++) {
      if (typed_array.is_null(offset + i)) {
        out->push_back(nullptr);
        continue;
      }

      std::vector<S2Point> pts;
      Translator::ImportPoints(typed_array.value.coord(offset + i), &pts,
                               options_.projection());
      out->push_back(absl::make_unique<PointGeography>(pts));
    }
  }
};

namespace {
std::unique_ptr<NewReaderImpl> MakeNewReader(
    const ::geoarrow::GeometryDataType& data_type,
    const ImportOptions& options) {
  switch (data_type.id()) {
    case GEOARROW_TYPE_WKB:
      return absl::make_unique<WKBReaderImpl>(options);
    default:
      switch (data_type.geometry_type()) {
        case GEOARROW_GEOMETRY_TYPE_POINT:
          return absl::make_unique<GeoArrowPointReader>(options,
                                                        data_type.id());
        default:
          throw Exception("GeoArrow type " + data_type.ToString() +
                          " not yet implemented");
      }
  }
}
}  // namespace

std::unique_ptr<NewReaderImpl> MakeNewReader(struct ArrowSchema* schema,
                                             const ImportOptions& options) {
  auto data_type = ::geoarrow::GeometryDataType::Make(schema);
  return MakeNewReader(data_type, options);
}

std::unique_ptr<NewReaderImpl> MakeNewReader(Reader::InputType input_type,
                                             const ImportOptions& options) {
  switch (input_type) {
    case Reader::InputType::kWKT:
      return MakeNewReader(::geoarrow::Wkt(), options);
    case Reader::InputType::kWKB:
      return MakeNewReader(::geoarrow::Wkb(), options);
  }
}

}  // namespace geoarrow

}  // namespace s2geography
