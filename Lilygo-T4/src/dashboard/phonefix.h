// Adaptateur : position reçue du téléphone (SensorServer) -> Fix.
// Logique pure (aucune dépendance Arduino/Wi-Fi) : testable sur PC, voir ../../test/host.
#pragma once
#include <stdint.h>
#include <time.h>
#include "fix.h"

// Dernier message « position » reçu, tel que décodé par phonelink.cpp.
struct PhoneSample {
  bool have = false;  // au moins une position reçue depuis le démarrage
  double lat = 0, lon = 0;
  bool speedValid = false;
  float speedMs = 0;  // m/s
  bool bearingValid = false;
  float bearing = 0;  // degrés, 0 = nord
  bool altValid = false;
  float alt = 0;  // m
  bool accValid = false;
  float acc = 0;        // précision horizontale estimée, m
  uint64_t timeMs = 0;  // horodatage du point (ms depuis 1970, UTC), 0 si absent
};

// Un point est « récent » tant qu'un nouveau point est arrivé depuis moins de ce délai.
// (SensorServer renvoie la dernière position connue, même si le GPS du téléphone s'est arrêté.)
#ifndef PHONE_FIX_STALE_MS
#define PHONE_FIX_STALE_MS 5000
#endif

struct PhoneTracker {
  uint64_t lastTimeMs = 0;
  double lastLat = 0, lastLon = 0;
  uint32_t lastNewMs = 0;  // millis() de l'arrivée du dernier point nouveau
  bool seen = false;
  bool accValid = false;  // précision du dernier point (pour le filtre de vitesse et la barre d'état)
  float acc = 0;
};

// Met la Fix à jour à partir du dernier message. À appeler souvent (à chaque tour de loop) :
//  - fix.seq ne bouge que si le point est nouveau ;
//  - fix.valid retombe à false si aucun point nouveau depuis PHONE_FIX_STALE_MS.
static void phoneApply(Fix &f, PhoneTracker &t, const PhoneSample &s, uint32_t nowMs) {
  if (!s.have) {
    f.valid = false;
    return;
  }
  // Nouveau point : horodatage différent, ou (sans horodatage) coordonnées différentes
  bool isNew;
  if (s.timeMs != 0 || t.lastTimeMs != 0) isNew = s.timeMs != t.lastTimeMs;
  else isNew = s.lat != t.lastLat || s.lon != t.lastLon;
  if (!t.seen) isNew = true;
  if (isNew) {
    t.seen = true;
    t.lastTimeMs = s.timeMs;
    t.lastLat = s.lat;
    t.lastLon = s.lon;
    t.lastNewMs = nowMs;
    f.seq++;
  }

  f.valid = t.seen && (uint32_t)(nowMs - t.lastNewMs) < PHONE_FIX_STALE_MS;
  f.lat = s.lat;
  f.lon = s.lon;

  f.speedValid = f.valid && s.speedValid;
  f.kmh = f.speedValid ? s.speedMs * 3.6f : 0;
  f.courseValid = s.bearingValid;
  f.course = s.bearingValid ? s.bearing : 0;
  f.altValid = s.altValid;
  f.alt = s.altValid ? s.alt : 0;

  // Pas de satellites ni de HDOP côté téléphone : seule la précision en mètres est connue.
  f.sats = 0;
  f.hdopValid = false;
  f.hdop = 99;
  t.accValid = s.accValid;
  t.acc = s.accValid ? s.acc : 0;

  // Heure UTC du point, si le téléphone l'a envoyée et qu'elle est plausible (après 2020)
  f.timeValid = f.dateValid = false;
  if (s.timeMs > 1577836800000ULL) {
    time_t secs = (time_t)(s.timeMs / 1000ULL);
    struct tm g;
    if (gmtime_r(&secs, &g)) {
      f.timeValid = f.dateValid = true;
      f.hour = g.tm_hour;
      f.minute = g.tm_min;
      f.second = g.tm_sec;
      f.year = g.tm_year + 1900;
      f.month = g.tm_mon + 1;
      f.day = g.tm_mday;
    }
  }
}
