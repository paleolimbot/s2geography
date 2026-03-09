
#include "s2geography/geography_interface.h"

#include <s2/s2cell_id.h>

namespace s2geography {

int s2geography::Geography::dimension() const {
  if (num_shapes() == 0) {
    return -1;
  }

  int dim = Shape(0)->dimension();
  for (int i = 1; i < num_shapes(); i++) {
    if (dim != Shape(i)->dimension()) {
      return -1;
    }
  }

  return dim;
}

void EncodeTag::Encode(Encoder* encoder) const {
  encoder->Ensure(4 * sizeof(uint8_t));
  encoder->put8(static_cast<uint8_t>(kind));
  encoder->put8(flags);
  encoder->put8(covering_size);
  encoder->put8(reserved);
}

void EncodeTag::Decode(Decoder* decoder) {
  if (decoder->avail() < 4 * sizeof(uint8_t)) {
    throw Exception(
        "EncodeTag::Decode() fewer than 4 bytes available in decoder");
  }

  uint8_t geography_type = decoder->get8();

  if (geography_type == static_cast<uint8_t>(GeographyKind::POINT)) {
    kind = GeographyKind::POINT;
  } else if (geography_type == static_cast<uint8_t>(GeographyKind::POLYLINE)) {
    kind = GeographyKind::POLYLINE;
  } else if (geography_type == static_cast<uint8_t>(GeographyKind::POLYGON)) {
    kind = GeographyKind::POLYGON;
  } else if (geography_type ==
             static_cast<uint8_t>(GeographyKind::GEOGRAPHY_COLLECTION)) {
    kind = GeographyKind::GEOGRAPHY_COLLECTION;
  } else if (geography_type ==
             static_cast<uint8_t>(GeographyKind::SHAPE_INDEX)) {
    kind = GeographyKind::SHAPE_INDEX;
  } else if (geography_type ==
             static_cast<uint8_t>(GeographyKind::CELL_CENTER)) {
    kind = GeographyKind::CELL_CENTER;

  } else {
    throw Exception("EncodeTag::Decode(): Unknown geography kind identifier " +
                    std::to_string(geography_type));
  }

  flags = decoder->get8();
  covering_size = decoder->get8();
  reserved = decoder->get8();
  Validate();
}

void EncodeTag::DecodeCovering(Decoder* decoder,
                               std::vector<S2CellId>* cell_ids) const {
  if (decoder->avail() < (covering_size * sizeof(uint64_t))) {
    throw Exception("Insufficient size in decoder for " +
                    std::to_string(covering_size) + " cell ids");
  }

  cell_ids->resize(covering_size);
  for (uint8_t i = 0; i < covering_size; i++) {
    cell_ids->at(i) = S2CellId(decoder->get64());
  }
}

void s2geography::EncodeTag::SkipCovering(Decoder* decoder) const {
  if (decoder->avail() < (covering_size * sizeof(uint64_t))) {
    throw Exception("Insufficient size in decoder for " +
                    std::to_string(covering_size) + " cell ids");
  }

  decoder->skip(covering_size * sizeof(uint64_t));
}

void EncodeTag::Validate() {
  if (reserved != 0) {
    throw Exception("EncodeTag: reserved byte must be zero");
  }

  uint8_t flags_validate = flags & ~kFlagEmpty;
  if (flags_validate != 0) {
    throw Exception("EncodeTag: Unknown flag(s)");
  }
}

}  // namespace s2geography
