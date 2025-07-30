
#include "s2geography/accessors.h"

#include <s2/s2earth.h>

#include "s2geography/arrow_udf/arrow_udf_internal.h"
#include "s2geography/build.h"
#include "s2geography/geography.h"

namespace s2geography {

bool s2_is_collection(const PolygonGeography& geog) {
  int num_outer_loops = 0;
  for (int i = 0; i < geog.Polygon()->num_loops(); i++) {
    S2Loop* loop = geog.Polygon()->loop(i);
    num_outer_loops += loop->depth() == 0;
    if (num_outer_loops > 1) {
      return true;
    }
  }

  return false;
}

bool s2_is_collection(const Geography& geog) {
  int dimension = s2_dimension(geog);

  if (dimension == -1) {
    return false;
  }

  if (dimension == 0) {
    return s2_num_points(geog) > 1;
  }

  if (dimension == 1) {
    int num_chains = 0;
    for (int i = 0; i < geog.num_shapes(); i++) {
      std::unique_ptr<S2Shape> shape = geog.Shape(i);
      num_chains += shape->num_chains();
      if (num_chains > 1) {
        return true;
      }
    }

    return false;
  }

  auto polygon_geog_ptr = dynamic_cast<const PolygonGeography*>(&geog);
  if (polygon_geog_ptr != nullptr) {
    return s2_is_collection(*polygon_geog_ptr);
  } else {
    std::unique_ptr<PolygonGeography> built = s2_build_polygon(geog);
    return s2_is_collection(*built);
  }
}

int s2_dimension(const Geography& geog) {
  int dimension = geog.dimension();
  if (dimension != -1) {
    return dimension;
  }

  for (int i = 0; i < geog.num_shapes(); i++) {
    std::unique_ptr<S2Shape> shape = geog.Shape(i);
    if (shape->dimension() > dimension) {
      dimension = shape->dimension();
    }
  }

  return dimension;
}

int s2_num_points(const Geography& geog) {
  int num_points = 0;
  for (int i = 0; i < geog.num_shapes(); i++) {
    std::unique_ptr<S2Shape> shape = geog.Shape(i);
    switch (shape->dimension()) {
      case 0:
      case 2:
        num_points += shape->num_edges();
        break;
      case 1:
        num_points += shape->num_edges() + shape->num_chains();
        break;
    }
  }

  return num_points;
}

bool s2_is_empty(const Geography& geog) {
  for (int i = 0; i < geog.num_shapes(); i++) {
    std::unique_ptr<S2Shape> shape = geog.Shape(i);
    if (!shape->is_empty()) {
      return false;
    }
  }

  return true;
}

double s2_area(const PolygonGeography& geog) {
  return geog.Polygon()->GetArea();
}

double s2_area(const GeographyCollection& geog) {
  double area = 0;
  for (auto& feature : geog.Features()) {
    area += s2_area(*feature);
  }
  return area;
}

double s2_area(const Geography& geog) {
  if (s2_dimension(geog) != 2) {
    return 0;
  }

  auto polygon_geog_ptr = dynamic_cast<const PolygonGeography*>(&geog);
  if (polygon_geog_ptr != nullptr) {
    return s2_area(*polygon_geog_ptr);
  }

  auto collection_geog_ptr = dynamic_cast<const GeographyCollection*>(&geog);
  if (collection_geog_ptr != nullptr) {
    return s2_area(*collection_geog_ptr);
  }

  std::unique_ptr<PolygonGeography> built = s2_build_polygon(geog);
  return s2_area(*built);
}

double s2_length(const Geography& geog) {
  double length = 0;

  if (s2_dimension(geog) == 1) {
    for (int i = 0; i < geog.num_shapes(); i++) {
      std::unique_ptr<S2Shape> shape = geog.Shape(i);
      for (int j = 0; j < shape->num_edges(); j++) {
        S2Shape::Edge e = shape->edge(j);
        S1ChordAngle angle(e.v0, e.v1);
        length += angle.radians();
      }
    }
  }

  return length;
}

double s2_perimeter(const Geography& geog) {
  double length = 0;

  if (s2_dimension(geog) == 2) {
    for (int i = 0; i < geog.num_shapes(); i++) {
      std::unique_ptr<S2Shape> shape = geog.Shape(i);
      for (int j = 0; j < shape->num_edges(); j++) {
        S2Shape::Edge e = shape->edge(j);
        S1ChordAngle angle(e.v0, e.v1);
        length += angle.radians();
      }
    }
  }

  return length;
}

double s2_x(const Geography& geog) {
  double out = NAN;
  for (int i = 0; i < geog.num_shapes(); i++) {
    std::unique_ptr<S2Shape> shape = geog.Shape(i);
    if (shape->dimension() == 0 && shape->num_edges() == 1 && std::isnan(out)) {
      S2LatLng pt(shape->edge(0).v0);
      out = pt.lng().degrees();
    } else if (shape->dimension() == 0 && shape->num_edges() == 1) {
      return NAN;
    }
  }

  return out;
}

double s2_y(const Geography& geog) {
  double out = NAN;
  for (int i = 0; i < geog.num_shapes(); i++) {
    std::unique_ptr<S2Shape> shape = geog.Shape(i);
    if (shape->dimension() == 0 && shape->num_edges() == 1 && std::isnan(out)) {
      S2LatLng pt(shape->edge(0).v0);
      out = pt.lat().degrees();
    } else if (shape->dimension() == 0 && shape->num_edges() == 1) {
      return NAN;
    }
  }

  return out;
}

bool s2_find_validation_error(const PolylineGeography& geog, S2Error* error) {
  for (const auto& polyline : geog.Polylines()) {
    if (polyline->FindValidationError(error)) {
      return true;
    }
  }

  return false;
}

bool s2_find_validation_error(const PolygonGeography& geog, S2Error* error) {
  return geog.Polygon()->FindValidationError(error);
}

bool s2_find_validation_error(const GeographyCollection& geog, S2Error* error) {
  for (const auto& feature : geog.Features()) {
    if (s2_find_validation_error(*feature, error)) {
      return true;
    }
  }

  return false;
}

bool s2_find_validation_error(const Geography& geog, S2Error* error) {
  if (geog.dimension() == 0) {
    *error = S2Error();
    return false;
  }

  if (geog.dimension() == 1) {
    auto poly_ptr = dynamic_cast<const PolylineGeography*>(&geog);
    if (poly_ptr != nullptr) {
      return s2_find_validation_error(*poly_ptr, error);
    } else {
      try {
        auto poly = s2_build_polyline(geog);
        return s2_find_validation_error(*poly, error);
      } catch (Exception& e) {
        *error = ToS2Error(absl::Status(absl::StatusCode::kInternal, e.what()));
        return true;
      }
    }
  }

  if (geog.dimension() == 2) {
    auto poly_ptr = dynamic_cast<const PolygonGeography*>(&geog);
    if (poly_ptr != nullptr) {
      return s2_find_validation_error(*poly_ptr, error);
    } else {
      try {
        auto poly = s2_build_polygon(geog);
        return s2_find_validation_error(*poly, error);
      } catch (Exception& e) {
        *error = ToS2Error(absl::Status(absl::StatusCode::kInternal, e.what()));
        return true;
      }
    }
  }

  auto collection_ptr = dynamic_cast<const GeographyCollection*>(&geog);
  if (collection_ptr != nullptr) {
    return s2_find_validation_error(*collection_ptr, error);
  } else {
    try {
      auto collection = s2_build_polygon(geog);
      return s2_find_validation_error(*collection, error);
    } catch (Exception& e) {
      *error = ToS2Error(absl::Status(absl::StatusCode::kInternal, e.what()));
      return true;
    }
  }

  throw Exception(
      "s2_find_validation() error not implemented for this geography type");
}

namespace arrow_udf {
struct S2LengthExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_length(value) * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Length() {
  return std::make_unique<UnaryUDF<S2LengthExec>>();
}

struct S2AreaExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_area(value) * S2Earth::RadiusMeters() * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Area() {
  return std::make_unique<UnaryUDF<S2AreaExec>>();
}

struct S2PerimeterExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string>& options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_perimeter(value) * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Perimeter() {
  return std::make_unique<UnaryUDF<S2PerimeterExec>>();
}
}  // namespace arrow_udf

}  // namespace s2geography
