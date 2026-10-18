#pragma once
// Page carte : tuiles OSM (SD), trace GPX, position, bandeau "prochain virage".
//
// La carte est composée par bandes horizontales de 18 lignes dans un petit sprite
// (320x18, ~11 Ko) : on y lit les lignes de tuiles sur la SD, on dessine la trace et les
// éléments d'interface par-dessus, puis on envoie la bande à l'écran. Pas de scintillement
// et pas besoin d'un tampon plein écran (trop gros pour la mémoire de l'ESP32).
//
// Ce fichier est inclus par main.cpp, après la définition de tft, gps, compass, col::,
// destLat/destLon, currentKmh() et pollGps().
#include <SD.h>
#include <SPI.h>
#include "tiles.h"
#include "track.h"
#include "page_map.h"

TFT_eSprite band(&tft);
TileCache tiles;
Track track;
SPIClass sdSPI(VSPI);
bool sdOk = false;

static int mapZoomIdx = 1;
static int projZ = -1;
static bool mapForce = true;
static bool noFixShown = false;
static int32_t lastOx = 0, lastOy = 0;
static uint32_t lastMapMs = 0;
static char lastKey[96];
static float lastHeading = 0;

static void mapMessage(const char *msg, uint16_t color) {
  tft.fillRect(0, 20, 320, 206, col::BG);
  Surf s = onScreen(tft, col::BG);
  text(s, FONT_B17, 160, 125, msg, color, AL_C);
  hud::pageDots(s, 2, NB_PAGES);
}

static float navHeading(float kmh) {
  if (kmh >= COURSE_MIN_KMH && gps.course.isValid())
    lastHeading = gps.course.deg();
  else if (compass.ok() && compass.calibrated())
    lastHeading = compass.heading(COMPASS_OFFSET_DEG, COMPASS_INVERT);
  return lastHeading;
}

static void mapCycleZoom() {
  mapZoomIdx = (mapZoomIdx + 1) % 3;
  mapForce = true;
}

static void mapZoomStep(int d) {
  int n = mapZoomIdx + d;
  if (n < 0) n = 0;
  if (n > 2) n = 2;
  if (n != mapZoomIdx) {
    mapZoomIdx = n;
    mapForce = true;
  }
}

static void mapEnter() {
  mapForce = true;
  noFixShown = false;
  lastKey[0] = 0;
  lastMapMs = 0;
}

static void renderMapPage(uint32_t now) {
  if (!sdOk) {
    if (mapForce) {
      mapMessage("Carte SD absente", col::AMBER);
      mapForce = false;
    }
    return;
  }
  bool fix = gps.location.isValid() && gps.location.age() < 3000;
  if (!fix) {
    if (!noFixShown) {
      mapMessage("En attente du fix GPS", col::AMBER);
      noFixShown = true;
      mapForce = true;
    }
    return;
  }
  noFixShown = false;
  if (now - lastMapMs < 300) return;

  const int z = MAP_ZOOMS[mapZoomIdx];
  double lat = gps.location.lat(), lon = gps.location.lng();
  double wxd, wyd;
  worldPx(lat, lon, z, wxd, wyd);
  int32_t ox = (int32_t)floor(wxd) - MAP_CX;
  int32_t oy = (int32_t)floor(wyd) - MAP_CY;

  float kmh = currentKmh();
  MapView v;
  memset(&v, 0, sizeof(v));
  v.z = z;
  v.spd = (int)(kmh + 0.5f);
  v.heading = navHeading(kmh);
  v.hasTrack = track.valid();
  if (v.hasTrack) {
    float off = track.follow(lat, lon);
    v.onTrack = off < 60.0f;
    v.remain = track.remainingM();
    if (v.onTrack) v.hasTurn = track.nextTurn(v.turnD, v.turnA);
  } else {
    v.remain = (float)TinyGPSPlus::distanceBetween(lat, lon, destLat, destLon);
  }
  splitDist(v.turnNum, sizeof(v.turnNum), v.turnUnit, v.turnD);
  splitDist(v.remNum, sizeof(v.remNum), v.remUnit, v.remain);
  v.st = currentStatus();
  {  // heure d'arrivée estimée
    int mins = (int)(v.remain / 1000.0f / etaSpeedKmh() * 60.0f + 0.5f);
    if (gps.time.isValid()) {
      int t = ((gps.time.hour() + cfg.tz + 24) % 24) * 60 + gps.time.minute() + mins;
      snprintf(v.eta, sizeof(v.eta), "%02d:%02d", (t / 60) % 24, t % 60);
    } else {
      strcpy(v.eta, "--:--");
    }
  }
  {  // échelle : plus grande longueur « ronde » qui tient dans 72 px
    float mpp = 156543.034f * cosf((float)lat * 0.017453292f) / (float)(1 << z);
    static const int NICE[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    v.scaleM = NICE[0];
    for (int n : NICE) if (n / mpp <= 72.0f) v.scaleM = n;
    v.scalePx = (int)(v.scaleM / mpp + 0.5f);
    if (v.scaleM >= 1000) snprintf(v.scaleTxt, sizeof(v.scaleTxt), "%d km", v.scaleM / 1000);
    else snprintf(v.scaleTxt, sizeof(v.scaleTxt), "%d m", v.scaleM);
  }

  // Faut-il redessiner ?
  char key[96];
  snprintf(key, sizeof(key), "%d|%d|%d|%d|%d|%s%s|%s%s|%s|%s|%d%d%d|%d", v.spd, z, (int)(v.heading / 8), v.onTrack,
           v.hasTurn, v.turnNum, v.turnUnit, v.remNum, v.remUnit, v.eta, v.st.clock, v.st.sats, v.st.fix, v.st.rec,
           (int)v.turnA / 10);
  bool moved = labs((long)(ox - lastOx)) >= 2 || labs((long)(oy - lastOy)) >= 2;
  if (!mapForce && !moved && strcmp(key, lastKey) == 0) return;
  strncpy(lastKey, key, sizeof(lastKey) - 1);
  lastKey[sizeof(lastKey) - 1] = 0;
  lastOx = ox;
  lastOy = oy;
  lastMapMs = now;
  mapForce = false;

  // Trace projetée au zoom courant, segments visibles seulement
  if (v.hasTrack && projZ != z) {
    track.project(z);
    projZ = z;
  }
  nVis = 0;
  if (v.hasTrack) {
    for (int i = 0; i + 1 < track.count() && nVis < MAX_VIS; i++) {
      int32_t x0 = track.wx(i) - ox, y0 = track.wy(i) - oy + MAP_Y0;
      int32_t x1 = track.wx(i + 1) - ox, y1 = track.wy(i + 1) - oy + MAP_Y0;
      if ((x0 < -8 && x1 < -8) || (x0 > 328 && x1 > 328) || (y0 < MAP_Y0 - 8 && y1 < MAP_Y0 - 8) ||
          (y0 > 248 && y1 > 248))
        continue;
      if (labs(x0) > 20000 || labs(x1) > 20000 || labs(y0) > 20000 || labs(y1) > 20000) continue;
      vis[nVis++] = {(int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1};
    }
  }
  double dx, dy;
  worldPx(destLat, destLon, z, dx, dy);
  int32_t dsx = (int32_t)dx - ox, dsy = (int32_t)dy - oy + MAP_Y0;
  v.destVisible = dsx > -12 && dsx < 332 && dsy > MAP_Y0 - 12 && dsy < 252;
  v.dsx = (int)dsx;
  v.dsy = (int)dsy;

  // Composition par bandes
  uint8_t *buf = (uint8_t *)band.getPointer();
  for (int b = 0; b < MAP_H / BAND_H; b++) {
    const int by = MAP_Y0 + b * BAND_H;
    band.fillSprite(MAP_BG);
    for (int r = 0; r < BAND_H; r++) {
      int32_t wyp = oy + b * BAND_H + r;
      int32_t ty = wyp >> 8;
      int ry = (int)(wyp & 255);
      for (int32_t tx = ox >> 8; tx <= (ox + 319) >> 8; tx++) {
        int32_t px0 = tx * 256 > ox ? tx * 256 : ox;
        int32_t px1 = (tx + 1) * 256 < ox + 320 ? (tx + 1) * 256 : ox + 320;
        tiles.readRow(z, (int)tx, (int)ty, ry, (int)(px0 - tx * 256), (int)(px1 - px0),
                      buf + 2 * (r * 320 + (px0 - ox)));
      }
    }
    drawMapOverlay(band, by, v, mapZoomIdx, NB_PAGES);
    band.pushSprite(0, by);
    pollGps();  // le GPS continue d'envoyer pendant le dessin
  }
}
