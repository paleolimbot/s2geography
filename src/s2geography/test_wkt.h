
#include <vector>

static const char* kRoundtrippableWkt[] = {
    "POINT (30 10)",
    "POINT EMPTY",
    "LINESTRING (30 10, 10 30, 40 40)",
    "LINESTRING EMPTY",
    "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))",
    "POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10), (20 30, 35 35, 30 20, 20 "
    "30))",
    "POLYGON EMPTY",
    "MULTIPOINT ((10 40), (40 30), (20 20), (30 10))",
    "MULTILINESTRING ((10 10, 20 20, 10 40), (40 40, 30 30, 40 20, 30 10))",
    "MULTIPOLYGON (((30 20, 45 40, 10 40, 30 20)), ((15 5, 40 10, 10 20, 5 10, "
    "15 5)))",
    "MULTIPOLYGON (((40 40, 20 45, 45 30, 40 40)), ((20 35, 10 30, 10 10, 30 "
    "5, 45 20, 20 35), (30 20, 20 15, 20 25, 30 20)))",
    "GEOMETRYCOLLECTION (POINT (30 10))",
    "GEOMETRYCOLLECTION (LINESTRING (30 10, 10 30, 40 40))",
    "GEOMETRYCOLLECTION (POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "GEOMETRYCOLLECTION (POINT (30 10), LINESTRING (30 10, 10 30, 40 40), "
    "POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (POINT (30 10)))",
    "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (LINESTRING (30 10, 10 30, 40 "
    "40)))",
    "GEOMETRYCOLLECTION (GEOMETRYCOLLECTION (POLYGON ((30 10, 40 40, 20 40, 10 "
    "20, 30 10))))",
    "GEOMETRYCOLLECTION EMPTY"};

static int kNumRoundtrippableWkt =
    sizeof(kRoundtrippableWkt) / sizeof(const char*);

static const char* kNonRoundtrippableWkt[] = {
    "MULTIPOINT ((30 10))",
    "MULTIPOINT EMPTY",
    "MULTILINESTRING ((30 10, 10 30, 40 40))",
    "MULTILINESTRING EMPTY",
    "MULTIPOLYGON (((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "MULTIPOLYGON EMPTY",
};
static int kNumNonRoundtrippableWkt =
    sizeof(kNonRoundtrippableWkt) / sizeof(const char*);

static const char* kNonRoundtrippableWktRoundtrip[] = {
    "POINT (30 10)",
    "POINT EMPTY",
    "LINESTRING (30 10, 10 30, 40 40)",
    "LINESTRING EMPTY",
    "POLYGON (((30 10, 40 40, 20 40, 10 20, 30 10)))",
    "MULTIPOLYGON EMPTY",
};

static inline std::vector<std::string> TestWKT() {
  std::vector<std::string> out(kNumRoundtrippableWkt);
  for (int i = 0; i < kNumRoundtrippableWkt; i++) {
    out[i] = kRoundtrippableWkt[i];
  }
  return out;
}
