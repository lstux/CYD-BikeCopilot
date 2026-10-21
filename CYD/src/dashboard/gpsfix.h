// Adaptateur : TinyGPSPlus (GPS série de la CYD) -> Fix.
#pragma once
#include <TinyGPSPlus.h>
#include "fix.h"

static void fixFromGps(TinyGPSPlus &g, Fix &f) {
  // isUpdated() doit être lu avant lat()/lng(), qui l'effacent
  if (g.location.isValid() && g.location.isUpdated()) f.seq++;
  f.valid = g.location.isValid() && g.location.age() < 3000;
  if (g.location.isValid()) {
    f.lat = g.location.lat();
    f.lon = g.location.lng();
  }
  f.speedValid = g.speed.isValid() && g.speed.age() < 3000;
  f.kmh = f.speedValid ? (float)g.speed.kmph() : 0;
  f.courseValid = g.course.isValid();
  f.course = f.courseValid ? (float)g.course.deg() : 0;
  f.altValid = g.altitude.isValid();
  f.alt = f.altValid ? (float)g.altitude.meters() : 0;
  f.sats = (uint8_t)g.satellites.value();
  f.hdopValid = g.hdop.isValid();
  f.hdop = f.hdopValid ? (float)g.hdop.hdop() : 99;
  f.timeValid = g.time.isValid();
  f.hour = g.time.hour();
  f.minute = g.time.minute();
  f.second = g.time.second();
  f.dateValid = g.date.isValid();
  f.year = g.date.year();
  f.month = g.date.month();
  f.day = g.date.day();
}
