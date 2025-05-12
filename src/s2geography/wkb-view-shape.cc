
#include "s2geography/wkb-view-shape.h"

WkbViewShape::WkbViewShape(int dimension, void* geometry)
    : dimension_(dimension), geometry_(geometry) {}

int WkbViewShape::num_edges() const { return 0; }

S2Shape::Edge WkbViewShape::edge(int edge_id) const { return {}; }

int WkbViewShape::dimension() const { return 0; }

S2Shape::ReferencePoint WkbViewShape::GetReferencePoint() const { return {}; }

int WkbViewShape::num_chains() const { return 0; }

S2Shape::Chain WkbViewShape::chain(int chain_id) const { return {}; }

S2Shape::Edge WkbViewShape::chain_edge(int chain_id, int offset) const {
  return {};
}

S2Shape::ChainPosition WkbViewShape::chain_position(int edge_id) const {
  return {};
}
