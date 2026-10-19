// Simulateur d'interface : dessine chaque page avec des données d'exemple et écrit des images.
// Compilation : voir tools/sim/build.sh
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "page_gauge.h"
#include "page_compass.h"
#include "page_map.h"
#include "page_trips.h"
// --- fausses données pour les pages qui lisent l'état global (Réglages) ---
static const int NB_PAGES = 5;
static TFT_eSPI tft;
struct FakeLoc { bool isValid() { return true; } uint32_t age() { return 0; } double lat() { return 48.1; } double lng() { return 2.3; } };
struct FakeGps { FakeLoc location; } gps;
struct TripLogger { enum State { IDLE, PENDING, ACTIVE }; State st = ACTIVE; State state() const { return st; } void finish() {} } logger;
struct FakeTrip { double distM = 12400; } trip;
#include "settings.h"
static Settings cfg;
static void updateDestination() {}
#include "settingspage.h"

static void save(TFT_eSPI &t, const char *dir, const char *name) {
  char p[256];
  snprintf(p, sizeof(p), "%s/%s.ppm", dir, name);
  t.writePPM(p);
}


// Rendu d'une carte factice (bandes) : même code que la carte réelle, sans tuiles
static void renderMap(TFT_eSPI &tft, const char *dir, const char *name, MapView v, const hud::Status &st) {
  TFT_eSprite band(&tft);
  band.createSprite(320, BAND_H);
  v.st = st;
  nVis = 3;
  Seg route[3] = {{160, 150, 160, 95}, {160, 95, 235, 95}, {235, 95, 235, 6}};
  for (int i = 0; i < 3; i++) vis[i] = route[i];
  int ax[3][4] = {{0, 165, 640, 165}, {65, 0, 65, 240}, {0, 20, 320, 75}};
  for (int b = 0; b < MAP_H / BAND_H; b++) {
    int by = b * BAND_H;
    band.fillSprite(MAP_BG);
    gfx::Surf S = gfx::onSprite(band, 0, by);
    gfx::rect(S, 172, 102, 55, 55, RGB565(12, 27, 21));
    gfx::rect(S, 7, 40, 50, 48, RGB565(12, 27, 21));
    for (int i = 0; i < 3; i++)
      gfx::line(S, ax[i][0], ax[i][1], ax[i][2], ax[i][3], i == 2 ? 5.5f : 3.0f,
                i == 2 ? RGB565(38, 49, 61) : RGB565(23, 32, 42));
    gfx::line(S, 0, 130, 320, 130, 3.0f, RGB565(23, 32, 42));
    gfx::line(S, 100, 0, 100, 240, 3.0f, RGB565(23, 32, 42));
    drawMapOverlay(band, by, v, 1, 5);
    band.pushSprite(0, by);
  }
  save(tft, dir, name);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  TFT_eSprite spr(&tft);
  spr.createSprite(pg_gauge::SW, pg_gauge::SH);

  // --- compteur ---
  tft.fillScreen(col::BG);
  gfx::Surf scr = gfx::onScreen(tft, col::BG);
  hud::Status st;
  st.sats = 9;
  st.fix = true;
  st.rec = 2;
  strcpy(st.clock, "14:05");
  hud::statusBar(scr, st);
  pg_gauge::drawTileFrames(scr);
  const char *vals[6] = {"12.4", "41:20", "128", "18.0", "23.0", "112"};
  const char *units[6] = {"km", "min", "m", "km/h", "km/h", "m"};
  for (int i = 0; i < 6; i++) pg_gauge::drawTileValue(scr, i, vals[i], units[i]);
  gfx::Surf ss = gfx::onSprite(spr, pg_gauge::SX, pg_gauge::SY);
  pg_gauge::draw(ss, 25, 18.0f, 18, 23.0f, true);
  spr.pushSprite(pg_gauge::SX, pg_gauge::SY);
  hud::pageDots(scr, 0, 5);
  save(tft, dir, "gauge");

  // --- direction ---
  tft.fillScreen(col::BG);
  hud::statusBar(scr, st);
  TFT_eSprite sp2(&tft);
  sp2.createSprite(pg_compass::SW, pg_compass::SH);
  gfx::Surf s2 = gfx::onSprite(sp2, pg_compass::SX, pg_compass::SY);
  pg_compass::draw(s2, 312, 18, true, "2.3", "km");
  sp2.pushSprite(pg_compass::SX, pg_compass::SY);
  pg_compass::tile(scr, 4, "CAP", "312°", col::TXT, pg_compass::cardinalName(312), "boussole");
  pg_compass::tile(scr, 251, "DEST.", "18°", col::ACCENT, pg_compass::cardinal16(18), "droite 66°");
  hud::pageDots(scr, 1, 5);
  save(tft, dir, "compass");
  {
    pg_compass::DestMenuState ms;
    ms.active = 1;
    ms.en[1] = true; ms.en[2] = true;
    ms.cur = "domicile";
    pg_compass::drawDestMenu(scr, ms);
    save(tft, dir, "destmenu");
  }

  // --- carte : position, appui long ; carte sans fix ---
  {
    MapView v;
    memset(&v, 0, sizeof(v));
    v.z = 16; v.spd = 18; v.heading = 0; v.hasTrack = v.onTrack = v.hasTurn = true;
    v.turnA = 80; v.destVisible = true; v.dsx = 235; v.dsy = 12; v.hasDest = true;
    strcpy(v.turnNum, "120"); strcpy(v.turnUnit, "m"); strcpy(v.remNum, "4.2"); strcpy(v.remUnit, "km");
    strcpy(v.eta, "14:31"); v.scalePx = 64; strcpy(v.scaleTxt, "100 m");
    v.posX = 160; v.posY = 150; v.homeVisible = true; v.homeX = 60; v.homeY = 110;
    v.canIn = v.canOut = v.panned = true;
    v.fix = true; v.markerOn = true; v.src = CS_LIVE; v.srcName = "";
    v.popOpen = true; v.popX = 200; v.popY = 110;
    renderMap(tft, dir, "map", v, st);

    hud::Status nf = st;
    nf.fix = false; nf.sats = 0; nf.rec = 0;
    MapView w = v;
    w.fix = false; w.markerOn = true; w.src = CS_LAST; w.srcName = "la dernière position connue";
    w.spd = -1; w.hasTurn = false; w.onTrack = false; w.popOpen = false; w.panned = false;
    strcpy(w.remNum, "--"); w.remUnit[0] = 0; strcpy(w.eta, "--:--");
    renderMap(tft, dir, "map_nofix", w, nf);
  }

  // --- réglages ---
  tft.fillScreen(col::BG);
  hud::statusBar(scr, st);
  cfg.tz = 2; cfg.gaugeMin = 15; cfg.logOn = true; cfg.homeOk = true; cfg.homeLat = 48.1234; cfg.homeLon = 2.3456;
  settingsEnter();
  save(tft, dir, "settings");

  // --- trajets (liste) ---
  {
    tft.fillScreen(col::BG);
    hud::statusBar(scr, st);
    pg_trips::Week w; w.km = 56.2f; w.n = 4; w.secs = 3 * 3600 + 8 * 60;
    pg_trips::drawSummary(scr, w);
    TripSummary t[5];
    const float d[5] = {12400, 24800, 7400, 11600, 3100};
    const float mx[5] = {34.2f, 38.1f, 31.6f, 33.0f, 22.0f};
    for (int i = 0; i < 5; i++) {
      t[i].t0 = epochFromDate(2026, 10, 18 - (i == 0 ? 0 : i + (i > 2)), 11 + i, 56, 0);
      t[i].distM = d[i]; t[i].durS = (uint32_t)(d[i] / 5.0f); t[i].avgKmh = 17.5f; t[i].maxKmh = mx[i];
    }
    for (int r = 0; r < pg_trips::ROWS; r++) pg_trips::drawRow(scr, r, t[r], t[r].distM / 24800.0f, 2);
    pg_trips::drawScrollBar(scr, 0, 9);
    hud::pageDots(scr, 3, NB_PAGES);
    save(tft, dir, "trips");

    // --- détail ---
    tft.fillScreen(col::BG);
    hud::statusBar(scr, st);
    TripSummary &c = t[0];
    c.up = 128; c.down = 96;
    pg_trips::drawDetailHeader(scr, c, 2);
    pg_trips::drawDetailTiles(scr, c);
    static TripSeries ser;
    for (int i = 0; i < TripSeries::N; i++) {
      float f = i / (float)(TripSeries::N - 1);
      ser.spd[i] = 17 + 9 * sinf(f * 17.0f) + 5 * sinf(f * 41.0f) + (f > 0.9f ? -8 * (f - 0.9f) * 10 : 0);
      if (ser.spd[i] < 0) ser.spd[i] = 0;
      ser.alt[i] = 100 + 25 * sinf(f * 6.0f) + 12 * f;
    }
    TFT_eSprite cs(&tft);
    cs.createSprite(pg_trips::CH_W, pg_trips::CH_H);
    pg_trips::drawChart(cs, "VITESSE", "max 34.2 km/h", ser.spd, TripSeries::N, 0, 37.6f, col::ACCENT, 140, "27.4 km/h · 7.1 km");
    cs.pushSprite(5, pg_trips::CHART_Y[0]);
    pg_trips::drawChart(cs, "ALTITUDE", "80 – 140 m", ser.alt, TripSeries::N, 80, 140, col::OK, 140, "118 m · 7.1 km");
    cs.pushSprite(5, pg_trips::CHART_Y[1]);
    hud::pageDots(scr, 3, NB_PAGES);
    save(tft, dir, "detail");
    pg_trips::drawConfirmDelete(scr);
    save(tft, dir, "confirm");
  }
  return 0;
}
