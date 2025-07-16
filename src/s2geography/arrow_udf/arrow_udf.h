#pragma once

#include <cerrno>
#include <memory>
#include <string_view>

#include "s2geography/arrow_abi.h"

namespace s2geography {

namespace arrow_udf {

class ArrowUDF {
 public:
  virtual ~ArrowUDF() {}
  virtual int Init(struct ArrowSchema* arg_schema, std::string_view options,
                   struct ArrowSchema* out) = 0;
  virtual int Execute(struct ArrowArray** args, int64_t n_args,
                      struct ArrowArray* out) = 0;
  virtual const char* GetLastError() = 0;
};

std::unique_ptr<ArrowUDF> Length();
std::unique_ptr<ArrowUDF> Centroid();
std::unique_ptr<ArrowUDF> InterpolateNormalized();
std::unique_ptr<ArrowUDF> Intersects();

}  // namespace arrow_udf

}  // namespace s2geography
