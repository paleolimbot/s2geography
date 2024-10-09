
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s2/s1angle.h"
#include "s2/s2projections.h"
#include "s2geography/arrow_abi.h"
#include "s2geography/geography.h"

namespace s2geography {

namespace geoarrow {

/// \brief Inspect the underlying GeoArrow implementation version
const char* version();

S2::Projection* lnglat();

/// \brief Options used to build Geography objects from GeoArrow arrays
class ImportOptions {
 public:
  ImportOptions()
      : oriented_(false),
        check_(true),
        projection_(lnglat()),
        tessellate_tolerance_(S1Angle::Infinity()) {}
  bool oriented() const { return oriented_; }
  void set_oriented(bool oriented) { oriented_ = oriented; }
  bool check() const { return check_; }
  void set_check(bool check) { check_ = check; }
  S2::Projection* projection() const { return projection_; }
  void set_projection(S2::Projection* projection) { projection_ = projection; }
  S1Angle tessellate_tolerance() const { return tessellate_tolerance_; }
  void set_tessellate_tolerance(S1Angle tessellate_tolerance) {
    tessellate_tolerance_ = tessellate_tolerance;
  }

 private:
  bool oriented_;
  bool check_;
  S2::Projection* projection_;
  S1Angle tessellate_tolerance_;
};

class ReaderImpl;

/// \brief Array reader for any GeoArrow extension array
///
/// This class is used to convert an ArrowArray with geoarrow data (serialized
/// or native) into a vector of Geography objects.
class Reader {
 public:
  enum class InputType { kWKT, kWKB };
  Reader();
  ~Reader();

  void Init(const ArrowSchema* schema) { Init(schema, ImportOptions()); }

  void Init(const ArrowSchema* schema, const ImportOptions& options);

  void Init(InputType input_type, const ImportOptions& options);

  void ReadGeography(const ArrowArray* array, int64_t offset, int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out);

 private:
  std::unique_ptr<ReaderImpl> impl_;
};

}  // namespace geoarrow

}  // namespace s2geography
