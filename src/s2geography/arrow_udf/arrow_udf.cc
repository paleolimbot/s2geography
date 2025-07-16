
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
    ArrowArrayMove(array_.get(), out);
  }

 protected:
  nanoarrow::UniqueArray array_;
};

using BoolOutputBuilder = ArrowOutputBuilder<bool, NANOARROW_TYPE_BOOL>;
using IntOutputBuilder = ArrowOutputBuilder<int32_t, NANOARROW_TYPE_INT32>;
using DoubleOutputBuilder = ArrowOutputBuilder<double, NANOARROW_TYPE_DOUBLE>;

class WkbGeographyOutputBuilder {
 public:
  using c_type = const Geography &;
  static constexpr enum ArrowType arrow_type = NANOARROW_TYPE_BINARY;

  WkbGeographyOutputBuilder() {
    writer_.Init(geoarrow::Writer::OutputType::kWKB, geoarrow::ExportOptions());
  }

  void Reserve(int64_t additional_size) {
    // The current geoarrow writer doesn't provide any support for this
  }

  void AppendNull() { writer_.WriteNull(); }

  void Append(c_type value) { writer_.WriteGeography(value); }

  void Finish(struct ArrowArray *out) { writer_.Finish(out); }

 private:
  geoarrow::Writer writer_;
};

template <typename c_type_t>
class ArrowInputView {
 public:
  using c_type = c_type_t;

  ArrowInputView(const struct ArrowSchema *type) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayViewInitFromSchema(view_.get(), type, nullptr));
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
    if constexpr (std::is_integral_v<c_type_t>) {
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

using BoolInputView = ArrowInputView<bool>;
using IntInputView = ArrowInputView<int64_t>;
using DoubleInputView = ArrowInputView<double>;

class GeographyInputView {
 public:
  using c_type = const Geography &;

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

template <typename Exec>
class UnaryUDF : public InternalUDF {
 protected:
  using arg0_t = typename Exec::arg0_t;
  using out_t = typename Exec::out_t;

  nanoarrow::UniqueSchema ReturnType() override {
    // May need to update this if we have a "unary" UDF that can
    // accept an optional constant argument via the options
    if (arg_types_.size() != 1) {
      throw Exception("Expected one argument in unary s2geography UDF");
    }

    arg0 = std::make_unique<arg0_t>(arg_types_[0].get());
    out = std::make_unique<out_t>();
    exec.Init(options_);

    nanoarrow::UniqueSchema out_type;
    NANOARROW_THROW_NOT_OK(
        ArrowSchemaInitFromType(out_type.get(), out_t::arrow_type));
    return out_type;
  }

  nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) override {
    if (args.size() != 1 || arg_types_.size() != 1) {
      throw Exception(
          "Expected one argument/one argument type in in unary s2geography "
          "UDF");
    }

    int64_t num_rows = args[0]->length;
    arg0->SetArray(args[0].get(), num_rows);

    for (int i = 0; i < num_rows; i++) {
      if (arg0->IsNull(i)) {
        out->AppendNull();
      } else {
        typename Exec::arg0_t::c_type item0 = arg0->Get(i);
        typename Exec::out_t::c_type item_out = exec.Exec(item0);
        out->Append(item_out);
      }
    }

    nanoarrow::UniqueArray array_out;
    out->Finish(array_out.get());
    return array_out;
  }

 private:
  std::unique_ptr<arg0_t> arg0;
  std::unique_ptr<out_t> out;
  Exec exec;
};

template <typename Exec>
class BinaryUDF : public InternalUDF {
 protected:
  using arg0_t = typename Exec::arg0_t;
  using arg1_t = typename Exec::arg1_t;
  using out_t = typename Exec::out_t;

  nanoarrow::UniqueSchema ReturnType() override {
    // May need to update this if we have a "binary" UDF that can
    // accept an optional constant argument via the options
    if (arg_types_.size() != 2) {
      throw Exception("Expected one argument in unary s2geography UDF");
    }

    arg0 = std::make_unique<arg0_t>(arg_types_[0].get());
    arg1 = std::make_unique<arg1_t>(arg_types_[1].get());
    out = std::make_unique<out_t>();
    exec.Init(options_);

    nanoarrow::UniqueSchema out_type;
    NANOARROW_THROW_NOT_OK(
        ArrowSchemaInitFromType(out_type.get(), out_t::arrow_type));
    return out_type;
  }

  nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) override {
    if (args.size() != 2 || arg_types_.size() != 2) {
      throw Exception(
          "Expected one argument/one argument type in in unary s2geography "
          "UDF");
    }

    int64_t num_rows = 1;
    for (const auto &arg : args) {
      if (arg->length != 1) {
        num_rows = arg->length;
        break;
      }
    }

    arg0->SetArray(args[0].get(), num_rows);

    for (int i = 0; i < num_rows; i++) {
      if (arg0->IsNull(i) || arg1->IsNull(i)) {
        out->AppendNull();
      } else {
        typename Exec::arg0_t::c_type item0 = arg0->Get(i);
        typename Exec::arg1_t::c_type item1 = arg1->Get(i);
        typename Exec::out_t::c_type item_out = exec.Exec(item0, item1);
        out->Append(item_out);
      }
    }

    nanoarrow::UniqueArray array_out;
    out->Finish(array_out.get());
    return array_out;
  }

 private:
  std::unique_ptr<arg0_t> arg0;
  std::unique_ptr<arg1_t> arg1;
  std::unique_ptr<out_t> out;
  Exec exec;
};

struct S2InterpolateNormalizedExec {
  using arg0_t = GeographyInputView;
  using arg1_t = DoubleInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value0, arg1_t::c_type value1) {
    stashed_ = s2_interpolate_normalized(value0, value1);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> InterpolateNormalized() {
  return std::make_unique<BinaryUDF<S2InterpolateNormalizedExec>>();
}

struct S2LengthExec {
  using arg0_t = GeographyInputView;
  using out_t = DoubleOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  out_t::c_type Exec(arg0_t::c_type value) {
    return s2_length(value) * S2Earth::RadiusMeters();
  }
};

std::unique_ptr<ArrowUDF> Length() {
  return std::make_unique<UnaryUDF<S2LengthExec>>();
}

struct S2CentroidExec {
  using arg0_t = GeographyInputView;
  using out_t = WkbGeographyOutputBuilder;

  void Init(const std::unordered_map<std::string, std::string> &options) {}

  const Geography &Exec(const Geography &value) {
    S2Point out = s2_centroid(value);
    stashed_ = PointGeography(out);
    return stashed_;
  }

  PointGeography stashed_;
};

std::unique_ptr<ArrowUDF> Centroid() {
  return std::make_unique<UnaryUDF<S2CentroidExec>>();
}

}  // namespace arrow_udf

}  // namespace s2geography
