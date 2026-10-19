#pragma once
// Thème de la carte (sombre / clair) : choix manuel ou automatique d'après la photorésistance du CYD.
// Inclus par main.cpp avant mappage.h (utilise cfg, millis(), analogRead()).
#include <Arduino.h>
#include "config.h"

static int ldrRaw = -1;          // luminosité lissée, 0..4095 (plus grand = plus clair), -1 : pas encore lue
static bool mapLight = false;    // thème demandé : clair ?
static uint32_t ldrNextMs = 0, ldrPendingMs = 0;

static int ldrSample() {
  long s = 0;
  for (int i = 0; i < 8; i++) s += analogRead(LDR_PIN);
  int v = (int)(s / 8);
  return LDR_INVERT ? 4095 - v : v;
}

// À appeler souvent ; renvoie true quand le thème demandé change.
static bool mapThemeUpdate(uint32_t now) {
  if (now >= ldrNextMs) {
    ldrNextMs = now + 250;
    int v = ldrSample();
    ldrRaw = ldrRaw < 0 ? v : (ldrRaw * 7 + v) / 8;
  }
  bool target = mapLight;
  if (cfg.mapTheme == MT_DARK) {
    target = false;
    ldrPendingMs = 0;
  } else if (cfg.mapTheme == MT_LIGHT) {
    target = true;
    ldrPendingMs = 0;
  } else {  // auto : hystérésis + durée minimale
    bool want = mapLight;
    if (!mapLight && ldrRaw > LDR_LIGHT_ON) want = true;
    else if (mapLight && ldrRaw < LDR_LIGHT_OFF) want = false;
    if (want == mapLight) {
      ldrPendingMs = 0;
    } else {
      if (ldrPendingMs == 0) ldrPendingMs = now ? now : 1;
      if (now - ldrPendingMs >= LDR_HOLD_MS) {
        target = want;
        ldrPendingMs = 0;
      }
    }
  }
  if (target == mapLight) return false;
  mapLight = target;
  return true;
}
