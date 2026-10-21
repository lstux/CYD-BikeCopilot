#pragma once
// Projection Web Mercator (celle des tuiles OpenStreetMap).
#include <math.h>
#include <stdint.h>

// Position en pixels "monde" au zoom z (tuiles de 256 px)
static inline void worldPx(double lat, double lon, int z, double &x, double &y) {
  double n = 256.0 * (double)(1UL << z);
  x = (lon + 180.0) / 360.0 * n;
  double s = sin(lat * M_PI / 180.0);
  y = (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI)) * n;
}
