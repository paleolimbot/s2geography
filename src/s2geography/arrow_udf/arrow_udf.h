#pragma once

#include <cerrno>
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

}  // namespace arrow_udf

}  // namespace s2geography
