
#include "s2geography/wkt-writer.h"

#include "s2geography/accessors.h"

// using ryu for double -> char* is ~5x faster!
#ifndef geoarrow_d2s_fixed_n
static inline int geoarrow_compat_d2s_fixed_n(double f, uint32_t precision,
                                              char* result) {
  return snprintf(result, 128, "%.*g", precision, f);
}

#define geoarrow_d2s_fixed_n geoarrow_compat_d2s_fixed_n
#endif

namespace s2geography {

class WKTStreamWriter : public Handler {
 public:
  WKTStreamWriter(std::ostream& stream)
      : significant_digits_(16),
        is_first_ring_(true),
        is_first_coord_(true),
        dimensions_(util::Dimensions::XY),
        stream_(stream) {
    stack_.reserve(32);
  }

  void new_dimensions(util::Dimensions dimensions) { dimensions_ = dimensions; }

  Result feat_start() {
    stack_.clear();
    is_first_ring_ = true;
    is_first_coord_ = true;
    return Result::CONTINUE;
  }

  Result null_feat() { throw Exception("null_feat() is not implemented"); }

  Result geom_start(util::GeometryType geometry_type, int64_t size) {
    if (stack_.size() > 0 && stack_.back().part > 0) {
      write_string(", ");
    }

    if (stack_.size() > 0) {
      stack_.back().part++;
    }

    if (stack_.size() == 0 ||
        stack_.back().type == util::GeometryType::GEOMETRYCOLLECTION) {
      switch (geometry_type) {
        case util::GeometryType::POINT:
          write_string("POINT");
          break;
        case util::GeometryType::LINESTRING:
          write_string("LINESTRING");
          break;
        case util::GeometryType::POLYGON:
          write_string("POLYGON");
          break;
        case util::GeometryType::MULTIPOINT:
          write_string("MULTIPOINT");
          break;
        case util::GeometryType::MULTILINESTRING:
          write_string("MULTILINESTRING");
          break;
        case util::GeometryType::MULTIPOLYGON:
          write_string("MULTIPOLYGON");
          break;
        case util::GeometryType::GEOMETRYCOLLECTION:
          write_string("GEOMETRYCOLLECTION");

          break;
        default:
          throw Exception("Unknown geometry type in WKTWriter");
      }

      write_char(' ');

      switch (dimensions_) {
        case util::Dimensions::XYZM:
          write_string("ZM ");
          break;
        case util::Dimensions::XYZ:
          write_string("Z ");
          break;
        case util::Dimensions::XYM:
          write_string("M ");
          break;
        default:
          break;
      }
    }

    if (size == 0) {
      write_string("EMPTY");
    } else {
      write_char('(');
    }

    stack_.push_back(State{geometry_type, size, 0});
    is_first_ring_ = true;
    is_first_coord_ = true;
    return Result::CONTINUE;
  }

  Result ring_start(int64_t size) {
    if (!is_first_ring_) {
      write_string(", ");
    } else {
      is_first_ring_ = false;
    }

    write_char('(');

    is_first_coord_ = true;
    return Result::CONTINUE;
  }

  Result coords(const double* coord, int64_t n, int32_t coord_size) {
    for (int64_t i = 0; i < n; i++) {
      if (!is_first_coord_) {
        write_string(", ");
      }

      write_coord(coord[i * coord_size]);
      for (int32_t j = 1; j < coord_size; j++) {
        write_char(' ');
        write_coord(coord[i * coord_size + j]);
      }

      is_first_coord_ = false;
    }

    return Result::CONTINUE;
  }

  Result ring_end() {
    write_char(')');
    return Result::CONTINUE;
  }

  Result geom_end() {
    if (stack_.size() > 0 && stack_.back().size != 0) {
      write_char(')');
    }

    if (stack_.size() > 0) {
      stack_.pop_back();
    }

    return Result::CONTINUE;
  }

  Result feat_end() { return Result::CONTINUE; }

 private:
  class State {
   public:
    util::GeometryType type;
    int64_t size;
    int64_t part;
  };

  int significant_digits_;
  std::vector<State> stack_;
  bool is_first_ring_;
  bool is_first_coord_;
  util::Dimensions dimensions_;

  char write_buffer_[1024];
  std::ostream& stream_;

  void write_coord(double value) {
    int n_needed =
        geoarrow_d2s_fixed_n(value, significant_digits_, write_buffer_);
    stream_ << std::string(write_buffer_, n_needed);
  }

  void write_string(const char* value) { stream_ << value; }

  void write_char(char value) { stream_ << std::string(&value, 1); }
};

WKTWriter::WKTWriter()
    : geometry_type_(util::GeometryType::GEOMETRY_TYPE_UNKNOWN) {
  this->exporter_ = absl::make_unique<WKTStreamWriter>(stream_);
  exporter_->new_dimensions(util::Dimensions::XY);
  exporter_->new_geometry_type(util::GeometryType::GEOMETRY_TYPE_UNKNOWN);
}

std::string WKTWriter::write_feature(const S2Geography& geog) {
  stream_.str("");
  handle_feature(geog, exporter_.get());
  return stream_.str();
}

Handler::Result WKTWriter::handle_points(const PointGeography& geog, Handler* handler) {
  double coords[2];

  if (geog.Points().size() == 0) {
    handler->new_geometry_type(util::GeometryType::POINT);
    handler->geom_start(util::GeometryType::POINT, 0);
    handler->geom_end();
  } else if (geog.Points().size() == 1) {
    handler->new_geometry_type(util::GeometryType::POINT);
    handler->geom_start(util::GeometryType::POINT, 1);
    S2LatLng ll(geog.Points()[0]);
    coords[0] = ll.lng().degrees();
    coords[1] = ll.lat().degrees();
    handler->coords(coords, 1, 2);
    handler->geom_end();
  } else {
    handler->new_geometry_type(util::GeometryType::MULTIPOINT);
    handler->geom_start(util::GeometryType::MULTIPOINT, geog.Points().size());

    for (const S2Point& pt: geog.Points()) {
      handler->geom_start(util::GeometryType::POINT, 1);
      S2LatLng ll(pt);
      coords[0] = ll.lng().degrees();
      coords[1] = ll.lat().degrees();
      handler->coords(coords, 1, 2);
      handler->geom_end();
    }

    handler->geom_end();
  }

  return Handler::Result::CONTINUE;
}

Handler::Result WKTWriter::handle_polylines(const PolylineGeography& geog,
                                 Handler* handler) {

  double coords[2];

  if (geog.Polylines().size() == 0) {
    handler->new_geometry_type(util::GeometryType::LINESTRING);
    handler->geom_start(util::GeometryType::LINESTRING, 0);
    handler->geom_end();
  } else if (geog.Polylines().size() == 1) {
    handler->new_geometry_type(util::GeometryType::LINESTRING);

    const auto& poly = geog.Polylines()[0];
    handler->geom_start(util::GeometryType::LINESTRING, poly->num_vertices());

    for (int i = 0; i < poly->num_vertices(); i++) {
      S2LatLng ll(poly->vertex(i));
      coords[0] = ll.lng().degrees();
      coords[1] = ll.lat().degrees();
      handler->coords(coords, 1, 2);
    }

    handler->geom_end();
  } else {
    handler->new_geometry_type(util::GeometryType::MULTILINESTRING);
    handler->geom_start(util::GeometryType::MULTILINESTRING, geog.Polylines().size());

    for (const auto& poly: geog.Polylines()) {
      handler->geom_start(util::GeometryType::LINESTRING, poly->num_vertices());

      for (int i = 0; i < poly->num_vertices(); i++) {
        S2LatLng ll(poly->vertex(i));
        coords[0] = ll.lng().degrees();
        coords[1] = ll.lat().degrees();
        handler->coords(coords, 1, 2);
      }

      handler->geom_end();
    }

    handler->geom_end();
  }

  return Handler::Result::CONTINUE;
}

Handler::Result WKTWriter::handle_polygon(const PolygonGeography& geog, Handler* handler) {
  throw Exception("Polygon not implemented");
}

Handler::Result WKTWriter::handle_collection(const S2GeographyCollection& geog,
                                             Handler* handler) {
  throw Exception("Collection not implemented");
}

Handler::Result WKTWriter::handle_feature(const S2Geography& geog, Handler* handler) {
  Handler::Result result;

  HANDLE_OR_RETURN(handler->feat_start());

  auto child_point = dynamic_cast<const PointGeography*>(&geog);
  if (child_point != nullptr) {
    HANDLE_OR_RETURN(handle_points(*child_point, handler));
  } else {
    auto child_polyline = dynamic_cast<const PolylineGeography*>(&geog);
    if (child_polyline != nullptr) {
      HANDLE_OR_RETURN(handle_polylines(*child_polyline, handler));
    } else {
      auto child_polygon = dynamic_cast<const PolygonGeography*>(&geog);
      if (child_polygon != nullptr) {
        HANDLE_OR_RETURN(handle_polygon(*child_polygon, handler));
      } else {
        auto child_collection =
            dynamic_cast<const S2GeographyCollection*>(&geog);
        if (child_collection != nullptr) {
          HANDLE_OR_RETURN(handle_collection(*child_collection, handler));
        } else {
          throw Exception("Unsupported S2Geography subclass");
        }
      }
    }
  }

  HANDLE_OR_RETURN(handler->feat_end());
  return Handler::Result::CONTINUE;
}

}  // namespace s2geography
