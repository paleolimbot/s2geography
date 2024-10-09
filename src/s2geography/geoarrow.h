
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

/// \brief Options used to import/export Geography objects from/to GeoArrow arrays
class ImportExportOptions {
 public:
  ImportExportOptions()
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

class Reader {
 public:
  enum class InputType { kWKT, kWKB };
  Reader();
  ~Reader();

  void Init(const ArrowSchema* schema) { Init(schema, ImportExportOptions()); }

  void Init(const ArrowSchema* schema, const ImportExportOptions& options);

  void Init(InputType input_type, const ImportExportOptions& options);

  void ReadGeography(const ArrowArray* array, int64_t offset, int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out);

 private:
  std::unique_ptr<ReaderImpl> impl_;
};

class WriterImpl;

class Writer {
  public:
  enum class OutputType { kWKT, kWKB };

  Writer();
  ~Writer();

  void Init(const ArrowSchema* schema, const ImportExportOptions& options);

 private:
  std::unique_ptr<WriterImpl> impl_;
};

}  // namespace geoarrow

}  // namespace s2geography
