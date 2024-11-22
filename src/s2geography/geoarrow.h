
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s2/s1angle.h"
#include "s2/s2projections.h"
#include "s2geography/arrow_abi.h"
#include "s2geography/geography.h"
#include "s2geography/projections.h"

namespace s2geography {

namespace geoarrow {

/// \brief Inspect the underlying GeoArrow implementation version
const char* version();

/// \brief Options used to build Geography objects from GeoArrow arrays

class TessellationOptions {
 public:
  TessellationOptions()
      : projection_(lnglat()), tessellate_tolerance_(S1Angle::Infinity()) {}
  S2::Projection* projection() const { return projection_.get(); }
  void set_projection(std::shared_ptr<S2::Projection> projection) {
    projection_ = std::move(projection);
  }
  S1Angle tessellate_tolerance() const { return tessellate_tolerance_; }
  void set_tessellate_tolerance(S1Angle tessellate_tolerance) {
    tessellate_tolerance_ = tessellate_tolerance;
  }

 protected:
  std::shared_ptr<S2::Projection> projection_;
  S1Angle tessellate_tolerance_;
};

class ImportOptions : public TessellationOptions {
 public:
  ImportOptions() : TessellationOptions(), oriented_(false), check_(true) {}
  bool oriented() const { return oriented_; }
  void set_oriented(bool oriented) { oriented_ = oriented; }
  bool check() const { return check_; }
  void set_check(bool check) { check_ = check; }

 private:
  bool oriented_;
  bool check_;
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

class ExportOptions : public TessellationOptions {
 public:
  ExportOptions() : TessellationOptions(), precision_(16) {}
  // The number of digits after the decimal to output in WKT (default 16)
  int precision() const { return precision_; }
  void set_precision(int precision) { precision_ = precision; }

 private:
  int precision_;
};

class WriterImpl;

/// \brief Array writer for any GeoArrow extension array
///
/// This class is used to convert Geography objects into an ArrowArray
/// with geoarrow data (serialized or native).
class Writer {
 public:
  enum class OutputType { kWKT, kWKB };
  Writer();
  ~Writer();

  void Init(const ArrowSchema* schema) { Init(schema, ExportOptions()); }

  void Init(const ArrowSchema* schema, const ExportOptions& options);

  void Init(OutputType output_type, const ExportOptions& options);

  void WriteGeography(const Geography& geog);

  // TODO
  // void WriteNull()

  void Finish(struct ArrowArray* out);

 private:
  std::unique_ptr<WriterImpl> impl_;
};

}  // namespace geoarrow

}  // namespace s2geography
