
#include <stdio.h>

#include "s2geography.h"

using namespace s2geography;

int main(int argc, char *argv[]) {
  PointGeography point1 = S2LatLng::FromDegrees(45, -64).ToPoint();
  PointGeography point2 = S2LatLng::FromDegrees(45, 0).ToPoint();

  ShapeIndexGeography point1_index(point1);
  ShapeIndexGeography point2_index(point2);

  double dist = s2_distance(point1_index, point2_index);

  printf("distance result is %g", dist);
}
