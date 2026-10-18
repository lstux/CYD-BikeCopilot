#pragma once
// Page « Trajets » : liste des trajets enregistrés (défilement au doigt), puis détail
// (tuiles + courbes vitesse / altitude avec curseur). Le dessin est dans page_trips.h.
// Inclus par main.cpp (utilise tft, spr, col::, cfg, gps, trips.h).
#include "page_trips.h"

#define HIST_MAX 48

static TripSummary hist[HIST_MAX];
static int histN = 0;
static int histTop = 0;   // rang (0 = plus récent) de la première ligne affichée
static int histSel = -1;  // -1 = liste, sinon rang du trajet affiché en détail
static TripSummary histCur;
static TripSeries histSer;
static int histCursor = -1;  // colonne du curseur dans les courbes, -1 si absent
static int histAcc = 0;      // glissé vertical accumulé (pixels)
static bool histSprOk = false;

static uint32_t histNowEpoch() {
  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024)
    return epochFromDate(gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(),
                         gps.time.second());
  return histN > 0 ? hist[histN - 1].t0 : 0;
}

static void drawHistRows() {
  using namespace pg_trips;
  Surf s = scr();
  rect(s, 0, LIST_Y - 1, 320, ROWS * ROW_H, col::BG);
  float maxD = 1;
  for (int i = 0; i < histN; i++)
    if (hist[i].distM > maxD) maxD = hist[i].distM;
  for (int r = 0; r < ROWS; r++) {
    int idx = histTop + r;
    if (idx >= histN) break;
    const TripSummary &t = hist[histN - 1 - idx];
    drawRow(s, r, t, t.distM / maxD, cfg.tz);
  }
  drawScrollBar(s, histTop, histN);
}

static void drawHistList() {
  using namespace pg_trips;
  Surf s = scr();
  clearContent(s);
  hud::pageDots(s, 3, NB_PAGES);
  if (histN == 0) {
    drawEmpty(s, sdOk);
    return;
  }
  Week w;
  uint32_t now = histNowEpoch();
  for (int i = 0; i < histN; i++) {
    const TripSummary &t = hist[i];
    if (t.t0 + 7UL * 86400UL >= now) {
      w.km += t.distM / 1000.0f;
      w.n++;
      w.secs += t.durS;
    }
  }
  drawSummary(s, w);
  drawHistRows();
}

// Les deux cartes de courbes (le sprite est conservé tant qu'on est dans le détail)
static void histDrawCharts() {
  using namespace pg_trips;
  if (!histSprOk) return;
  const int W = CH_X1 - CH_X0 + 1, N = TripSeries::N;
  int idx = histCursor >= 0 ? histCursor * (N - 1) / (W - 1) : 0;
  char r1[32], r2[32], c1[40], c2[40];
  float km = histCur.distM / 1000.0f * idx / (N - 1);
  snprintf(r1, sizeof(r1), "max %.1f km/h", histCur.maxKmh);
  snprintf(r2, sizeof(r2), "%.0f – %.0f m", histSer.altMin, histSer.altMax);
  snprintf(c1, sizeof(c1), "%.1f km/h · %.1f km", histSer.spd[idx], km);
  snprintf(c2, sizeof(c2), "%.0f m · %.1f km", histSer.alt[idx], km);
  drawChart(spr, "VITESSE", r1, histSer.spd, N, 0, histSer.spdMax * 1.1f, col::ACCENT, histCursor, c1);
  spr.pushSprite(5, CHART_Y[0]);
  drawChart(spr, "ALTITUDE", r2, histSer.alt, N, histSer.altMin, histSer.altMax, col::OK, histCursor, c2);
  spr.pushSprite(5, CHART_Y[1]);
}

static void drawHistDetail() {
  using namespace pg_trips;
  Surf s = scr();
  clearContent(s);
  drawDetailHeader(s, histCur, cfg.tz);
  drawDetailTiles(s, histCur);
  hud::pageDots(s, 3, NB_PAGES);
  spr.deleteSprite();
  spr.setColorDepth(16);
  histSprOk = spr.createSprite(CH_W, CH_H) != nullptr;
  histCursor = -1;
  if (histSprOk) histDrawCharts();
  else text(s, FONT_B17, 160, 150, "Mémoire insuffisante", col::AMBER, AL_C);
}

static void histEnter() {
  histSprOk = false;
  if (sdOk) histN = indexLoad(hist, HIST_MAX);
  else histN = 0;
  if (histTop >= histN) histTop = 0;
  if (histSel >= histN) histSel = -1;
  if (histSel >= 0) drawHistDetail();
  else drawHistList();
}

static void histBack() {
  histSel = -1;
  histSprOk = false;
  spr.deleteSprite();
  drawHistList();
}

static void histOpen(int rank) {
  using namespace pg_trips;
  const TripSummary &t = hist[histN - 1 - rank];
  Surf s = scr();
  clearContent(s);
  text(s, FONT_B17, 160, 125, "Analyse…", col::SEC, AL_C);
  char path[64];
  tripPath(t.name, path, sizeof(path));
  histCur = t;
  if (tripAnalyze(path, histCur, &histSer)) {
    histSel = rank;
    drawHistDetail();
  } else {
    histSel = -1;
    drawHistList();
  }
}

// Glissé vertical dans la liste (une ligne tous les ~22 px)
static void histDrag(int dy) {
  if (histSel >= 0 || histN <= pg_trips::ROWS) return;
  histAcc += dy;
  int old = histTop;
  while (histAcc <= -22 && histTop < histN - pg_trips::ROWS) { histTop++; histAcc += 22; }
  while (histAcc >= 22 && histTop > 0) { histTop--; histAcc -= 22; }
  if (histTop == 0 && histAcc > 0) histAcc = 0;
  if (histTop >= histN - pg_trips::ROWS && histAcc < 0) histAcc = 0;
  if (histTop != old) drawHistRows();
}

// Curseur des courbes : x = position du doigt à l'écran
static void histCursorAt(int x) {
  using namespace pg_trips;
  if (histSel < 0 || !histSprOk) return;
  int c = x - 5 - CH_X0;
  const int W = CH_X1 - CH_X0 + 1;
  if (c < 0) c = 0;
  if (c > W - 1) c = W - 1;
  if (c == histCursor) return;
  histCursor = c;
  histDrawCharts();
}

// Retourne true si le toucher a été traité par la page
static bool histTap(int x, int y) {
  using namespace pg_trips;
  if (histSel >= 0) {
    if (y < 46) histBack();  // en-tête : retour à la liste
    else if (y >= CHART_Y[0]) histCursorAt(x);
    return true;
  }
  histAcc = 0;
  if (x >= 303) {  // barre de défilement : page précédente / suivante
    int old = histTop;
    if (y < LIST_Y + ROWS * ROW_H / 2) histTop = histTop > ROWS ? histTop - ROWS : 0;
    else if (histTop + ROWS < histN) histTop = histTop + ROWS > histN - ROWS ? histN - ROWS : histTop + ROWS;
    if (histTop != old) drawHistRows();
    return true;
  }
  int r = (y - LIST_Y) / ROW_H;
  if (y >= LIST_Y && r >= 0 && r < ROWS && histTop + r < histN) histOpen(histTop + r);
  return true;
}
