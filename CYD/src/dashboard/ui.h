#pragma once
// Anciennes primitives de dessin (sans anti-crénelage), encore utilisées par la page carte.
#include <TFT_eSPI.h>
#include "theme.h"

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
