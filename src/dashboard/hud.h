#pragma once
// Éléments communs de l'interface : couleurs dégradées, icônes, barre d'état, pastilles de page, tuiles.
#include "gfx.h"
#include "theme.h"

namespace hud {
using namespace gfx;

// Dégradé cyan -> vert -> ambre, t de 0 à 1
static inline uint16_t gradColor(float t) {
  t = clamp01(t);
  if (t < 0.5f) return mix(col::OK, col::ACCENT, (uint8_t)(t * 2 * 255));
  return mix(col::AMBER, col::OK, (uint8_t)((t - 0.5f) * 2 * 255));
}

// --- icônes 10x10 (tracé d'une grille de 22 px réduite de moitié) ------------------------------
enum IconKind { IC_DIST, IC_TIME, IC_AVG, IC_MAX, IC_CLIMB, IC_ALT };

static void icon(const Surf &s, IconKind k, float x, float y, uint16_t c) {
  const float f = 10.0f / 22.0f, w = 1.15f;
  auto L = [&](float ax, float ay, float bx, float by) { line(s, x + ax * f, y + ay * f, x + bx * f, y + by * f, w, c); };
  switch (k) {
    case IC_DIST:
      circle(s, x + 5 * f, y + 17 * f, 2.5f * f + 0.6f, w, c);
      circle(s, x + 17 * f, y + 5 * f, 2.5f * f + 0.6f, w, c);
      L(7, 16, 9.94f, 14.44f); L(9.94f, 14.44f, 11, 11); L(11, 11, 12.06f, 7.56f); L(12.06f, 7.56f, 15, 6);
      break;
    case IC_TIME:
      circle(s, x + 11 * f, y + 11 * f, 8.5f * f, w, c);
      L(11, 6, 11, 11); L(11, 11, 15, 13);
      break;
    case IC_AVG:
      arc(s, x + 11 * f, y + 12 * f, 8.5f * f - w / 2, 8.5f * f + w / 2, -127, 127, [&](float) { return c; });
      L(11, 16, 15, 9);
      break;
    case IC_MAX:
      L(3, 17, 9, 10); L(9, 10, 13, 14); L(13, 14, 19, 5); L(14, 5, 19, 5); L(19, 5, 19, 10);
      break;
    case IC_CLIMB:
    case IC_ALT:
      L(2, 18, 9, 6); L(9, 6, 13, 12); L(13, 12, 16, 8); L(16, 8, 20, 18);
      if (k == IC_ALT) L(2, 18, 20, 18);
      break;
  }
}

// --- barre d'état (haut, 20 px) -------------------------------------------------------------
struct Status {
  int sats = 0;
  bool fix = false;
  int rec = 0;  // 0 : rien, 1 : départ détecté, 2 : trajet en cours
  char clock[8] = "--:--";
};

static void statusBar(const Surf &s, const Status &st, bool clear = true) {
  if (clear) rect(s, 0, 0, 320, 20, col::BG);
  int lit = st.fix ? (st.sats + 2) / 3 : 0;
  if (lit > 4) lit = 4;
  for (int i = 0; i < 4; i++) {
    int h = 3 + 2 * i;
    rect(s, 5 + i * 5, 16 - h, 3, h, i < lit ? (st.fix ? col::OK : col::AMBER) : col::DOT);
  }
  char b[16];
  snprintf(b, sizeof(b), "%d sat", st.sats);
  text(s, FONT_S11, 28, 15, b, col::SEC);
  int cw = text(s, FONT_S13, 316, 16, st.clock, col::TXT, AL_R);
  if (st.rec) {
    int rw = textWidth(FONT_S9, "REC", 1);
    int rx = 316 - cw - 10 - rw;
    if (st.rec == 2) {
      disc(s, rx - 7, 10.5f, 3.2f, col::RED);
      text(s, FONT_S9, rx, 14, "REC", col::RED, AL_L, 1);
    } else {
      circle(s, rx + rw / 2.0f, 10.5f, 3.0f, 1.2f, col::SEC);
    }
  }
}

// --- pastilles de page (bas, y = 231) -------------------------------------------------------
static void pageDots(const Surf &s, int page, int n, bool clear = true) {
  int total = 14 + (n - 1) * 4 + (n - 1) * 5;
  int x = 160 - total / 2;
  if (clear) rect(s, 0, 226, 320, 14, col::BG);
  for (int i = 0; i < n; i++) {
    if (i == page) {
      roundRect(s, x, 229, 14, 4, 2, col::ACCENT);
      x += 14 + 5;
    } else {
      roundRect(s, x, 229, 4, 4, 2, col::DOT);
      x += 4 + 5;
    }
  }
}

// --- tuile de mesure (65 x 54) -----------------------------------------------------------------
static const int TILE_W = 65, TILE_H = 54;

static void tileFrame(const Surf &s, int x, int y, IconKind k, const char *label, uint16_t iconColor) {
  card(s, x, y, TILE_W, TILE_H, 8, col::CARD, col::BORDER);
  Surf c = withBg(s, col::CARD);
  icon(c, k, x + 6, y + 5, iconColor);
  text(c, FONT_S9, x + 19, y + 14, label, col::SEC, AL_L, 1);
}

// Valeur d'une tuile (grande police si elle tient, sinon la petite) et unité en dessous
static void tileValue(const Surf &s, int x, int y, const char *value, const char *unit, uint16_t color = col::TXT) {
  Surf c = withBg(s, col::CARD);
  rect(c, x + 3, y + 17, TILE_W - 6, TILE_H - 20, col::CARD);
  const FontDef &f = textWidth(FONT_B21, value, -1) <= TILE_W - 8 ? FONT_B21 : FONT_B17;
  text(c, f, x + 5, y + 35, value, color, AL_L, -1);
  if (unit && unit[0]) text(c, FONT_M9, x + 5, y + 48, unit, col::SEC);
}

}  // namespace hud
