
#include "s2geography/arrow_udf/arrow_udf.h"

#include <cerrno>
#include <string_view>
#include <unordered_map>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2/s2earth.h"
#include "s2geography.h"

namespace s2geography {

namespace arrow_udf {

class InternalUDF : public ArrowUDF {
 public:
  int Init(struct ArrowSchema *arg_schema, std::string_view options,
           struct ArrowSchema *out) override {
    last_error_.clear();
    try {
      if (arg_schema == nullptr || arg_schema->release == nullptr) {
        last_error_ = "Invalid or released arg_schema";
        return EINVAL;
      }

      for (int64_t i = 0; i < arg_schema->n_children; i++) {
        arg_types_.emplace_back(arg_schema->children[i]);
      }

      auto return_type = ReturnType();
      ArrowSchemaMove(return_type.get(), out);

      return GEOARROW_OK;
    } catch (std::exception &e) {
      last_error_ = e.what();
      return EINVAL;
    }
  }

  int Execute(struct ArrowArray **args, int64_t n_args,
              struct ArrowArray *out) override {
    last_error_.clear();
    try {
      std::vector<nanoarrow::UniqueArray> arg_vec;
      for (int64_t i = 0; i < n_args; i++) {
        arg_vec.emplace_back(args[i]);
      }

      auto result = ExecuteImpl(arg_vec);
      ArrowArrayMove(result.get(), out);

      return GEOARROW_OK;
    } catch (std::exception &e) {
      last_error_ = e.what();
      return EINVAL;
    }
  }

  const char *GetLastError() override { return last_error_.c_str(); }

 protected:
  std::vector<nanoarrow::UniqueSchema> arg_types_;
  std::unordered_map<std::string, std::string> options_;

  virtual nanoarrow::UniqueSchema ReturnType() = 0;

  virtual nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) = 0;

 private:
  std::string last_error_;
};

// Combinations that appear
// (geog) -> bool
// (geog) -> int
// (geog) -> double
// (geog) -> geog
// (geog, double) -> geog
// (geog, geog) -> bool
// (geog, geog) -> double
// (geog, geog, double) -> bool
// (geog, geog) -> geog

class OutputBuilder {
 public:
  OutputBuilder(enum ArrowType type) {
    NANOARROW_THROW_NOT_OK(ArrowArrayInitFromType(array_.get(), type));
    NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array_.get()));
  }

  void Reserve(int64_t additional_size) {
    NANOARROW_THROW_NOT_OK(ArrowArrayReserve(array_.get(), additional_size));
  }

  void AppendNull() {
    NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array_.get(), 1));
  }

 protected:
  nanoarrow::UniqueArray array_;
};

template <typename c_type_t, enum ArrowType arrow_type_val>
class ArrowOutputBuilder {
 public:
  using c_type = c_type_t;
  static constexpr enum ArrowType arrow_type = arrow_type_val;

  ArrowOutputBuilder() {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayInitFromType(array_.get(), arrow_type_val));
    NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array_.get()));
  }

  void Reserve(int64_t additional_size) {
    NANOARROW_THROW_NOT_OK(ArrowArrayReserve(array_.get(), additional_size));
  }

  void AppendNull() {
    NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array_.get(), 1));
  }

  void Append(c_type value) {
    if constexpr (std::is_integral_v<c_type>) {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendInt(array_.get(), value));
    } else if constexpr (std::is_floating_point_v<c_type>) {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendDouble(array_.get(), value));
    } else {
      static_assert(false, "value type not supported");
    }
  }

  void Finish(struct ArrowArray *out) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayFinishBuildingDefault(array_.get(), nullptr));
  }

 protected:
  nanoarrow::UniqueArray array_;
};

using BoolOuputBuilder = ArrowOutputBuilder<bool, NANOARROW_TYPE_BOOL>;
using IntOuputBuilder = ArrowOutputBuilder<int32_t, NANOARROW_TYPE_INT32>;
using DoubleOuputBuilder = ArrowOutputBuilder<double, NANOARROW_TYPE_DOUBLE>;

class WkbGeographyOutputBuilder {
 public:
  using c_type = const Geography &;
  static constexpr enum ArrowType arrow_type = NANOARROW_TYPE_BINARY;

  WkbGeographyOutputBuilder() {
    writer.Init(geoarrow::Writer::OutputType::kWKB, geoarrow::ExportOptions());
  }

  void Reserve(int64_t additional_size) {
    // The current geoarrow writer doesn't provide any support for this
  }

  void AppendNull() {
    throw Exception("AppendNull() for geography output not implemented");
  }

  void Append(c_type value) { writer.WriteGeography(value); }

  void Finish(struct ArrowArray *out) { writer.Finish(out); }

 private:
  geoarrow::Writer writer;
};

template <typename c_type_t, enum ArrowType arrow_type_val>
class ArrowInputView {
 public:
  using c_type = const Geography &;
  static constexpr enum ArrowType arrow_type = NANOARROW_TYPE_BINARY;

  ArrowInputView(const struct ArrowSchema *type) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayViewInitFromSchema(view_.get(), type, nullptr));
    if (view_->storage_type != arrow_type) {
      throw Exception("Expected storage type " +
                      std::string(ArrowTypeString(arrow_type)) + " but got " +
                      std::string(ArrowTypeString(view_->storage_type)));
    }
  }

  void SetArray(const struct ArrowArray *array, int64_t num_rows) {
    NANOARROW_THROW_NOT_OK(ArrowArrayViewSetArray(view_.get(), array, nullptr));

    if (array->length == 0) {
      throw Exception("Array input must not be empty");
    }
  }

  bool IsNull(int64_t i) {
    return ArrowArrayViewIsNull(view_.get(), i % view_->length);
  }

  c_type_t Get(int64_t i) {
    if constexpr (std::is_integral_v<c_type>) {
      return ArrowArrayViewGetIntUnsafe(view_.get(), i % view_->length);
    } else if constexpr (std::is_floating_point_v<c_type>) {
      return ArrowArrayViewGetDoubleUnsafe(view_.get(), i % view_->length);
    } else {
      static_assert(false, "value type not supported");
    }
  }

 private:
  nanoarrow::UniqueArrayView view_;
};

class GeographyInputView {
 public:
  using c_type = const Geography &;
  static constexpr enum ArrowType arrow_type = NANOARROW_TYPE_BINARY;

  GeographyInputView(const struct ArrowSchema *type)
      : current_array_(nullptr), stashed_index_(-1) {
    reader_.Init(type);
  }

  void SetArray(const struct ArrowArray *array, int64_t num_rows) {
    current_array_ = array;
    stashed_index_ = -1;
  }

  bool IsNull(int64_t i) {
    StashIfNeeded(i % current_array_->length);
    return stashed_[0].get() == nullptr;
  }

  const Geography &Get(int64_t i) {
    StashIfNeeded(i % current_array_->length);
    return *stashed_[0];
  }

 private:
  geoarrow::Reader reader_;
  const struct ArrowArray *current_array_;
  int64_t stashed_index_;
  std::vector<std::unique_ptr<Geography>> stashed_;

  void StashIfNeeded(int64_t i) {
    if (i != stashed_index_) {
      stashed_.clear();
      reader_.ReadGeography(current_array_, i, 1, &stashed_);
    }
  }
};

class S2Length : public InternalUDF {
 protected:
  nanoarrow::UniqueSchema ReturnType() override {
    if (arg_types_.size() != 1) {
      throw Exception("Expected one argument in S2Length");
    }

    auto geometry = ::geoarrow::GeometryDataType::Make(arg_types_[0].get());
    if (geometry.edge_type() != GEOARROW_EDGE_TYPE_SPHERICAL) {
      throw Exception("Expected input with spherical edges");
    }

    nanoarrow::UniqueSchema out;
    NANOARROW_THROW_NOT_OK(
        ArrowSchemaInitFromType(out.get(), NANOARROW_TYPE_DOUBLE));
    return out;
  }

  nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) override {
    if (args.size() != 1 || arg_types_.size() != 1) {
      throw Exception(
          "Expected one argument/one argument type in S2Length::Execute()");
    }

    auto reader = geoarrow::Reader();
    reader.Init(arg_types_[0].get());

    nanoarrow::UniqueArray out;
    NANOARROW_THROW_NOT_OK(
        ArrowArrayInitFromType(out.get(), NANOARROW_TYPE_DOUBLE));
    NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(out.get()));
    std::vector<std::unique_ptr<Geography>> geogs;
    for (int64_t i = 0; i < args[0]->length; i++) {
      geogs.clear();
      reader.ReadGeography(args[0].get(), i, 1, &geogs);
      if (geogs[0]) {
        double value = s2_length(*geogs[0]) * S2Earth::RadiusMeters();
        NANOARROW_THROW_NOT_OK(ArrowArrayAppendDouble(out.get(), value));
      } else {
        NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(out.get(), 1));
      }
    }

    NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(out.get(), nullptr));
    return out;
  }
};

std::unique_ptr<ArrowUDF> Length() { return std::make_unique<S2Length>(); }

}  // namespace arrow_udf

}  // namespace s2geography
