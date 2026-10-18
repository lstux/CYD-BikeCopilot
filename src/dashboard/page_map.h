#pragma once
// Page carte : éléments d'interface dessinés par-dessus les tuiles (trace, position, puces, boutons,
// échelle, carte du prochain virage). Aucune dépendance au matériel : utilisé aussi par le simulateur.
#include "hud.h"
using namespace gfx;

static const int MAP_Y0 = 0, MAP_H = 240, BAND_H = 20;  // carte plein écran, composée en 12 bandes
static const int MAP_CX = 160, MAP_CY = 150;                  // la position est dans le tiers bas : on voit devant
static const uint8_t MAP_ZOOMS[5] = {13, 14, 15, 16, 17};
static const uint16_t MAP_BG = RGB565(10, 15, 20);
static const uint16_t MAP_BLUE = RGB565(42, 107, 255);
static const uint16_t MAP_ROUTE_DARK = RGB565(6, 42, 61);

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
  bool destVisible, hasDest;
  int dsx, dsy;
  int posX, posY;           // position GPS à l'écran (peut sortir de l'écran quand on déplace la carte)
  bool homeVisible;
  int homeX, homeY;
  bool canIn, canOut, panned;
  char turnNum[8], turnUnit[3], remNum[8], remUnit[3], eta[8];
  int scalePx, scaleM;  // longueur de l'échelle
  char scaleTxt[10];
  hud::Status st;
};

// Distance en deux morceaux (valeur, unité)
static void splitDist(char *num, size_t n, char *unit, float m) {
  if (m < 100) { snprintf(num, n, "%d", (int)(m / 5 + 0.5f) * 5); strcpy(unit, "m"); }
  else if (m < 1000) { snprintf(num, n, "%d", (int)(m / 10 + 0.5f) * 10); strcpy(unit, "m"); }
  else if (m < 10000) { snprintf(num, n, "%.1f", m / 1000.0f); strcpy(unit, "km"); }
  else { snprintf(num, n, "%d", (int)(m / 1000.0f + 0.5f)); strcpy(unit, "km"); }
}

static bool inBand(int by, int y0, int y1) { return y1 >= by && y0 < by + BAND_H; }

// Icône de virage centrée en (cx,cy) ; angle > 0 : droite, < 0 : gauche
static void drawTurnGlyph(const Surf &s, float cx, float cy, bool straight, float angle, uint16_t c) {
  const float k = 0.62f, w = 2.7f;
  float sg = angle >= 0 ? 1.0f : -1.0f;
  auto P = [&](float x, float y, float &ox, float &oy) { ox = cx + sg * (x - 17) * k; oy = cy + (y - 17) * k; };
  float ax[5], ay[5], hx[3], hy[3];
  int n;
  if (straight) {
    float pts[2][2] = {{17, 29}, {17, 8}}, hd[3][2] = {{10, 15}, {17, 8}, {24, 15}};
    n = 2;
    for (int i = 0; i < 2; i++) P(pts[i][0], pts[i][1], ax[i], ay[i]);
    for (int i = 0; i < 3; i++) P(hd[i][0], hd[i][1], hx[i], hy[i]);
  } else {
    float pts[5][2] = {{10, 29}, {10, 16}, {11.2f, 11.5f}, {15.5f, 10}, {25, 10}}, hd[3][2] = {{19, 3}, {26, 10}, {19, 17}};
    n = 5;
    for (int i = 0; i < 5; i++) P(pts[i][0], pts[i][1], ax[i], ay[i]);
    for (int i = 0; i < 3; i++) P(hd[i][0], hd[i][1], hx[i], hy[i]);
  }
  for (int i = 0; i + 1 < n; i++) line(s, ax[i], ay[i], ax[i + 1], ay[i + 1], w, c);
  line(s, hx[0], hy[0], hx[1], hy[1], w, c);
  line(s, hx[1], hy[1], hx[2], hy[2], w, c);
}

static const char *turnWord(float a, char *buf, size_t n) {
  float m = fabsf(a);
  const char *side = a >= 0 ? "droite" : "gauche";
  if (m < 25) snprintf(buf, n, "Tout droit");
  else if (m < 60) snprintf(buf, n, "Légèrement à %s", side);
  else if (m <= 125) snprintf(buf, n, "Tourner à %s", side);
  else snprintf(buf, n, "Virage serré à %s", side);
  return buf;
}

static void mapPlusMinus(const Surf &s, int x, int y, bool plus, bool enabled) {
  card(s, x, y, 31, 31, 9, col::CARD, col::BORDER);
  uint16_t c = enabled ? col::TXT : col::TER;
  line(s, x + 10, y + 15.5f, x + 21, y + 15.5f, 1.8f, c);
  if (plus) line(s, x + 15.5f, y + 10, x + 15.5f, y + 21, 1.8f, c);
}

static void drawMapOverlay(TFT_eSprite &band, int by, const MapView &v, int zoomIdx, int nPages) {
  Surf S = onSprite(band, 0, by);

  // Trace : contour sombre puis trait cyan
  for (int pass = 0; pass < 2; pass++) {
    uint16_t c = pass == 0 ? MAP_ROUTE_DARK : col::ACCENT;
    float w = pass == 0 ? 9.0f : 4.6f;
    for (int k = 0; k < nVis; k++) {
      const Seg &sg = vis[k];
      int ya = sg.y0 < sg.y1 ? sg.y0 : sg.y1, yb = sg.y0 < sg.y1 ? sg.y1 : sg.y0;
      if (!inBand(by, ya - 6, yb + 6)) continue;
      line(S, sg.x0, sg.y0, sg.x1, sg.y1, w, c);
    }
  }

  // Domicile : pastille avec une petite maison
  if (v.homeVisible && inBand(by, v.homeY - 11, v.homeY + 11)) {
    disc(S, v.homeX, v.homeY, 10.0f, col::ACCENT);
    disc(S, v.homeX, v.homeY, 8.6f, col::CARD);
    float rx[3] = {v.homeX - 5.5f, v.homeX, v.homeX + 5.5f}, ry[3] = {v.homeY - 0.5f, v.homeY - 6.0f, v.homeY - 0.5f};
    polygon(S, rx, ry, 3, col::TXT);
    rect(S, v.homeX - 3, v.homeY - 1, 6, 5, col::TXT);
    rect(S, v.homeX - 1, v.homeY + 1, 2, 3, col::CARD);
  }

  // Destination
  if (v.destVisible && v.hasDest && inBand(by, v.dsy - 11, v.dsy + 11)) {
    disc(S, v.dsx, v.dsy, 9.5f, col::TXT);
    disc(S, v.dsx, v.dsy, 7.5f, col::RED);
    disc(S, v.dsx, v.dsy, 2.2f, col::TXT);
  }

  // Position : halo, disque blanc cerclé de bleu, pointe orientée selon le cap (nord en haut)
  if (inBand(by, v.posY - 16, v.posY + 16)) {
    disc(S, v.posX, v.posY, 13.0f, col::ACCENT, 46);
    disc(S, v.posX, v.posY, 7.5f, col::TXT);
    circle(S, v.posX, v.posY, 6.8f, 1.6f, MAP_BLUE);
    float a = v.heading * 0.017453292f, ca = cosf(a), sa = sinf(a);
    auto R = [&](float px_, float py_, float &ox, float &oy) { ox = v.posX + px_ * ca - py_ * sa; oy = v.posY + px_ * sa + py_ * ca; };
    float xs[3], ys[3];
    R(0, -8, xs[0], ys[0]); R(4, 1, xs[1], ys[1]); R(0, -1, xs[2], ys[2]);
    polygon(S, xs, ys, 3, MAP_BLUE);
    R(0, -8, xs[0], ys[0]); R(-4, 1, xs[1], ys[1]); R(0, -1, xs[2], ys[2]);
    polygon(S, xs, ys, 3, MAP_BLUE);
  }

  // Voile en haut pour la lisibilité de la barre d'état
  if (by < 40) {
    for (int y = by; y < by + BAND_H && y < 40; y++) {
      uint8_t a = (uint8_t)(235 * (40 - y) / 40);
      for (int x = 0; x < 320; x++) {
        uint16_t c = band.readPixel(x, y - by);
        band.drawPixel(x, y - by, mix(col::BG, c, a));
      }
    }
    hud::statusBar(S, v.st, false);
  }

  // Vitesse
  if (inBand(by, 26, 59)) {
    char b[8];
    snprintf(b, sizeof(b), "%d", v.spd);
    int w1 = textWidth(FONT_B30, b, -1), w2 = textWidth(FONT_M10, "km/h");
    card(S, 5, 27, w1 + w2 + 26, 32, 9, col::CARD, col::BORDER);
    text(S, FONT_B30, 13, 51, b, col::TXT, AL_L, -1);
    text(S, FONT_M10, 13 + w1 + 4, 51, "km/h", col::SEC);
  }

  // Nord (boussole fixe : carte orientée nord en haut)
  if (inBand(by, 26, 59)) {
    const float cx = 299, cy = 43;
    disc(S, cx, cy, 16.0f, col::BORDER);
    disc(S, cx, cy, 15.0f, col::CARD);
    float nx[3] = {cx, cx + 4.5f, cx - 4.5f}, ny[3] = {cy - 11, cy + 1, cy + 1};
    polygon(S, nx, ny, 3, col::RED);
    float sx[3] = {cx, cx + 4.5f, cx - 4.5f}, sy[3] = {cy + 11, cy + 1, cy + 1};
    polygon(S, sx, sy, 3, RGB565(75, 85, 96));
    disc(S, cx, cy, 1.6f, col::BG);
  }

  // Bouton « recentrer » (visible seulement quand la carte a été déplacée)
  if (v.panned && inBand(by, 62, 96)) {
    card(S, 5, 64, 31, 31, 9, col::CARD, col::BORDER);
    circle(S, 20.5f, 79.5f, 6.0f, 1.6f, col::ACCENT);
    disc(S, 20.5f, 79.5f, 2.2f, col::ACCENT);
    for (int k = 0; k < 4; k++) {
      float dxk = k == 0 ? 1 : (k == 1 ? -1 : 0), dyk = k == 2 ? 1 : (k == 3 ? -1 : 0);
      line(S, 20.5f + dxk * 6.5f, 79.5f + dyk * 6.5f, 20.5f + dxk * 10.0f, 79.5f + dyk * 10.0f, 1.6f, col::ACCENT);
    }
  }

  // Boutons de zoom
  if (inBand(by, 88, 156)) {
    mapPlusMinus(S, 284, 89, true, v.canIn);
    mapPlusMinus(S, 284, 125, false, v.canOut);
  }

  // Échelle et attribution (obligatoire)
  if (inBand(by, 160, 180)) {
    hline(S, 8, 176, v.scalePx, col::TXT);
    vline(S, 8, 170, 7, col::TXT);
    vline(S, 8 + v.scalePx - 1, 170, 7, col::TXT);
    text(S, FONT_S11, 8 + v.scalePx + 5, 177, v.scaleTxt, col::TXT);
    text(S, FONT_M9, 314, 177, "© OpenStreetMap", col::SEC, AL_R);
  }

  // Carte du bas : prochain virage, distance restante, arrivée
  if (inBand(by, 182, 224)) {
    card(S, 5, 182, 310, 41, 11, col::CARD, col::BORDER);
    const float icx = 25.5f, icy = 202.5f;
    if (!v.hasTrack) {
      disc(S, icx, icy, 14.5f, col::DOT);
      text(S, FONT_B17, 47, 208, "Pas de trace", col::SEC);
    } else if (!v.onTrack) {
      disc(S, icx, icy, 14.5f, col::AMBER);
      text(S, FONT_B21, icx, icy + 7, "!", col::BG, AL_C);
      text(S, FONT_B17, 47, 204, "Hors trace", col::AMBER);
      text(S, FONT_M10, 47, 217, "rejoindre l'itinéraire", col::SEC);
    } else {
      disc(S, icx, icy, 14.5f, col::ACCENT);
      drawTurnGlyph(S, icx, icy, !v.hasTurn, v.turnA, col::BG);
      char b[28];
      if (v.hasTurn) {
        int w = text(S, FONT_B30, 47, 207, v.turnNum, col::TXT, AL_L, -1);
        text(S, FONT_S11, 47 + w + 3, 207, v.turnUnit, col::SEC);
        text(S, FONT_S11, 47, 219, turnWord(v.turnA, b, sizeof(b)), col::ACCENT);
      } else {
        text(S, FONT_B17, 47, 206, "Tout droit", col::TXT);
        text(S, FONT_M10, 47, 218, "jusqu'au prochain virage", col::SEC);
      }
    }
    int wu = textWidth(FONT_S11, v.remUnit);
    text(S, FONT_S11, 311, 203, v.remUnit, col::SEC, AL_R);
    text(S, FONT_B21, 311 - wu - 3, 203, v.remNum, col::TXT, AL_R, -1);
    char b[20];
    snprintf(b, sizeof(b), "arrivée %s", v.eta);
    text(S, FONT_M10, 311, 217, b, col::SEC, AL_R);
  }

  if (by >= 220) hud::pageDots(S, 2, nPages, false);
}

