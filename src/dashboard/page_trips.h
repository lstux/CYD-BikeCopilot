#pragma once
// Pages « Trajets » (liste) et « Détail » : dessin seul, sans accès à la SD (utilisé aussi par le simulateur).
#include "hud.h"
#include "tripdata.h"

namespace pg_trips {
using namespace gfx;

static const int ROWS = 4, LIST_Y = 68, ROW_H = 38, ROW_W = 297;
static const int CH_W = 310, CH_H = 62;           // taille d'une carte de courbe
static const int CH_X0 = 8, CH_X1 = 302;          // zone de tracé (dans la carte)
static const int CH_Y0 = 20, CH_Y1 = 54;
static const int CHART_Y[2] = {96, 162};          // position des deux cartes à l'écran

static const char *DAYS[7] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
static const char *MONTHS[12] = {"janv", "févr", "mars", "avr", "mai", "juin", "juil", "août", "sept", "oct", "nov", "déc"};

// « Dim 18 oct · 13:56 »
static void fmtDateLong(uint32_t epoch, int tz, char *out, size_t n) {
  int y, mo, d, h, mi, s;
  localParts(epoch, tz, y, mo, d, h, mi, s);
  int64_t days = ((int64_t)epoch + (int64_t)tz * 3600) / 86400;
  int wd = (int)((days + 4) % 7);
  snprintf(out, n, "%s %d %s · %02d:%02d", DAYS[wd], d, MONTHS[mo - 1], h, mi);
}

static void fmtDur(uint32_t secs, char *out, size_t n) {
  if (secs >= 3600) snprintf(out, n, "%luh%02lu", (unsigned long)(secs / 3600), (unsigned long)((secs % 3600) / 60));
  else snprintf(out, n, "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
}

static void clearContent(const Surf &s) { rect(s, 0, 21, 320, 205, col::BG); }

// --- liste ---------------------------------------------------------------------------------
struct Week {
  float km = 0;
  int n = 0;
  uint32_t secs = 0;
};

static void drawSummary(const Surf &s, const Week &w) {
  card(s, 5, 23, 310, 40, 11, col::CARD, col::BORDER);
  Surf c = withBg(s, col::CARD);
  text(c, FONT_S9, 14, 35, "7 DERNIERS JOURS", col::SEC, AL_L, 1);
  char b[16];
  snprintf(b, sizeof(b), "%.1f", w.km);
  int x = 14 + text(c, FONT_B21, 14, 57, b, col::ACCENT, AL_L, -1);
  text(c, FONT_S11, x + 3, 57, "km", col::SEC);
  snprintf(b, sizeof(b), "%d", w.n);
  x = 138 + text(c, FONT_B21, 138, 57, b, col::TXT, AL_L, -1);
  text(c, FONT_S11, x + 3, 57, w.n > 1 ? "trajets" : "trajet", col::SEC);
  fmtDur(w.secs, b, sizeof(b));
  text(c, FONT_B21, 306, 57, b, col::TXT, AL_R, -1);
}

static void drawRow(const Surf &s, int r, const TripSummary &t, float ratio, int tz) {
  const int x = 5, y = LIST_Y + r * ROW_H, w = ROW_W, h = 34;
  card(s, x, y, w, h, 9, col::CARD, col::BORDER);
  // barre proportionnelle à la distance (dégradé de la gauche vers la droite)
  int bw = (int)((w - 8) * clamp01(ratio));
  for (int i = 0; i < bw; i++) {
    uint8_t a = (uint8_t)(46 - 40 * i / (bw > 1 ? bw : 1));
    vline(s, x + 4 + i, y + 3, h - 6, mix(col::ACCENT, col::CARD, a));
  }
  Surf c = withBg(s, col::CARD);
  char a[40], b[48], d[16];
  fmtDateLong(t.t0, tz, a, sizeof(a));
  text(s, FONT_S13, x + 11, y + 15, a, col::TXT);
  fmtDur(t.durS, d, sizeof(d));
  snprintf(b, sizeof(b), "%s · moy %.1f · max %.1f", d, t.avgKmh, t.maxKmh);
  text(s, FONT_M10, x + 11, y + 28, b, col::SEC);
  snprintf(a, sizeof(a), "%.1f", t.distM / 1000.0f);
  int uw = textWidth(FONT_M10, "km");
  text(s, FONT_M10, x + w - 9, y + 22, "km", col::SEC, AL_R);
  text(s, FONT_B17, x + w - 9 - uw - 3, y + 22, a, col::TXT, AL_R, -1);
}

static void drawScrollBar(const Surf &s, int top, int n) {
  const int x = 309, y = LIST_Y, h = ROWS * ROW_H - 4;
  rect(s, x - 1, y, 8, h, col::BG);
  roundRect(s, x, y, 4, h, 2, RGB565(18, 25, 34));
  if (n > ROWS) {
    int th = h * ROWS / n;
    if (th < 14) th = 14;
    int ty = y + (h - th) * top / (n - ROWS);
    roundRect(s, x, ty, 4, th, 2, RGB565(75, 85, 96));
  }
}

static void drawEmpty(const Surf &s, bool sd) {
  text(s, FONT_B17, 160, 125, sd ? "Aucun trajet" : "Carte SD absente", col::SEC, AL_C);
  if (sd) text(s, FONT_M10, 160, 143, "les trajets de plus de 100 m apparaissent ici", col::TER, AL_C);
}

// --- détail --------------------------------------------------------------------------------
static void drawDetailHeader(const Surf &s, const TripSummary &t, int tz) {
  rect(s, 0, 21, 320, 24, col::BG);
  disc(s, 17, 33, 11.5f, col::BORDER);
  disc(s, 17, 33, 10.5f, col::CARD);
  line(s, 18.5f, 28, 14.5f, 33, 1.8f, col::TXT);
  line(s, 14.5f, 33, 18.5f, 38, 1.8f, col::TXT);
  char a[40];
  fmtDateLong(t.t0, tz, a, sizeof(a));
  text(s, FONT_S13, 36, 38, a, col::TXT);
  card(s, 244, 24, 71, 20, 8, col::CARD, col::BORDER);
  text(withBg(s, col::CARD), FONT_S11, 279, 38, "Supprimer", col::RED, AL_C);
}

// Fenêtre de confirmation de suppression (zone des boutons : y 118..148, gauche = annuler)
static void drawConfirmDelete(const Surf &s) {
  card(s, 40, 86, 240, 68, 12, col::CARD, col::ACCENT);
  Surf c = withBg(s, col::CARD);
  text(c, FONT_S13, 160, 108, "Supprimer ce trajet ?", col::TXT, AL_C);
  roundRect(s, 56, 120, 94, 24, 8, col::SURF2);
  text(withBg(s, col::SURF2), FONT_S11, 103, 136, "Annuler", col::TXT, AL_C);
  roundRect(s, 170, 120, 94, 24, 8, col::RED);
  text(withBg(s, col::RED), FONT_S11, 217, 136, "Supprimer", col::BG, AL_C);
}

static void tile(const Surf &s, int i, const char *label, const char *value, uint16_t vc, const char *sub) {
  const int x = 5 + i * 78, y = 48, w = 74, h = 44;
  card(s, x, y, w, h, 9, col::CARD, col::BORDER);
  Surf c = withBg(s, col::CARD);
  text(c, FONT_S9, x + 7, y + 12, label, col::SEC, AL_L, 1);
  text(c, FONT_B21, x + 6, y + 31, value, vc, AL_L, -1);
  text(c, FONT_M9, x + 7, y + 41, sub, col::SEC);
}

static void drawDetailTiles(const Surf &s, const TripSummary &t) {
  char a[16], b[24];
  snprintf(a, sizeof(a), "%.1f", t.distM / 1000.0f);
  tile(s, 0, "DISTANCE", a, col::TXT, "km");
  fmtDur(t.durS, a, sizeof(a));
  tile(s, 1, "DURÉE", a, col::TXT, t.durS >= 3600 ? "h min" : "min:s");
  snprintf(a, sizeof(a), "%.1f", t.avgKmh);
  snprintf(b, sizeof(b), "max %.1f", t.maxKmh);
  tile(s, 2, "VIT. MOY.", a, col::ACCENT, b);
  snprintf(a, sizeof(a), "+%d", (int)t.up);
  snprintf(b, sizeof(b), "−%d m", (int)t.down);
  tile(s, 3, "DÉNIVELÉ", a, col::OK, b);
}

// Une carte de courbe dans un sprite de CH_W x CH_H. `cursor` : colonne (0..CH_X1-CH_X0) ou -1.
static void drawChart(TFT_eSprite &sp, const char *title, const char *right, const float *v, int n, float vmin,
                      float vmax, uint16_t lineCol, int cursor, const char *cursorTxt) {
  Surf S = onSprite(sp, 0, 0);
  sp.fillSprite(col::BG);
  card(S, 0, 0, CH_W, CH_H, 10, col::CARD, col::BORDER);
  Surf c = withBg(S, col::CARD);
  const int W = CH_X1 - CH_X0 + 1, H = CH_Y1 - CH_Y0;
  float span = vmax - vmin;
  if (span < 0.01f) span = 1;
  static int16_t ys[CH_X1 - CH_X0 + 1];
  for (int i = 0; i < W; i++) {
    float f = (float)i / (W - 1) * (n - 1);
    int i0 = (int)f;
    int i1 = i0 + 1 < n ? i0 + 1 : i0;
    float val = v[i0] + (v[i1] - v[i0]) * (f - i0);
    float y = CH_Y1 - (val - vmin) / span * H;
    ys[i] = (int16_t)(y < CH_Y0 ? CH_Y0 : (y > CH_Y1 ? CH_Y1 : y + 0.5f));
  }
  // aire dégradée sous la courbe
  for (int i = 0; i < W; i++) {
    int top = ys[i];
    for (int y = top; y <= CH_Y1; y++) {
      uint8_t a = (uint8_t)(80 - 68 * (y - top) / (CH_Y1 - top + 1));
      sp.drawPixel(CH_X0 + i, y, mix(lineCol, col::CARD, a));
    }
  }
  hline(S, CH_X0, CH_Y1 + 1, W, col::BORDER);
  for (int i = 0; i + 1 < W; i++) line(S, CH_X0 + i, ys[i], CH_X0 + i + 1, ys[i + 1], 1.5f, lineCol);
  text(c, FONT_S9, 12, 13, title, col::SEC, AL_L, 1);
  if (cursor >= 0 && cursor < W) {
    int cx = CH_X0 + cursor;
    vline(S, cx, CH_Y0 - 2, CH_Y1 - CH_Y0 + 4, mix(col::TXT, col::CARD, 130));
    disc(S, cx, ys[cursor], 3.6f, col::CARD);
    circle(S, cx, ys[cursor], 2.6f, 1.6f, lineCol);
    text(c, FONT_S11, 300, 13, cursorTxt, col::TXT, AL_R);
  } else {
    text(c, FONT_M9, 300, 13, right, col::SEC, AL_R);
  }
}

}  // namespace pg_trips
