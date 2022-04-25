
#include <stdio.h>
#include <iostream>

#include "s2geography.h"

using namespace s2geography;

int main(int argc, char *argv[]) {
  WKTReader reader;
  std::unique_ptr<S2Geography> geog1 = reader.read_feature("POINT (-64 45)");
  std::unique_ptr<S2Geography> geog2 = reader.read_feature("POINT (0 45)");

  ShapeIndexGeography geog1_index(*geog1);
  ShapeIndexGeography geog2_index(*geog2);

  double dist = s2_distance(geog1_index, geog2_index);

  printf("distance result is %g\n", dist);

  WKTWriter writer;
  std::cout << "geog1: " << writer.write_feature(*geog1) << "\n";
  std::cout << "geog2: " << writer.write_feature(*geog2) << "\n";
}
