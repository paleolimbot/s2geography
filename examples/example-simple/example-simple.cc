
#include <stdio.h>

#include "s2geography.h"

using namespace s2geography;

int main(int argc, char *argv[]) {
  WKTReader reader;
  std::unique_ptr<S2Geography> point1 = reader.read_feature("POINT (-64 45)");
  std::unique_ptr<S2Geography> point2 = reader.read_feature("POINT (0 45)");

  ShapeIndexGeography point1_index(*point1);
  ShapeIndexGeography point2_index(*point2);

  double dist = s2_distance(point1_index, point2_index);

  printf("distance result is %g\n", dist);
}
