#pragma once
// Page « Trajets » : liste des trajets enregistrés, puis détail (stats + courbes vitesse / altitude).
// Inclus par main.cpp (utilise tft, col::, cfg, trips.h).

#define HIST_MAX   48
#define HIST_ROWS  5

static TripSummary hist[HIST_MAX];
static int histN = 0;
static int histTop = 0;      // rang (0 = plus récent) de la première ligne affichée
static int histSel = -1;     // -1 = liste, sinon rang du trajet affiché en détail
static TripSummary histCur;
static TripSeries histSer;

static void fmtDate(uint32_t epoch, char *out, size_t n) {
  int y, mo, d, h, mi, s;
  localParts(epoch, cfg.tz, y, mo, d, h, mi, s);
  snprintf(out, n, "%02d/%02d %02d:%02d", d, mo, h, mi);
}

static void fmtDur(uint32_t secs, char *out, size_t n) {
  if (secs >= 3600)
    snprintf(out, n, "%luh%02lu", (unsigned long)(secs / 3600), (unsigned long)((secs % 3600) / 60));
  else
    snprintf(out, n, "%lu min", (unsigned long)((secs + 30) / 60));
}

static void drawArrowBtn(int x, int y, int w, int h, bool up, bool enabled) {
  tft.fillRoundRect(x, y, w, h, 6, col::TRACK);
  uint16_t c = enabled ? col::WHITE : col::RULE;
  int cx = x + w / 2, cy = y + h / 2;
  if (up)
    tft.fillTriangle(cx - 9, cy + 6, cx + 9, cy + 6, cx, cy - 7, c);
  else
    tft.fillTriangle(cx - 9, cy - 6, cx + 9, cy - 6, cx, cy + 7, c);
}

static void drawHistList() {
  tft.fillRect(0, 24, 320, 216, col::BG);
  tft.setTextPadding(0);
  if (histN == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(col::DIM, col::BG);
    tft.drawString(sdOk ? "Aucun trajet enregistre" : "Carte SD absente", 160, 120, 2);
    return;
  }
  for (int r = 0; r < HIST_ROWS; r++) {
    int idx = histTop + r;
    if (idx >= histN) break;
    const TripSummary &s = hist[histN - 1 - idx];
    int y = 28 + r * 38;
    char a[24], b[24], c[40];
    tft.fillRoundRect(4, y, 262, 34, 5, col::TRACK);
    fmtDate(s.t0, a, sizeof(a));
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(col::WHITE, col::TRACK);
    tft.drawString(a, 12, y + 2, 2);
    snprintf(b, sizeof(b), "%.1f km", s.distM / 1000.0f);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(b, 258, y + 2, 2);
    fmtDur(s.durS, a, sizeof(a));
    snprintf(c, sizeof(c), "%s   moy %.1f   max %.0f", a, s.avgKmh, s.maxKmh);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(col::LABEL, col::TRACK);
    tft.drawString(c, 12, y + 18, 2);
  }
  drawArrowBtn(272, 28, 44, 80, true, histTop > 0);
  drawArrowBtn(272, 116, 44, 80, false, histTop + HIST_ROWS < histN);
}

// Courbe en aire : valeurs v[0..n-1] ramenées sur w pixels
static void drawGraph(int x, int y, int w, int h, const float *v, int n, float vmin, float vmax, uint16_t line,
                      uint16_t fill, const char *title) {
  tft.fillRect(x - 2, y, w + 4, h, col::BG);
  tft.drawFastHLine(x, y + h - 1, w, col::RULE);
  float span = vmax - vmin;
  if (span < 0.01f) span = 1;
  int prev = -1;
  for (int px = 0; px < w; px++) {
    float val = v[px * (n - 1) / (w - 1)];
    int yy = (int)((val - vmin) / span * (h - 3));
    if (yy < 0) yy = 0;
    if (yy > h - 3) yy = h - 3;
    int top = y + h - 2 - yy;
    tft.drawFastVLine(x + px, top, y + h - 1 - top, fill);
    if (prev >= 0) {  // relie au point précédent pour une ligne continue
      int a = prev < top ? prev : top, b = prev < top ? top : prev;
      tft.drawFastVLine(x + px, a, b - a + 1, line);
    } else {
      tft.drawPixel(x + px, top, line);
    }
    tft.drawPixel(x + px, top, line);
    prev = top;
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(0);
  tft.setTextColor(col::TEXT2, col::BG);
  tft.drawString(title, x + 4, y + 1, 2);
}

static void drawHistDetail() {
  tft.fillRect(0, 24, 320, 216, col::BG);
  const TripSummary &s = histCur;
  char a[40], b[40];
  tft.setTextPadding(0);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col::DIM, col::BG);
  tft.drawString("<", 6, 26, 2);
  fmtDate(s.t0, a, sizeof(a));
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString(a, 22, 26, 2);
  snprintf(a, sizeof(a), "%.1f km", s.distM / 1000.0f);
  tft.drawString(a, 112, 26, 2);
  fmtDur(s.durS, a, sizeof(a));
  tft.drawString(a, 200, 26, 2);

  snprintf(a, sizeof(a), "moy %.1f", s.avgKmh);
  snprintf(b, sizeof(b), "max %.1f", s.maxKmh);
  tft.setTextColor(col::LABEL, col::BG);
  tft.drawString(a, 6, 44, 2);
  tft.drawString(b, 90, 44, 2);
  snprintf(a, sizeof(a), "D+ %d m", (int)s.up);
  snprintf(b, sizeof(b), "D- %d m", (int)s.down);
  tft.setTextColor(col::GREEN, col::BG);
  tft.drawString(a, 180, 44, 2);
  tft.setTextColor(col::ORANGE, col::BG);
  tft.drawString(b, 252, 44, 2);

  char t1[40], t2[48];
  snprintf(t1, sizeof(t1), "Vitesse (km/h)  max %.0f", histSer.spdMax);
  snprintf(t2, sizeof(t2), "Altitude (m)  %.0f a %.0f", histSer.altMin, histSer.altMax);
  drawGraph(10, 66, 300, 78, histSer.spd, TripSeries::N, 0, histSer.spdMax * 1.05f, col::CYAN, RGB565(0, 55, 70), t1);
  drawGraph(10, 150, 300, 78, histSer.alt, TripSeries::N, histSer.altMin, histSer.altMax, col::GREEN,
            RGB565(0, 60, 25), t2);
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(col::DIM, col::BG);
  snprintf(a, sizeof(a), "%.1f km", s.distM / 1000.0f);
  tft.drawString(a, 310, 239, 2);
  tft.setTextDatum(BL_DATUM);
  tft.drawString("0", 10, 239, 2);
}

static void histEnter() {
  if (sdOk) histN = indexLoad(hist, HIST_MAX);
  else histN = 0;
  if (histTop >= histN) histTop = 0;
  if (histSel >= histN) histSel = -1;
  if (histSel >= 0) drawHistDetail();
  else drawHistList();
}

static void histOpen(int rank) {
  const TripSummary &s = hist[histN - 1 - rank];
  tft.fillRect(0, 24, 320, 216, col::BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(0);
  tft.setTextColor(col::DIM, col::BG);
  tft.drawString("Analyse...", 160, 120, 4);
  char path[64];
  tripPath(s.name, path, sizeof(path));
  histCur = s;
  if (tripAnalyze(path, histCur, &histSer)) {
    histSel = rank;
    drawHistDetail();
  } else {
    histSel = -1;
    drawHistList();
  }
}

// Retourne true si le toucher a été traité par la page
static bool histTap(int x, int y) {
  if (histSel >= 0) {
    if (y < 64) {  // en-tête : retour à la liste
      histSel = -1;
      drawHistList();
    }
    return true;
  }
  if (x >= 268) {
    if (y < 112 && histTop > 0) {
      histTop = histTop > HIST_ROWS ? histTop - HIST_ROWS : 0;
      drawHistList();
    } else if (y >= 112 && histTop + HIST_ROWS < histN) {
      histTop += HIST_ROWS;
      drawHistList();
    }
    return true;
  }
  int r = (y - 28) / 38;
  if (y >= 28 && r >= 0 && r < HIST_ROWS && histTop + r < histN) histOpen(histTop + r);
  return true;
}
