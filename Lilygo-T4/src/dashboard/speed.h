// Vitesse affichée, trajet et échelle de la jauge : logique pure (testable sur PC).
// Reprend le comportement du dashboard CYD, mais sans satellites ni HDOP : le téléphone ne
// fournit qu'une précision en mètres (voir phonefix.h).
#pragma once
#include <math.h>
#include <stdint.h>
#include "fix.h"
#include "geo.h"
#include "config.h"

// --- Vitesse filtrée (anti-dérive à l'arrêt) --------------------------------------------------
struct SpeedFilter {
  float kmh = 0;
  bool moving = false;
  uint32_t aboveSince = 0, lastMs = 0;

  // accOk : précision du point suffisante pour déclencher le départ
  void update(uint32_t now, const Fix &fix, bool accOk) {
    float raw = fix.speedValid ? fix.kmh : 0;
    if (!moving) {
      if (fix.valid && accOk && raw >= SPEED_START_KMH) {
        if (aboveSince == 0) aboveSince = now ? now : 1;
        if (now - aboveSince >= SPEED_START_MS) {
          moving = true;
          kmh = raw;
        }
      } else {
        aboveSince = 0;
      }
      if (!moving) kmh = 0;
    } else if (raw < SPEED_STOP_KMH) {
      moving = false;
      aboveSince = 0;
      kmh = 0;
    } else {
      float alpha = 1.0f - expf(-(float)(now - lastMs) / SPEED_TAU_MS);
      kmh += alpha * (raw - kmh);
    }
    lastMs = now;
  }
};

// --- Statistiques du trajet -------------------------------------------------------------------
struct TripStats {
  double distM = 0;
  uint32_t movingMs = 0;
  float maxKmh = 0;
  bool hasLast = false;
  double lastLat = 0, lastLon = 0;
  uint32_t lastTickMs = 0;
  float climbM = 0, altRef = 0, altFilt = 0;
  bool hasAlt = false;
  uint32_t lastSeq = 0;

  void reset() { *this = TripStats(); }

  // À appeler à chaque tour : ne traite chaque nouveau point (fix.seq) qu'une fois.
  void update(uint32_t now, const Fix &fix, const SpeedFilter &sp) {
    if (!fix.valid || fix.seq == lastSeq) return;
    lastSeq = fix.seq;
    if (hasLast) {
      uint32_t dt = now - lastTickMs;
      if (sp.moving) {
        double d = distanceM(lastLat, lastLon, fix.lat, fix.lon);
        if (d >= MIN_STEP_M) {
          distM += d;
          lastLat = fix.lat;
          lastLon = fix.lon;
        }
        movingMs += dt;
        if (sp.kmh > maxKmh) maxKmh = sp.kmh;
        if (fix.altValid) {  // dénivelé positif : lissage + hystérésis de 3 m
          float alt = fix.alt;
          if (!hasAlt) {
            hasAlt = true;
            altFilt = altRef = alt;
          }
          altFilt += (alt - altFilt) * 0.2f;
          if (altFilt - altRef >= 3.0f) {
            climbM += altFilt - altRef;
            altRef = altFilt;
          } else if (altFilt < altRef) {
            altRef = altFilt;
          }
        }
      }
    } else {
      lastLat = fix.lat;
      lastLon = fix.lon;
      hasLast = true;
    }
    lastTickMs = now;
  }

  float avgKmh() const { return movingMs > 0 ? (float)((distM / 1000.0) / (movingMs / 3600000.0)) : 0; }
};

// --- Échelle adaptative de la jauge ----------------------------------------------------------
struct GaugeScale {
  int maxKmh = GAUGE_MIN_MAX;
  uint32_t belowSince = 0;

  void update(float kmh, uint32_t now, int minMax) {
    if (maxKmh < minMax) maxKmh = minMax;
    if (kmh > maxKmh) {  // montée immédiate au palier juste au-dessus
      int m = ((int)ceilf(kmh / GAUGE_STEP)) * GAUGE_STEP;
      maxKmh = m < minMax ? minMax : m;
      belowSince = 0;
      return;
    }
    // descente d'un cran seulement après un moment nettement sous le palier inférieur
    if (maxKmh > minMax && kmh < (maxKmh - GAUGE_STEP) * GAUGE_DOWN_RATIO) {
      if (belowSince == 0) {
        belowSince = now ? now : 1;
      } else if (now - belowSince >= GAUGE_DOWN_MS) {
        maxKmh -= GAUGE_STEP;
        belowSince = now;
      }
    } else {
      belowSince = 0;
    }
  }
};
