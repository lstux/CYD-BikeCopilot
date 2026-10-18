#pragma once
// Page « Compteur » : jauge à aiguille (dans un sprite) et six tuiles de mesures.
#include "hud.h"

namespace pg_gauge {
using namespace gfx;

// Sprite de la jauge : placé à (SX, SY) sur l'écran
static const int SX = 70, SY = 22, SW = 180, SH = 188;
static const float CX = 160.0f, CY = 119.0f;
static const float R_MID = 84.0f, R_W = 11.0f;
static const float A_MIN = -135.0f, A_SPAN = 270.0f;

// Tuiles : colonne gauche (x = 4) et droite (x = 251)
static const int TILE_X[2] = {4, 251};
static const int TILE_Y[3] = {29, 88, 147};

struct TileDef {
  hud::IconKind icon;
  const char *label;
  uint16_t color;
};
static const TileDef TILES[6] = {
    {hud::IC_DIST, "DIST.", col::ACCENT},  {hud::IC_TIME, "TEMPS", col::ACCENT}, {hud::IC_CLIMB, "D+", col::OK},
    {hud::IC_AVG, "MOY.", col::ACCENT},    {hud::IC_MAX, "MAX", col::AMBER},     {hud::IC_ALT, "ALT.", col::OK}};

static inline void tilePos(int i, int &x, int &y) {
  x = TILE_X[i / 3];
  y = TILE_Y[i % 3];
}

static void drawTileFrames(const Surf &scr) {
  for (int i = 0; i < 6; i++) {
    int x, y;
    tilePos(i, x, y);
    hud::tileFrame(scr, x, y, TILES[i].icon, TILES[i].label, TILES[i].color);
  }
}

static void drawTileValue(const Surf &scr, int i, const char *value, const char *unit, uint16_t color = col::TXT) {
  int x, y;
  tilePos(i, x, y);
  hud::tileValue(scr, x, y, value, unit, color);
}

// Graduations et libellés selon l'échelle
static void steps(int maxKmh, int &minor, int &label) {
  minor = maxKmh <= 30 ? 1 : (maxKmh <= 60 ? 2 : 5);
  label = maxKmh <= 30 ? 5 : (maxKmh <= 60 ? 10 : 20);
}

// Dessine la jauge dans la surface (sprite placé à SX, SY). `glow` : halo autour de l'arc de valeur.
static void draw(const Surf &s, int maxKmh, float needleKmh, int shownKmh, float maxSeenKmh, bool glow) {
  rect(s, SX, SY, SW, SH, col::BG);
  float ratio = clamp01(needleKmh / (float)maxKmh);
  float aEnd = A_MIN + A_SPAN * ratio;
  const float rIn = R_MID - R_W / 2, rOut = R_MID + R_W / 2;
  auto grad = [&](float t) { return hud::gradColor(t * ratio); };

  // halo
  if (glow && ratio > 0.01f) {
    auto g1 = [&](float t) { return mix(hud::gradColor(t * ratio), col::BG, 26); };
    auto g2 = [&](float t) { return mix(hud::gradColor(t * ratio), col::BG, 42); };
    arc(s, CX, CY, rIn - 4, rOut + 4, A_MIN, aEnd, g1);
    arc(s, CX, CY, rIn - 2, rOut + 2, A_MIN, aEnd, g2);
  }
  // piste, puis arc de valeur
  arc(s, CX, CY, rIn, rOut, A_MIN, A_MIN + A_SPAN, [](float) { return col::GTRACK; });
  if (ratio > 0.004f) arc(s, CX, CY, rIn, rOut, A_MIN, aEnd, grad);

  // repère de vitesse maximale atteinte
  if (maxSeenKmh > 0.5f) {
    float am = A_MIN + A_SPAN * clamp01(maxSeenKmh / (float)maxKmh);
    Pt a = polar(CX, CY, R_MID - 1.5f, am), b = polar(CX, CY, R_MID + 9.0f, am);
    line(s, a.x, a.y, b.x, b.y, 2.2f, col::AMBER);
  }

  // graduations
  int minor, lab;
  steps(maxKmh, minor, lab);
  for (int v = 0; v <= maxKmh; v += minor) {
    float a = A_MIN + A_SPAN * v / maxKmh;
    bool major = v % lab == 0;
    float r0 = major ? 69.5f : 71.0f, r1 = major ? 77.5f : 75.5f;
    float sx = sinf(a * 0.017453292f), sy = -cosf(a * 0.017453292f);
    line(s, CX + r0 * sx, CY + r0 * sy, CX + r1 * sx, CY + r1 * sy, major ? 1.7f : 0.9f,
         major ? col::TICKMAJ : col::TICKMIN, false);
  }
  // libellés
  Surf t = withBg(s, col::BG);
  for (int v = 0; v <= maxKmh; v += lab) {
    float a = A_MIN + A_SPAN * v / maxKmh;
    float sx = sinf(a * 0.017453292f), sy = -cosf(a * 0.017453292f);
    char b[8];
    snprintf(b, sizeof(b), "%d", v);
    int dxo = v == 0 ? -5 : (v + lab > maxKmh ? 5 : 0);  // les libellés des extrémités s'écartent du chiffre central
    text(t, FONT_S13, (int)lroundf(CX + 58.5f * sx) + dxo, (int)lroundf(CY + 58.5f * sy) + 5, b,
         v + lab > maxKmh ? col::TXT : col::SEC, AL_C);
  }

  // aiguille effilée + moyeu
  float sa = sinf(aEnd * 0.017453292f), ca = cosf(aEnd * 0.017453292f);
  auto rot = [&](float px, float py, float &ox, float &oy) {  // (px, py) dans le repère « aiguille vers le haut »
    ox = CX + px * ca - py * sa;
    oy = CY + px * sa + py * ca;
  };
  float xs[6], ys[6];
  rot(-1.5f, 11, xs[0], ys[0]);
  rot(1.5f, 11, xs[1], ys[1]);
  rot(3.5f, 0, xs[2], ys[2]);
  rot(0.5f, -73, xs[3], ys[3]);
  rot(-0.5f, -73, xs[4], ys[4]);
  rot(-3.5f, 0, xs[5], ys[5]);
  polygon(s, xs, ys, 6, col::RED);
  disc(s, CX, CY, 7.5f, col::RED);
  disc(s, CX, CY, 6.0f, col::CARD);
  disc(s, CX, CY, 2.5f, col::RED);

  // vitesse numérique
  char b[8];
  snprintf(b, sizeof(b), "%d", shownKmh);
  text(t, FONT_B56, 160, 172, b, col::TXT, AL_C);
  text(t, FONT_S11, 160, 192, "km/h", col::SEC, AL_C, 1);
}

}  // namespace pg_gauge
