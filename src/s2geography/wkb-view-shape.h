
#include <s2/s2shape.h>

class WkbViewShape : public S2Shape {
 public:
  WkbViewShape(int dimension, void* geometry);
  int num_edges() const;
  Edge edge(int edge_id) const;
  int dimension() const;
  ReferencePoint GetReferencePoint() const;
  int num_chains() const;
  Chain chain(int chain_id) const;
  Edge chain_edge(int chain_id, int offset) const;
  ChainPosition chain_position(int edge_id) const;

 private:
  int dimension_;
  void* geometry_;
};
