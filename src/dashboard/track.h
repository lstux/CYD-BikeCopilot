#pragma once
// Trace GPX : chargement depuis la carte SD, suivi de position, prochain virage.
#include <Arduino.h>
#include <SD.h>
#include <functional>
#include <new>
#include <stdlib.h>
#include <string.h>
#include "geo.h"

#define TURN_MIN_DEG   35.0f  // angle minimum pour parler de virage
#define TURN_WINDOW_M  25.0f  // longueur de trace de chaque côté pour mesurer l'angle

class Track {
 public:
  ~Track() { release(); }

  // Charge les points <trkpt> (ou <rtept> à défaut) d'un fichier GPX.
  // Si le fichier a plus de maxPts points, il est sous-échantillonné.
  bool load(const char *path, int maxPts = 2500) {
    release();
    const char *tag = "trkpt";
    int total = 0;
    forEachPoint(path, tag, [&](double, double) { total++; });
    if (total < 2) {
      tag = "rtept";
      total = 0;
      forEachPoint(path, tag, [&](double, double) { total++; });
    }
    if (total < 2) return false;

    int stride = (total + maxPts - 1) / maxPts;
    int cap = (total + stride - 1) / stride + 1;
    lat_ = new (std::nothrow) float[cap];
    lon_ = new (std::nothrow) float[cap];
    cum_ = new (std::nothrow) float[cap];
    wx_ = new (std::nothrow) int32_t[cap];
    wy_ = new (std::nothrow) int32_t[cap];
    if (!lat_ || !lon_ || !cum_ || !wx_ || !wy_) {
      release();
      return false;
    }

    int i = 0, k = 0;
    double lastLa = 0, lastLo = 0;
    forEachPoint(path, tag, [&](double la, double lo) {
      if (i % stride == 0 && k < cap) {
        lat_[k] = (float)la;
        lon_[k] = (float)lo;
        k++;
      }
      lastLa = la;
      lastLo = lo;
      i++;
    });
    if ((i - 1) % stride != 0 && k < cap) {  // toujours garder le dernier point
      lat_[k] = (float)lastLa;
      lon_[k] = (float)lastLo;
      k++;
    }
    n_ = k;
    cum_[0] = 0;
    for (int j = 1; j < n_; j++) cum_[j] = cum_[j - 1] + segLen(j - 1);
    idx_ = 0;
    along_ = 0;
    return n_ >= 2;
  }

  bool valid() const { return n_ >= 2; }
  int count() const { return n_; }
  double destLat() const { return lat_[n_ - 1]; }
  double destLon() const { return lon_[n_ - 1]; }
  float totalM() const { return n_ ? cum_[n_ - 1] : 0; }
  float remainingM() const { return totalM() - along_; }
  float offRouteM() const { return off_; }

  // Positions en pixels monde pour un zoom donné
  void project(int z) {
    for (int i = 0; i < n_; i++) {
      double x, y;
      worldPx(lat_[i], lon_[i], z, x, y);
      wx_[i] = (int32_t)x;
      wy_[i] = (int32_t)y;
    }
  }
  int32_t wx(int i) const { return wx_[i]; }
  int32_t wy(int i) const { return wy_[i]; }

  // Cherche le point de la trace le plus proche de la position (distance en m).
  // Cherche d'abord autour du dernier point connu, puis sur toute la trace si l'on est perdu.
  float follow(double lat, double lon) {
    if (n_ < 2) return 1e9f;
    int lo = idx_ - 10 < 0 ? 0 : idx_ - 10;
    int hi = idx_ + 80 > n_ - 2 ? n_ - 2 : idx_ + 80;
    float best = scan(lat, lon, lo, hi);
    if (best > 100.0f) best = scan(lat, lon, 0, n_ - 2);
    off_ = best;
    return best;
  }

  // Prochain virage : distance (m) et angle signé (+ droite, - gauche)
  bool nextTurn(float &distM, float &angleDeg) const {
    if (n_ < 3) return false;
    for (int j = idx_ + 1; j < n_ - 1; j++) {
      float d = cum_[j] - along_;
      if (d > 3000.0f) break;
      float a = turnAt(j);
      if (fabsf(a) < TURN_MIN_DEG) continue;
      // Une courbe s'étale sur plusieurs points : on garde le plus fort angle des 30 m suivants
      float bestA = a, bestD = d;
      for (int k = j + 1; k < n_ - 1 && cum_[k] - cum_[j] < 30.0f; k++) {
        float ak = turnAt(k);
        if (fabsf(ak) > fabsf(bestA)) {
          bestA = ak;
          bestD = cum_[k] - along_;
        }
      }
      distM = bestD < 0 ? 0 : bestD;
      angleDeg = bestA;
      return true;
    }
    return false;
  }

 private:
  float *lat_ = nullptr, *lon_ = nullptr, *cum_ = nullptr;
  int32_t *wx_ = nullptr, *wy_ = nullptr;
  int n_ = 0, idx_ = 0;
  float along_ = 0, off_ = 0;

  void release() {
    delete[] lat_;
    delete[] lon_;
    delete[] cum_;
    delete[] wx_;
    delete[] wy_;
    lat_ = lon_ = cum_ = nullptr;
    wx_ = wy_ = nullptr;
    n_ = 0;
  }

  float segLen(int i) const {
    float kLon = 111320.0f * cosf(lat_[i] * (float)M_PI / 180.0f);
    float dx = (lon_[i + 1] - lon_[i]) * kLon;
    float dy = (lat_[i + 1] - lat_[i]) * 110574.0f;
    return sqrtf(dx * dx + dy * dy);
  }

  float bearing(int i, int j) const {
    float kLon = cosf(lat_[i] * (float)M_PI / 180.0f);
    float dx = (lon_[j] - lon_[i]) * kLon;
    float dy = lat_[j] - lat_[i];
    return atan2f(dx, dy) * 180.0f / (float)M_PI;
  }

  float turnAt(int j) const {
    int a = j, b = j;
    while (a > 0 && cum_[j] - cum_[a] < TURN_WINDOW_M) a--;
    while (b < n_ - 1 && cum_[b] - cum_[j] < TURN_WINDOW_M) b++;
    if (a == j || b == j) return 0;
    float d = bearing(j, b) - bearing(a, j);
    while (d > 180) d -= 360;
    while (d < -180) d += 360;
    return d;
  }

  float scan(double lat, double lon, int lo, int hi) {
    float kLat = 110574.0f;
    float kLon = 111320.0f * cosf((float)lat * (float)M_PI / 180.0f);
    float best = 1e9f;
    int bestI = idx_;
    float bestT = 0;
    for (int i = lo; i <= hi; i++) {
      float ax = (float)(lon_[i] - lon) * kLon, ay = (float)(lat_[i] - lat) * kLat;
      float bx = (float)(lon_[i + 1] - lon) * kLon, by = (float)(lat_[i + 1] - lat) * kLat;
      float dx = bx - ax, dy = by - ay;
      float l2 = dx * dx + dy * dy;
      float t = l2 > 0 ? -(ax * dx + ay * dy) / l2 : 0;
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      float px = ax + t * dx, py = ay + t * dy;
      float d = sqrtf(px * px + py * py);
      if (d < best) {
        best = d;
        bestI = i;
        bestT = t;
      }
    }
    idx_ = bestI;
    along_ = cum_[bestI] + bestT * (cum_[bestI + 1] - cum_[bestI]);
    return best;
  }

  static bool attr(const char *tag, const char *name, double &out) {
    size_t nl = strlen(name);
    for (const char *p = tag; *p; p++) {
      bool start = (p == tag) || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n' || p[-1] == '\r';
      if (start && strncmp(p, name, nl) == 0 && p[nl] == '=') {
        const char *v = p + nl + 1;
        if (*v == '"' || *v == '\'') v++;
        char *end;
        out = strtod(v, &end);
        return end != v;
      }
    }
    return false;
  }

  // Parcourt le fichier et appelle cb(lat, lon) pour chaque balise <tag lat=".." lon="..">
  static void forEachPoint(const char *path, const char *tag,
                           const std::function<void(double, double)> &cb) {
    File f = SD.open(path, FILE_READ);
    if (!f) return;
    char buf[512], tagbuf[200];
    int tl = 0;
    bool inTag = false;
    size_t taglen = strlen(tag);
    while (true) {
      int n = f.read((uint8_t *)buf, sizeof(buf));
      if (n <= 0) break;
      for (int i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '<') {
          inTag = true;
          tl = 0;
        } else if (inTag) {
          if (c == '>') {
            inTag = false;
            tagbuf[tl] = 0;
            if (strncmp(tagbuf, tag, taglen) == 0 &&
                (tagbuf[taglen] == ' ' || tagbuf[taglen] == '\t' || tagbuf[taglen] == '\n' ||
                 tagbuf[taglen] == '\r')) {
              double la, lo;
              if (attr(tagbuf, "lat", la) && attr(tagbuf, "lon", lo)) cb(la, lo);
            }
          } else if (tl < (int)sizeof(tagbuf) - 1) {
            tagbuf[tl++] = c;
          }
        }
      }
    }
    f.close();
  }
};
