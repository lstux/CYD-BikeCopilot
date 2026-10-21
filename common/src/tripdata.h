#pragma once
// Données d'un trajet et fonctions de date, sans dépendance au matériel (aussi utilisé par le simulateur).
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------------------
// Dates
// ---------------------------------------------------------------------------------------
static inline int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static inline void civilFromDays(int64_t z, int &y, int &m, int &d) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t yy = (int64_t)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yy + (m <= 2));
}

static inline uint32_t epochFromDate(int y, int mo, int d, int h, int mi, int s) {
  return (uint32_t)(daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s);
}

// Décompose un epoch UTC décalé de tz heures
static inline void localParts(uint32_t epoch, int tz, int &y, int &mo, int &d, int &h, int &mi, int &s) {
  int64_t t = (int64_t)epoch + (int64_t)tz * 3600;
  int64_t days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
  int64_t sec = t - days * 86400;
  civilFromDays(days, y, mo, d);
  h = (int)(sec / 3600);
  mi = (int)((sec % 3600) / 60);
  s = (int)(sec % 60);
}

// ---------------------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------------------
struct TripSummary {
  char name[24] = "";  // AAAAMMJJ_HHMMSS
  uint32_t t0 = 0;
  float distM = 0;
  uint32_t durS = 0, movS = 0;
  float maxKmh = 0, avgKmh = 0;
  int16_t up = 0, down = 0;
};

struct TripSeries {
  static const int N = 240;
  float spd[N];
  float alt[N];
  float spdMax = 10, altMin = 0, altMax = 10;
};

struct LogSample {
  uint32_t epoch = 0;
  double lat = 0, lon = 0;
  float alt = 0, kmh = 0, hdop = 0;
  uint8_t sats = 0;
};

