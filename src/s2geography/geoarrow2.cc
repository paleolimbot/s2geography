
#include <s2/s1angle.h>
#include <s2/s2edge_tessellator.h>

#include <sstream>

#include "geoarrow/geoarrow.h"
#include "geoarrow/geoarrow.hpp"
#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"
#include "s2geography/macros.h"

using XY = geoarrow::array_util::XY<double>;
using XYZ = geoarrow::array_util::XYZ<double>;
using XYSequence = geoarrow::array_util::CoordSequence<XY>;
using XYZSequence = geoarrow::array_util::CoordSequence<XYZ>;
using WKBArray = geoarrow::wkb_util::WKBArray<int32_t>;
using geoarrow::wkb_util::WKBGeometry;
using geoarrow::wkb_util::WKBSequence;

namespace s2geography {

namespace geoarrow {

/// \brief Import/export points as literal XYZ values
///
/// This is the translator that most faithfully represents its input (by not
/// applying any projection whatsoever and keeping the unit vector point
/// representation).
///
/// This translator igores the value of tessellator and the projection.
template <typename Sequence>
struct LiteralTranslator {
  static S2Point ImportPoint(typename Sequence::value_type coord,
                             const S2::Projection* projection) {
    S2_DCHECK_EQ(projection, nullptr);
    return {coord.x(), coord.y(), coord.z()};
  }

  static void ImportSequence(const Sequence& seq, std::vector<S2Point>* out,
                             const S2EdgeTessellator& tessellator,
                             const S2::Projection* projection) {
    // Could maybe use a memcpy here
    out->reserve(out->size() + seq.size());
    for (const auto coord : seq) {
      out->push_back({coord.x(), coord.y(), coord.z()});
    }
  }
};

/// \brief Import/export vertices according to projection
///
/// Translates between S2Points and projected space by applying a projection
/// to each point in isolation. This is the transformation, for example, that
/// would be appropriate for WKB or GeoArrow annotated with spherical edges.
///
/// This translator igores the value of tessellator.
template <typename Sequence>
struct ProjectedTranslator {
  static S2Point ImportPoint(typename Sequence::value_type coord,
                             const S2::Projection* projection) {
    return projection->Unproject({coord.x(), coord.y()});
  }

  static void ImportSequence(const Sequence& seq, std::vector<S2Point>* out,
                             const S2EdgeTessellator& tessellator,
                             const S2::Projection* projection) {
    out->reserve(out->size() + seq.size());
    for (const auto coord : seq) {
      out->push_back(projection->Unproject({coord.x(), coord.y()}));
    }
  }
};

/// \brief Import/export planar edges from/to projected space
///
/// Transforms vertices according to projection but also ensures that planar
/// edges are tessellated to keep them within a specified tolerance of their
/// original positions in projected space (when importing) or spherical space
/// (when exporting).
template <typename Sequence>
struct TessellatedTranslator {
  static S2Point ImportPoint(typename Sequence::value_type coord,
                             const S2::Projection* projection) {
    return projection->Unproject({coord.x(), coord.y()});
  }

  static void ImportSequence(const Sequence& seq, std::vector<S2Point>* out,
                             const S2EdgeTessellator& tessellator,
                             const S2::Projection* projection) {
    out->reserve(out->size() + seq.size());
    seq.template VisitEdges<XY>([&](XY v0, XY v1) {
      tessellator.AppendUnprojected({v0.x(), v0.y()}, {v1.x(), v1.y()}, out);
    });
  }
};

class ReaderImpl {
 public:
  virtual ~ReaderImpl() = default;
  virtual void VisitConst(struct ArrowArray* array,
                          GeographyVisitor&& visitor) = 0;
  virtual void ReadGeography(struct ArrowArray* array, int64_t offset,
                             int64_t length,
                             std::vector<std::unique_ptr<Geography>>* out) = 0;

 protected:
  ::geoarrow::ArrayReader reader_;
  ImportOptions options_;
  S2::Projection* projection_;
  S2EdgeTessellator tessellator_;

  ReaderImpl(enum GeoArrowType type, const ImportOptions& options)
      : reader_(type),
        options_(options),
        projection_(options_.projection()),
        tessellator_(options_.projection(), options_.tessellate_tolerance()) {}
  ReaderImpl(const struct ArrowSchema* schema, const ImportOptions& options)
      : reader_(schema),
        options_(options),
        projection_(options_.projection()),
        tessellator_(options_.projection(), options_.tessellate_tolerance()) {}

};

using GeographyVisitor = std::function<void(std::optional<const Geography&>)>;


class WKBReaderImpl : public ReaderImpl {
 public:
  explicit WKBReaderImpl(const ImportOptions& options)
      : ReaderImpl(GEOARROW_TYPE_WKB, options) {}

  void VisitConst(struct ArrowArray* array, GeographyVisitor&& visitor) {
    reader_.SetArray(array);
  }

  void ReadGeography(struct ArrowArray* array, int64_t offset, int64_t length,
                     std::vector<std::unique_ptr<Geography>>* out) {
    reader_.SetArray(array);
  }

 private:
  WKBGeometry geometry_;
  WKBArray wkb_array_;
};

}  // namespace geoarrow

}  // namespace s2geography
