#pragma once
// Géométrie : projection Web Mercator (celle des tuiles OpenStreetMap) et distance entre deux points.
#include <math.h>
#include <stdint.h>

// Position en pixels "monde" au zoom z (tuiles de 256 px)
static inline void worldPx(double lat, double lon, int z, double &x, double &y) {
  double n = 256.0 * (double)(1UL << z);
  x = (lon + 180.0) / 360.0 * n;
  double s = sin(lat * M_PI / 180.0);
  y = (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI)) * n;
}

// Distance en mètres entre deux points (grand cercle). Même formule et même rayon que
// TinyGPSPlus::distanceBetween : les résultats sont identiques, sans dépendre de cette bibliothèque.
static inline double distanceM(double lat1, double lon1, double lat2, double lon2) {
  double d = (lon1 - lon2) * M_PI / 180.0;
  double sdl = sin(d), cdl = cos(d);
  double a1 = lat1 * M_PI / 180.0, a2 = lat2 * M_PI / 180.0;
  double s1 = sin(a1), c1 = cos(a1), s2 = sin(a2), c2 = cos(a2);
  double y = (c1 * s2) - (s1 * c2 * cdl);
  y = y * y + (c2 * sdl) * (c2 * sdl);
  double x = (s1 * s2) + (c1 * c2 * cdl);
  return atan2(sqrt(y), x) * 6372795.0;
}

// Cap initial en degrés (0..360, 0 = nord) pour aller du point 1 au point 2.
// Même formule que TinyGPSPlus::courseTo.
static inline double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
  double dlon = (lon2 - lon1) * M_PI / 180.0;
  double a1 = lat1 * M_PI / 180.0, a2 = lat2 * M_PI / 180.0;
  double y = sin(dlon) * cos(a2);
  double x = cos(a1) * sin(a2) - sin(a1) * cos(a2) * cos(dlon);
  double r = atan2(y, x);
  if (r < 0.0) r += 2.0 * M_PI;
  return r * 180.0 / M_PI;
}
