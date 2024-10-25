
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

class TessellationOptions {
 public:
  TessellationOptions()
      : projection_(lnglat()),
        tessellate_tolerance_(S1Angle::Infinity()) {}
  S2::Projection* projection() const { return projection_; }
  void set_projection(S2::Projection* projection) { projection_ = projection; }
  S1Angle tessellate_tolerance() const { return tessellate_tolerance_; }
  void set_tessellate_tolerance(S1Angle tessellate_tolerance) {
    tessellate_tolerance_ = tessellate_tolerance;
  }

 protected:
  S2::Projection* projection_;
  S1Angle tessellate_tolerance_;
};

class ImportOptions : public TessellationOptions {
 public:
  ImportOptions()
      : TessellationOptions(),
        oriented_(false),
        check_(true) {}
  bool oriented() const { return oriented_; }
  void set_oriented(bool oriented) { oriented_ = oriented; }
  bool check() const { return check_; }
  void set_check(bool check) { check_ = check; }

 private:
  bool oriented_;
  bool check_;
};

class ReaderImpl;

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

class ExportOptions : public TessellationOptions {
 public:
  ExportOptions()
      : TessellationOptions(),
        significant_digits_(6) {}
  int significant_digits() const { return significant_digits_; }
  void set_significant_digits(int significant_digits) { significant_digits_ = significant_digits; }

 private:
  int significant_digits_;
};

class WriterImpl;

/// \brief Array writer for any GeoArrow extension array
///
/// This class is used to convert a vector of Geography objects into an ArrowArray
/// with geoarrow data (serialized or native).
class Writer {
 public:
  enum class OutputType { kWKT, kWKB };
  Writer();
  ~Writer();

  void Init(const ArrowSchema* schema) { Init(schema, ExportOptions()); }

  void Init(const ArrowSchema* schema, const ExportOptions& options);

  void WriteGeography(const Geography& geog);

  void Finish(struct ArrowArray* out);

 private:
  std::unique_ptr<WriterImpl> impl_;
};

}  // namespace geoarrow

}  // namespace s2geography
