
#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

namespace s2geography {

class WKTReader {
 public:
  WKTReader() : WKTReader(geoarrow::ImportOptions()) {}
  WKTReader(const geoarrow::ImportOptions& options);
  std::unique_ptr<Geography> read_feature(const char* text, int64_t size);
  std::unique_ptr<Geography> read_feature(const char* text);
  std::unique_ptr<Geography> read_feature(const std::string& str);

 private:
  std::unique_ptr<geoarrow::Reader> reader_;
  std::vector<std::unique_ptr<Geography>> out_;
};

}  // namespace s2geography
