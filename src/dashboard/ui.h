#pragma once
// Couleurs et primitives de dessin communes (jauge, boussole).
// Les angles sont en degrés, 0 = haut, sens horaire.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

#define RGB565(r, g, b) \
  ((uint16_t)(((((int)(r)) & 0xF8) << 8) | ((((int)(g)) & 0xFC) << 3) | ((((int)(b)) & 0xFF) >> 3)))

namespace col {
static const uint16_t BG = RGB565(5, 7, 10);
static const uint16_t RULE = RGB565(35, 42, 49);
static const uint16_t TRACK = RGB565(22, 27, 34);
static const uint16_t TICK = RGB565(75, 83, 92);
static const uint16_t TICK_MAJOR = RGB565(196, 203, 211);
static const uint16_t LABEL = RGB565(154, 163, 173);
static const uint16_t DIM = RGB565(123, 135, 148);
static const uint16_t TEXT2 = RGB565(184, 192, 200);
static const uint16_t WHITE = 0xFFFF;
static const uint16_t NEEDLE = RGB565(255, 77, 46);
static const uint16_t GREEN = RGB565(47, 224, 90);
static const uint16_t ORANGE = RGB565(255, 176, 32);
static const uint16_t CYAN = RGB565(0, 229, 255);
static const uint16_t BLUE = RGB565(42, 107, 255);
}  // namespace col

struct Pt {
  int16_t x, y;
};

static inline Pt polar(float cx, float cy, float r, float deg) {
  float a = deg * (float)M_PI / 180.0f;
  Pt p;
  p.x = (int16_t)lroundf(cx + r * sinf(a));
  p.y = (int16_t)lroundf(cy - r * cosf(a));
  return p;
}

static inline void fillQuad(TFT_eSPI &g, Pt a, Pt b, Pt c, Pt d, uint16_t color) {
  g.fillTriangle(a.x, a.y, b.x, b.y, c.x, c.y, color);
  g.fillTriangle(a.x, a.y, c.x, c.y, d.x, d.y, color);
}

// Secteur d'anneau entre deux rayons et deux angles
static inline void wedge(TFT_eSPI &g, float cx, float cy, float rIn, float rOut, float a0, float a1,
                         uint16_t color) {
  fillQuad(g, polar(cx, cy, rIn, a0), polar(cx, cy, rOut, a0), polar(cx, cy, rOut, a1),
           polar(cx, cy, rIn, a1), color);
}

// Segment d'épaisseur w (px)
static inline void thickLine(TFT_eSPI &g, float x0, float y0, float x1, float y1, float w,
                             uint16_t color) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) return;
  float nx = -dy / len * w * 0.5f, ny = dx / len * w * 0.5f;
  Pt a = {(int16_t)lroundf(x0 + nx), (int16_t)lroundf(y0 + ny)};
  Pt b = {(int16_t)lroundf(x0 - nx), (int16_t)lroundf(y0 - ny)};
  Pt c = {(int16_t)lroundf(x1 - nx), (int16_t)lroundf(y1 - ny)};
  Pt d = {(int16_t)lroundf(x1 + nx), (int16_t)lroundf(y1 + ny)};
  fillQuad(g, a, b, c, d, color);
}

// Dégradé cyan -> vert -> orange, t dans [0, 1]
static inline uint16_t gradient(float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  int r, g, b;
  if (t < 0.55f) {
    float k = t / 0.55f;
    r = (int)(0 + 47 * k);
    g = (int)(229 - 5 * k);
    b = (int)(255 - 165 * k);
  } else {
    float k = (t - 0.55f) / 0.45f;
    r = (int)(47 + 208 * k);
    g = (int)(224 - 48 * k);
    b = (int)(90 - 58 * k);
  }
  return RGB565(r, g, b);
}
