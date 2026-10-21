#pragma once
// Journal des trajets sur la carte SD : enregistrement, analyse (stats + courbes), index.
//
// Fichier d'un trajet : /trips/AAAAMMJJ_HHMMSS.csv (heure locale), un point toutes les 2 s :
//   t (epoch UTC),lat,lon,alt (m),kmh,sat,hdop
// Index : /trips/index.csv, une ligne par trajet terminé :
//   nom,t0,distance_m,durée_s,temps_roulant_s,vmax,vmoy,d+,d-
#include <Arduino.h>
#include <SD.h>
#include <functional>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geo.h"
#include "tripdata.h"

#define TRIP_DIR       "/trips"
#define TRIP_INDEX     "/trips/index.csv"
#define LOG_SAMPLE_MS  2000UL    // un point toutes les 2 s
#define LOG_FLUSH_MS   30000UL   // écriture sur la SD toutes les 30 s
#define LOG_PAUSE_MS   60000UL   // à l'arrêt depuis plus de 60 s : on n'enregistre plus
#define LOG_END_MS     300000UL  // à l'arrêt depuis 5 min : trajet terminé
#define LOG_BUF        24
#define TRIP_MIN_M     100.0f    // en dessous, le trajet est supprimé à la fin
#define ELEV_THRESH_M  3.0f      // seuil du dénivelé (anti-bruit de l'altitude GPS)

// ---------------------------------------------------------------------------------------
// Lecture ligne à ligne
// ---------------------------------------------------------------------------------------
static void forEachLine(const char *path, const std::function<void(const char *)> &cb) {
  File f = SD.open(path, FILE_READ);
  if (!f) return;
  char buf[256], line[160];
  int ll = 0;
  while (true) {
    int n = f.read((uint8_t *)buf, sizeof(buf));
    if (n <= 0) break;
    for (int i = 0; i < n; i++) {
      char c = buf[i];
      if (c == '\n' || c == '\r') {
        if (ll > 0) {
          line[ll] = 0;
          cb(line);
        }
        ll = 0;
      } else if (ll < (int)sizeof(line) - 1) {
        line[ll++] = c;
      }
    }
  }
  if (ll > 0) {
    line[ll] = 0;
    cb(line);
  }
  f.close();
}

static bool parseRow(const char *l, double &t, double &la, double &lo, float &alt, float &kmh) {
  char *e;
  t = strtod(l, &e);
  if (e == l || *e != ',') return false;
  la = strtod(e + 1, &e);
  if (*e != ',') return false;
  lo = strtod(e + 1, &e);
  if (*e != ',') return false;
  alt = (float)strtod(e + 1, &e);
  if (*e != ',') return false;
  kmh = (float)strtod(e + 1, &e);
  return true;
}

// ---------------------------------------------------------------------------------------
// Analyse d'un fichier trajet : statistiques, et courbes (vitesse / altitude en fonction de
// la distance) si `ser` est fourni.
// ---------------------------------------------------------------------------------------
static bool tripAnalyze(const char *path, TripSummary &s, TripSeries *ser) {
  double dist = 0, pt = 0, pla = 0, plo = 0, first = 0, last = 0;
  bool have = false;
  float maxK = 0;
  double mov = 0;
  float win[5];
  int wn = 0, wi = 0;
  float ref = 0;
  bool refOk = false;
  float up = 0, dn = 0;
  int rows = 0;

  forEachLine(path, [&](const char *l) {
    double t, la, lo;
    float alt, kmh;
    if (!parseRow(l, t, la, lo, alt, kmh)) return;
    rows++;
    if (!have) {
      first = t;
    } else if (kmh > 0.5f) {
      double dt = t - pt;
      dist += distanceM(pla, plo, la, lo);
      mov += dt > 10 ? 10 : (dt < 0 ? 0 : dt);
    }
    if (kmh > maxK) maxK = kmh;
    pt = last = t;
    pla = la;
    plo = lo;
    have = true;
    // dénivelé : moyenne glissante sur 5 points puis seuil d'hystérésis
    win[wi] = alt;
    wi = (wi + 1) % 5;
    if (wn < 5) wn++;
    float m = 0;
    for (int i = 0; i < wn; i++) m += win[i];
    m /= wn;
    if (!refOk) {
      ref = m;
      refOk = true;
    } else if (m > ref + ELEV_THRESH_M) {
      up += m - ref;
      ref = m;
    } else if (m < ref - ELEV_THRESH_M) {
      dn += ref - m;
      ref = m;
    }
  });
  if (rows < 2) return false;

  s.t0 = (uint32_t)first;
  s.distM = (float)dist;
  s.durS = (uint32_t)(last - first);
  s.movS = (uint32_t)mov;
  s.maxKmh = maxK;
  s.avgKmh = mov > 0 ? (float)(dist / 1000.0 / (mov / 3600.0)) : 0;
  s.up = (int16_t)(up + 0.5f);
  s.down = (int16_t)(dn + 0.5f);

  if (ser) {
    const int N = TripSeries::N;
    static double sumS[TripSeries::N], sumA[TripSeries::N];
    static int cnt[TripSeries::N];
    for (int i = 0; i < N; i++) {
      sumS[i] = sumA[i] = 0;
      cnt[i] = 0;
    }
    double cum = 0, qla = 0, qlo = 0;
    bool h2 = false;
    forEachLine(path, [&](const char *l) {
      double t, la, lo;
      float alt, kmh;
      if (!parseRow(l, t, la, lo, alt, kmh)) return;
      if (h2 && kmh > 0.5f) cum += distanceM(qla, qlo, la, lo);
      qla = la;
      qlo = lo;
      h2 = true;
      int b = dist > 1 ? (int)(cum / dist * (N - 1) + 0.5) : 0;
      if (b < 0) b = 0;
      if (b > N - 1) b = N - 1;
      sumS[b] += kmh;
      sumA[b] += alt;
      cnt[b]++;
    });
    float ps = 0, pa = 0;
    bool seen = false;
    float amin = 1e9f, amax = -1e9f, smax = 0;
    for (int i = 0; i < N; i++) {
      if (cnt[i]) {
        ps = (float)(sumS[i] / cnt[i]);
        pa = (float)(sumA[i] / cnt[i]);
        if (!seen) {  // remplit le début avec la première valeur
          for (int j = 0; j < i; j++) {
            ser->spd[j] = ps;
            ser->alt[j] = pa;
          }
          seen = true;
        }
      }
      ser->spd[i] = ps;
      ser->alt[i] = pa;
    }
    for (int i = 0; i < N; i++) {
      if (ser->alt[i] < amin) amin = ser->alt[i];
      if (ser->alt[i] > amax) amax = ser->alt[i];
      if (ser->spd[i] > smax) smax = ser->spd[i];
    }
    if (amax - amin < 10) {  // évite d'agrandir le bruit sur un parcours plat
      float mid = (amax + amin) / 2;
      amin = mid - 5;
      amax = mid + 5;
    }
    ser->altMin = amin;
    ser->altMax = amax;
    ser->spdMax = smax < 10 ? 10 : smax;
  }
  return true;
}

// ---------------------------------------------------------------------------------------
// Index des trajets
// ---------------------------------------------------------------------------------------
static void indexAppend(const TripSummary &s) {
  File f = SD.open(TRIP_INDEX, FILE_APPEND);
  if (!f) return;
  char l[128];
  int n = snprintf(l, sizeof(l), "%s,%lu,%.0f,%lu,%lu,%.1f,%.1f,%d,%d\n", s.name, (unsigned long)s.t0,
                   s.distM, (unsigned long)s.durS, (unsigned long)s.movS, s.maxKmh, s.avgKmh, (int)s.up,
                   (int)s.down);
  f.write((const uint8_t *)l, n);
  f.close();
}

static bool indexParse(const char *l, TripSummary &s) {
  const char *c = strchr(l, ',');
  if (!c || c - l >= (int)sizeof(s.name) || c == l) return false;
  memset(s.name, 0, sizeof(s.name));
  memcpy(s.name, l, c - l);
  char *e;
  s.t0 = (uint32_t)strtoul(c + 1, &e, 10);
  if (*e != ',') return false;
  s.distM = (float)strtod(e + 1, &e);
  if (*e != ',') return false;
  s.durS = (uint32_t)strtoul(e + 1, &e, 10);
  if (*e != ',') return false;
  s.movS = (uint32_t)strtoul(e + 1, &e, 10);
  if (*e != ',') return false;
  s.maxKmh = (float)strtod(e + 1, &e);
  if (*e != ',') return false;
  s.avgKmh = (float)strtod(e + 1, &e);
  if (*e != ',') return false;
  s.up = (int16_t)strtol(e + 1, &e, 10);
  if (*e != ',') return false;
  s.down = (int16_t)strtol(e + 1, &e, 10);
  return true;
}

// Charge les `maxN` derniers trajets de l'index (du plus ancien au plus récent)
static int indexLoad(TripSummary *out, int maxN) {
  int total = 0;
  forEachLine(TRIP_INDEX, [&](const char *l) {
    TripSummary t;
    if (indexParse(l, t)) total++;
  });
  int skip = total > maxN ? total - maxN : 0;
  int i = 0, k = 0;
  forEachLine(TRIP_INDEX, [&](const char *l) {
    TripSummary t;
    if (!indexParse(l, t)) return;
    if (i++ >= skip && k < maxN) out[k++] = t;
  });
  return k;
}

static bool indexHas(const char *name) {
  bool found = false;
  size_t nl = strlen(name);
  forEachLine(TRIP_INDEX, [&](const char *l) {
    if (strncmp(l, name, nl) == 0 && l[nl] == ',') found = true;
  });
  return found;
}

static void tripPath(const char *name, char *out, size_t n) { snprintf(out, n, "%s/%s.csv", TRIP_DIR, name); }

// Supprime un trajet : le fichier et sa ligne dans l'index (réécrit via un fichier temporaire)
static bool tripDelete(const char *name) {
  char path[64];
  tripPath(name, path, sizeof(path));
  SD.remove(path);
  const char *tmp = "/trips/index.tmp";
  SD.remove(tmp);
  File out = SD.open(tmp, FILE_WRITE);
  if (!out) return false;
  size_t nl = strlen(name);
  forEachLine(TRIP_INDEX, [&](const char *l) {
    if (strncmp(l, name, nl) == 0 && l[nl] == ',') return;
    out.print(l);
    out.print("\n");
  });
  out.close();
  SD.remove(TRIP_INDEX);
  return SD.rename(tmp, TRIP_INDEX);  // en cas d'échec, l'index est reconstruit au prochain démarrage
}

// Dernier point valide d'un trajet (fin du trajet)
static bool tripLastPoint(const char *path, double &la, double &lo) {
  bool ok = false;
  forEachLine(path, [&](const char *l) {
    double t, a, b;
    float alt, kmh;
    if (parseRow(l, t, a, b, alt, kmh)) {
      la = a;
      lo = b;
      ok = true;
    }
  });
  return ok;
}

// Ajoute à l'index les fichiers .csv qui n'y figurent pas (coupure de courant pendant un trajet)
static void tripsRebuildIndex() {
  File dir = SD.open(TRIP_DIR);
  if (!dir) return;
  char names[8][24];
  int n = 0;
  File e = dir.openNextFile();
  while (e && n < 8) {
    const char *full = e.name();
    const char *b = strrchr(full, '/');
    b = b ? b + 1 : full;
    size_t bl = strlen(b);
    if (!e.isDirectory() && bl > 4 && bl - 4 < 24 && strcmp(b + bl - 4, ".csv") == 0 &&
        strcmp(b, "index.csv") != 0) {
      memcpy(names[n], b, bl - 4);
      names[n][bl - 4] = 0;
      n++;
    }
    e.close();
    e = dir.openNextFile();
  }
  if (e) e.close();
  dir.close();
  for (int i = 0; i < n; i++) {
    if (indexHas(names[i])) continue;
    char path[64];
    tripPath(names[i], path, sizeof(path));
    TripSummary s;
    strncpy(s.name, names[i], sizeof(s.name) - 1);
    if (tripAnalyze(path, s, nullptr) && s.distM >= TRIP_MIN_M)
      indexAppend(s);
    else
      SD.remove(path);
  }
}

// ---------------------------------------------------------------------------------------
// Enregistreur
// ---------------------------------------------------------------------------------------
class TripLogger {
 public:
  enum State { IDLE, PENDING, ACTIVE };
  State state() const { return st_; }
  const char *currentName() const { return name_; }

  void begin() { SD.mkdir(TRIP_DIR); }

  // À appeler en boucle. `moving` = vitesse filtrée « en mouvement », `fixOk` = position et heure valides.
  void update(uint32_t now, bool enabled, bool moving, bool fixOk, const LogSample &s, int tz) {
    if (!enabled) {
      if (st_ == ACTIVE) finish();
      st_ = IDLE;
      n_ = 0;
      return;
    }
    if (moving) lastMoveMs_ = now;

    if (st_ == IDLE) {
      if (moving && fixOk) {
        st_ = PENDING;
        startMs_ = now;
        lastSampleMs_ = 0;
        n_ = 0;
      }
      return;
    }

    // Échantillonnage : toutes les 2 s, mais plus après 60 s d'arrêt
    if (fixOk && now - lastSampleMs_ >= LOG_SAMPLE_MS && (moving || now - lastMoveMs_ < LOG_PAUSE_MS)) {
      lastSampleMs_ = now;
      if (n_ < LOG_BUF) buf_[n_++] = s;
    }

    if (st_ == PENDING) {
      if (!moving) {  // mouvement trop court : on jette
        st_ = IDLE;
        n_ = 0;
      } else if (now - startMs_ >= LOG_FLUSH_MS) {
        if (create(tz)) {
          st_ = ACTIVE;
          lastFlushMs_ = now;
          flush();
        } else {
          st_ = IDLE;
          n_ = 0;
        }
      }
      return;
    }

    // ACTIVE
    if (n_ > 0 && (now - lastFlushMs_ >= LOG_FLUSH_MS || n_ >= LOG_BUF - 1)) {
      lastFlushMs_ = now;
      flush();
    }
    if (!moving && now - lastMoveMs_ >= LOG_END_MS) finish();
  }

  // Termine le trajet en cours (écrit le reste, calcule les stats, met à jour l'index)
  bool finish() {
    if (st_ != ACTIVE) {
      st_ = IDLE;
      n_ = 0;
      return false;
    }
    flush();
    st_ = IDLE;
    char path[64];
    tripPath(name_, path, sizeof(path));
    TripSummary s;
    strncpy(s.name, name_, sizeof(s.name) - 1);
    bool ok = tripAnalyze(path, s, nullptr);
    if (ok && s.distM >= TRIP_MIN_M) {
      indexAppend(s);
      return true;
    }
    SD.remove(path);
    return false;
  }

 private:
  State st_ = IDLE;
  LogSample buf_[LOG_BUF];
  int n_ = 0;
  uint32_t startMs_ = 0, lastSampleMs_ = 0, lastFlushMs_ = 0, lastMoveMs_ = 0;
  char name_[24] = "";

  bool create(int tz) {
    if (n_ == 0) return false;
    int y, mo, d, h, mi, s;
    localParts(buf_[0].epoch, tz, y, mo, d, h, mi, s);
    snprintf(name_, sizeof(name_), "%04d%02d%02d_%02d%02d%02d", y, mo, d, h, mi, s);
    char path[64];
    tripPath(name_, path, sizeof(path));
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    const char *hdr = "t,lat,lon,alt,kmh,sat,hdop\n";
    f.write((const uint8_t *)hdr, strlen(hdr));
    f.close();
    return true;
  }

  void flush() {
    if (n_ == 0) return;
    char path[64];
    tripPath(name_, path, sizeof(path));
    File f = SD.open(path, FILE_APPEND);
    if (!f) return;  // on garde le tampon, nouvel essai au prochain passage
    char l[96];
    for (int i = 0; i < n_; i++) {
      const LogSample &s = buf_[i];
      int k = snprintf(l, sizeof(l), "%lu,%.6f,%.6f,%.1f,%.1f,%u,%.1f\n", (unsigned long)s.epoch, s.lat, s.lon,
                       s.alt, s.kmh, (unsigned)s.sats, s.hdop);
      f.write((const uint8_t *)l, k);
    }
    f.close();
    n_ = 0;
  }
};
