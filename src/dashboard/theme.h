#pragma once
// Palette, points et coordonnées polaires (sans dépendance à TFT_eSPI).
// Les angles sont en degrés, 0 = haut, sens horaire.
#include <Arduino.h>
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
// Palette de l'interface « V2 »
static const uint16_t CARD = RGB565(14, 19, 25);       // fond des cartes
static const uint16_t BORDER = RGB565(28, 36, 46);     // contour des cartes
static const uint16_t SURF2 = RGB565(23, 32, 42);      // boutons sur les cartes
static const uint16_t TXT = RGB565(243, 246, 249);     // texte principal
static const uint16_t SEC = RGB565(142, 153, 166);     // texte secondaire
static const uint16_t TER = RGB565(90, 101, 114);      // texte discret
static const uint16_t DOT = RGB565(42, 52, 64);        // pastille inactive
static const uint16_t TICKMIN = RGB565(56, 66, 78);    // graduation fine
static const uint16_t TICKMAJ = RGB565(201, 209, 218); // graduation principale
static const uint16_t GTRACK = RGB565(18, 25, 34);     // piste de la jauge
static const uint16_t AMBER = RGB565(255, 176, 32);
static const uint16_t RED = RGB565(255, 77, 46);
static const uint16_t ACCENT = RGB565(25, 211, 255);   // cyan principal
static const uint16_t OK = RGB565(46, 230, 107);
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

