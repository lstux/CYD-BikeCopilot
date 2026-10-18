#pragma once
// Page « Direction » : rose des vents tournante (dans un sprite), flèche vers la destination,
// tuiles CAP et DESTINATION.
#include "hud.h"

namespace pg_compass {
using namespace gfx;

// Sprite : placé à (SX, SY) sur l'écran, le centre de la rose est en (CX, CY)
static const int SX = 72, SY = 21, SW = 176, SH = 186;
static const float CX = 160.0f, CY = 118.0f;
static const float R_OUT = 88.0f, R_TICK = 79.0f;

static const char *cardinal16(float deg) {
  static const char *n[16] = {"N",   "NNE", "NE", "ENE", "E",  "ESE", "SE", "SSE",
                              "S",   "SSO", "SO", "OSO", "O",  "ONO", "NO", "NNO"};
  int i = (int)((deg + 11.25f) / 22.5f) % 16;
  return n[i < 0 ? i + 16 : i];
}

static const char *cardinalName(float deg) {
  static const char *n[8] = {"Nord", "Nord-est", "Est", "Sud-est", "Sud", "Sud-ouest", "Ouest", "Nord-ouest"};
  int i = (int)((deg + 22.5f) / 45.0f) % 8;
  return n[i < 0 ? i + 8 : i];
}

static inline float norm360(float a) {
  while (a < 0) a += 360;
  while (a >= 360) a -= 360;
  return a;
}

// heading : cap en degrés ; bearing : relèvement de la destination ; haveDest : destination connue
static void draw(const Surf &s, float heading, float bearing, bool haveDest, const char *distNum, const char *distUnit) {
  rect(s, SX, SY, SW, SH, col::BG);
  // disque de fond (dégradé radial en trois paliers)
  disc(s, CX, CY, R_OUT, col::BORDER);
  disc(s, CX, CY, R_OUT - 1, RGB565(9, 13, 18));
  disc(s, CX, CY, 60, RGB565(11, 16, 22));
  disc(s, CX, CY, 34, RGB565(14, 21, 28));

  // graduations : tous les 5°, plus longues tous les 30°
  for (int az = 0; az < 360; az += 5) {
    float a = (az - heading) * 0.017453292f;
    float sx = sinf(a), sy = -cosf(a);
    bool major = az % 30 == 0;
    float r0 = R_TICK - (major ? 4.5f : 2.25f), r1 = R_TICK + (major ? 4.5f : 2.25f);
    line(s, CX + r0 * sx, CY + r0 * sy, CX + r1 * sx, CY + r1 * sy, major ? 1.5f : 0.9f,
         major ? col::TICKMAJ : col::TICKMIN, false);
  }
  // points cardinaux (toujours à l'endroit)
  Surf t = withBg(s, RGB565(11, 16, 22));
  static const char *L[4] = {"N", "E", "S", "O"};
  for (int i = 0; i < 4; i++) {
    float a = (i * 90 - heading) * 0.017453292f;
    int x = (int)lroundf(CX + 60 * sinf(a)), y = (int)lroundf(CY - 60 * cosf(a));
    if (y > 128 && x > (int)CX - 52 && x < (int)CX + 52) continue;  // évite la zone de distance
    text(t, FONT_B17, x, y + 6, L[i], i == 0 ? col::RED : col::TXT, AL_C);
  }

  // repère de destination sur l'anneau
  if (haveDest) {
    float a = (bearing - heading) * 0.017453292f;
    float mx = CX + R_TICK * sinf(a), my = CY - R_TICK * cosf(a);
    disc(s, mx, my, 8.5f, col::ACCENT);
    line(s, mx - 3.5f, my + 3.5f, mx - 3.5f, my - 3.5f, 1.3f, col::BG);
    line(s, mx - 3.5f, my - 3.5f, mx + 3.5f, my - 3.5f, 1.3f, col::BG);
    line(s, mx + 3.5f, my - 3.5f, mx + 1.0f, my - 1.0f, 1.3f, col::BG);
    line(s, mx + 1.0f, my - 1.0f, mx + 3.5f, my + 1.5f, 1.3f, col::BG);
    line(s, mx + 3.5f, my + 1.5f, mx - 3.5f, my + 1.5f, 1.3f, col::BG);
  }

  // repère fixe : sens de marche
  {
    float xs[3] = {154, 166, 160}, ys[3] = {22, 22, 32};
    polygon(s, xs, ys, 3, col::TXT);
  }

  // flèche vers la destination (au centre) : tête + hampe
  if (haveDest) {
    float rel = (bearing - heading) * 0.017453292f, sa = sinf(rel), ca = cosf(rel);
    auto rot = [&](float px, float py, float &ox, float &oy) {
      ox = CX + px * ca - py * sa;
      oy = CY + px * sa + py * ca;
    };
    float xs[4], ys[4];
    rot(0, -44, xs[0], ys[0]);
    rot(12, -22, xs[1], ys[1]);
    rot(-12, -22, xs[2], ys[2]);
    polygon(s, xs, ys, 3, col::ACCENT);
    rot(-5, -23, xs[0], ys[0]);
    rot(5, -23, xs[1], ys[1]);
    rot(5, 0, xs[2], ys[2]);
    rot(-5, 0, xs[3], ys[3]);
    polygon(s, xs, ys, 4, col::ACCENT);
  }
  disc(s, CX, CY, 4.5f, col::BG);
  circle(s, CX, CY, 3.2f, 1.4f, haveDest ? col::ACCENT : col::SEC);

  // distance
  int w1 = textWidth(FONT_B30, distNum, -1), w2 = textWidth(FONT_S11, distUnit);
  int x0 = (int)CX - (w1 + 4 + w2) / 2;
  Surf d = withBg(s, RGB565(11, 16, 22));
  text(d, FONT_B30, x0, 161, distNum, col::TXT, AL_L, -1);
  text(d, FONT_S11, x0 + w1 + 4, 161, distUnit, col::SEC);
}

// --- tuiles latérales (65 x 60) ----------------------------------------------------------------
static const int TILE_Y = 80, TILE_H = 60;

static void tile(const Surf &scr, int x, const char *label, const char *value, uint16_t valueColor,
                 const char *sub1, const char *sub2) {
  card(scr, x, TILE_Y, hud::TILE_W, TILE_H, 8, col::CARD, col::BORDER);
  Surf c = withBg(scr, col::CARD);
  text(c, FONT_S9, x + 6, TILE_Y + 14, label, col::SEC, AL_L, 1);
  text(c, FONT_B21, x + 5, TILE_Y + 34, value, valueColor, AL_L, -1);
  text(c, FONT_S9, x + 5, TILE_Y + 46, sub1, col::TXT);
  text(c, FONT_M9, x + 5, TILE_Y + 56, sub2, col::TER);
}

static void tileUpdate(const Surf &scr, int x, const char *value, uint16_t valueColor, const char *sub1,
                       const char *sub2) {
  Surf c = withBg(scr, col::CARD);
  rect(c, x + 3, TILE_Y + 18, hud::TILE_W - 6, TILE_H - 21, col::CARD);
  text(c, FONT_B21, x + 5, TILE_Y + 34, value, valueColor, AL_L, -1);
  text(c, FONT_S9, x + 5, TILE_Y + 46, sub1, col::TXT);
  text(c, FONT_M9, x + 5, TILE_Y + 56, sub2, col::TER);
}

}  // namespace pg_compass
