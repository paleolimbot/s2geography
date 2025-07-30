
#pragma once

#include "s2geography/arrow_udf/arrow_udf.h"
#include "s2geography/geography.h"

namespace s2geography {

bool s2_is_collection(const Geography& geog);
int s2_dimension(const Geography& geog);
int s2_num_points(const Geography& geog);
bool s2_is_empty(const Geography& geog);
double s2_area(const Geography& geog);
double s2_length(const Geography& geog);
double s2_perimeter(const Geography& geog);
double s2_x(const Geography& geog);
double s2_y(const Geography& geog);
bool s2_find_validation_error(const Geography& geog, S2Error* error);

namespace arrow_udf {
/// \brief Instantiate an ArrowUDF for the s2_length() function
///
/// This ArrowUDF handles any GeoArrow array as input and produces
/// a double array as output. Note that unlike s2_length(), this
/// function returns results in meters by default.
std::unique_ptr<ArrowUDF> Length();
std::unique_ptr<ArrowUDF> Area();
std::unique_ptr<ArrowUDF> Perimeter();
}  // namespace arrow_udf

}  // namespace s2geography
