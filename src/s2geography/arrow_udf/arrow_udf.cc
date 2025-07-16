
#include "s2geography/arrow_udf/arrow_udf.h"

#include <cerrno>
#include <optional>
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
