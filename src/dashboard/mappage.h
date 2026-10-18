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

static const int MAP_Y0 = 24, MAP_H = 216, BAND_H = 18;  // zone carte : y 24..239
static const int MAP_CX = 160, MAP_CY = MAP_Y0 + MAP_H / 2;
static const uint8_t MAP_ZOOMS[3] = {15, 16, 17};
static const uint16_t MAP_BG = RGB565(232, 228, 220);
static const uint16_t MAP_DARK = RGB565(10, 12, 16);
static const uint16_t MAP_BLUE = RGB565(23, 105, 255);
static const uint16_t MAP_RED = RGB565(224, 48, 30);

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
static char lastKey[64];
static float lastHeading = 0;

// Segments de trace visibles, en coordonnées écran
struct Seg {
  int16_t x0, y0, x1, y1;
};
static const int MAX_VIS = 500;
static Seg vis[MAX_VIS];
static int nVis = 0;

// Données de l'image en cours (calculées avant le dessin des bandes)
struct MapView {
  int z;
  int spd;
  float heading;
  bool hasTrack, onTrack, hasTurn;
  float turnD, turnA, remain;
  bool destVisible;
  int dsx, dsy;
  char turnTxt[16], remTxt[16];
};

static void fmtDist(char *out, size_t n, float m) {
  if (m < 100)
    snprintf(out, n, "%d m", (int)(m / 5 + 0.5f) * 5);
  else if (m < 1000)
    snprintf(out, n, "%d m", (int)(m / 10 + 0.5f) * 10);
  else if (m < 10000)
    snprintf(out, n, "%.1f km", m / 1000.0f);
  else
    snprintf(out, n, "%d km", (int)(m / 1000.0f + 0.5f));
}

static bool inBand(int by, int y0, int y1) { return y1 >= by && y0 < by + BAND_H; }

// Icône de virage centrée en (cx,cy) ; angle > 0 : droite, < 0 : gauche
static void drawTurnIcon(TFT_eSPI &g, int cx, int cy, bool straight, float angle, uint16_t color) {
  if (straight) {
    thickLine(g, cx, cy + 11, cx, cy - 6, 4, color);
    g.fillTriangle(cx - 8, cy - 4, cx + 8, cy - 4, cx, cy - 13, color);
    return;
  }
  int s = angle >= 0 ? 1 : -1;
  thickLine(g, cx - s * 7, cy + 11, cx - s * 7, cy - 3, 4, color);
  thickLine(g, cx - s * 7, cy - 3, cx + s * 3, cy - 3, 4, color);
  g.fillTriangle(cx + s * 2, cy - 10, cx + s * 2, cy + 4, cx + s * 12, cy - 3, color);
}

static void drawMapOverlay(int by, const MapView &v) {
  // Trace : liseré blanc puis trait bleu
  for (int pass = 0; pass < 2; pass++) {
    uint16_t c = pass == 0 ? col::WHITE : MAP_BLUE;
    float w = pass == 0 ? 6.5f : 4.0f;
    int r = pass == 0 ? 3 : 2;
    for (int k = 0; k < nVis; k++) {
      const Seg &s = vis[k];
      int ya = s.y0 < s.y1 ? s.y0 : s.y1, yb = s.y0 < s.y1 ? s.y1 : s.y0;
      if (!inBand(by, ya - 4, yb + 4)) continue;
      thickLine(band, s.x0, s.y0 - by, s.x1, s.y1 - by, w, c);
      band.fillCircle(s.x1, s.y1 - by, r, c);
      band.fillCircle(s.x0, s.y0 - by, r, c);
    }
  }

  // Destination
  if (v.destVisible && inBand(by, v.dsy - 10, v.dsy + 10)) {
    band.fillCircle(v.dsx, v.dsy - by, 9, col::WHITE);
    band.fillCircle(v.dsx, v.dsy - by, 7, MAP_RED);
    band.fillCircle(v.dsx, v.dsy - by, 2, col::WHITE);
  }

  // Position (flèche orientée selon le cap, nord en haut)
  if (inBand(by, MAP_CY - 20, MAP_CY + 20)) {
    float a = v.heading * (float)M_PI / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    const float P[4][2] = {{0, -16}, {-11, 12}, {0, 6}, {11, 12}};
    int X[4], Y[4];
    for (int pass = 0; pass < 2; pass++) {
      float k = pass == 0 ? 1.0f : 0.68f;
      for (int i = 0; i < 4; i++) {
        float px = P[i][0] * k, py = P[i][1] * k;
        X[i] = MAP_CX + (int)lroundf(px * ca - py * sa);
        Y[i] = MAP_CY - by + (int)lroundf(px * sa + py * ca);
      }
      uint16_t c = pass == 0 ? col::WHITE : MAP_BLUE;
      band.fillTriangle(X[0], Y[0], X[1], Y[1], X[2], Y[2], c);
      band.fillTriangle(X[0], Y[0], X[3], Y[3], X[2], Y[2], c);
    }
  }

  // Vitesse (en haut à gauche)
  if (inBand(by, 29, 63)) {
    char buf[8];
    band.fillRoundRect(5, 29 - by, 80, 34, 6, MAP_DARK);
    snprintf(buf, sizeof(buf), "%d", v.spd);
    band.setTextDatum(TL_DATUM);
    band.setTextColor(col::WHITE, MAP_DARK);
    band.drawString(buf, 12, 34 - by, 4);
    int w = band.textWidth(buf, 4);
    band.setTextColor(col::DIM, MAP_DARK);
    band.drawString("km/h", 12 + w + 4, 46 - by, 2);
  }

  // Nord (en haut à droite)
  if (inBand(by, 26, 54)) {
    band.fillCircle(298, 40 - by, 14, MAP_DARK);
    band.setTextDatum(MC_DATUM);
    band.setTextColor(col::NEEDLE, MAP_DARK);
    band.drawString("N", 298, 40 - by, 2);
  }

  // Attribution OpenStreetMap (obligatoire)
  if (inBand(by, 194, 207)) {
    band.fillRect(212, 196 - by, 108, 12, col::WHITE);
    band.setTextDatum(TR_DATUM);
    band.setTextColor(RGB565(60, 60, 60), col::WHITE);
    band.drawString("(c) OpenStreetMap", 318, 198 - by, 1);
  }

  // Bandeau du bas : prochain virage, distance restante, zoom
  if (inBand(by, 208, 239)) {
    band.fillRect(0, 208 - by, 320, 32, MAP_DARK);
    band.setTextPadding(0);
    band.setTextDatum(ML_DATUM);
    if (!v.hasTrack) {
      band.setTextColor(col::DIM, MAP_DARK);
      band.drawString("pas de trace", 8, 224 - by, 2);
    } else if (!v.onTrack) {
      band.setTextColor(col::ORANGE, MAP_DARK);
      band.drawString("hors trace", 8, 224 - by, 2);
    } else {
      drawTurnIcon(band, 22, 224 - by, !v.hasTurn, v.turnA, col::CYAN);
      band.setTextColor(col::WHITE, MAP_DARK);
      if (v.hasTurn)
        band.drawString(v.turnTxt, 42, 224 - by, 4);
      else
        band.drawString("tout droit", 42, 224 - by, 2);
    }
    band.setTextDatum(MC_DATUM);
    band.setTextColor(col::WHITE, MAP_DARK);
    band.drawString(v.remTxt, 214, 224 - by, 4);
    band.setTextDatum(MR_DATUM);
    band.setTextColor(col::DIM, MAP_DARK);
    char zt[8];
    snprintf(zt, sizeof(zt), "z%d", v.z);
    band.drawString(zt, 314, 224 - by, 2);
  }
}

static void mapMessage(const char *msg, uint16_t color) {
  tft.fillRect(0, MAP_Y0, 320, MAP_H, col::BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(0);
  tft.setTextColor(color, col::BG);
  tft.drawString(msg, 160, 130, 4);
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
      mapMessage("Carte SD absente", col::ORANGE);
      mapForce = false;
    }
    return;
  }
  bool fix = gps.location.isValid() && gps.location.age() < 3000;
  if (!fix) {
    if (!noFixShown) {
      mapMessage("En attente du fix GPS", col::ORANGE);
      noFixShown = true;
      mapForce = true;
    }
    return;
  }
  if (now - lastMapMs < 300) return;

  const int z = MAP_ZOOMS[mapZoomIdx];
  double lat = gps.location.lat(), lon = gps.location.lng();
  double wxd, wyd;
  worldPx(lat, lon, z, wxd, wyd);
  int32_t ox = (int32_t)floor(wxd) - MAP_CX;
  int32_t oy = (int32_t)floor(wyd) - MAP_H / 2;

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
  fmtDist(v.turnTxt, sizeof(v.turnTxt), v.turnD);
  fmtDist(v.remTxt, sizeof(v.remTxt), v.remain);

  // Faut-il redessiner ?
  char key[64];
  snprintf(key, sizeof(key), "%d|%d|%d|%d|%d|%s|%s", v.spd, z, (int)(v.heading / 8), v.onTrack,
           v.hasTurn, v.turnTxt, v.remTxt);
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
    drawMapOverlay(by, v);
    band.pushSprite(0, by);
    pollGps();  // le GPS continue d'envoyer pendant le dessin
  }
}
