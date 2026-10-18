// Simulateur d'interface : dessine chaque page avec des données d'exemple et écrit des images.
// Compilation : voir tools/sim/build.sh
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "page_gauge.h"
#include "page_compass.h"
#include "page_map.h"
static const int NB_PAGES = 5;

static void save(TFT_eSPI &t, const char *dir, const char *name) {
  char p[256];
  snprintf(p, sizeof(p), "%s/%s.ppm", dir, name);
  t.writePPM(p);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  TFT_eSPI tft;
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

  // --- carte ---
  {
    TFT_eSprite band(&tft);
    band.createSprite(320, BAND_H);
    MapView v;
    memset(&v, 0, sizeof(v));
    v.z = 16; v.spd = 18; v.heading = 0; v.hasTrack = v.onTrack = v.hasTurn = true;
    v.turnA = 80; v.destVisible = true; v.dsx = 235; v.dsy = 12;
    strcpy(v.turnNum, "120"); strcpy(v.turnUnit, "m"); strcpy(v.remNum, "4.2"); strcpy(v.remUnit, "km");
    strcpy(v.eta, "14:31"); v.scalePx = 64; strcpy(v.scaleTxt, "100 m");
    v.st = st;
    int ax[3][4] = {{0, 165, 640, 165}, {65, 0, 65, 240}, {0, 20, 320, 75}};
    Seg route[3] = {{160, 150, 160, 95}, {160, 95, 235, 95}, {235, 95, 235, 6}};
    nVis = 3;
    for (int i = 0; i < 3; i++) vis[i] = route[i];
    for (int b = 0; b < MAP_H / BAND_H; b++) {
      int by = b * BAND_H;
      band.fillSprite(MAP_BG);
      gfx::Surf S = gfx::onSprite(band, 0, by);
      gfx::rect(S, 172, 102, 55, 55, RGB565(12, 27, 21));
      gfx::rect(S, 7, 40, 50, 48, RGB565(12, 27, 21));
      for (int i = 0; i < 3; i++) gfx::line(S, ax[i][0], ax[i][1], ax[i][2], ax[i][3], i == 2 ? 5.5f : 3.0f, i == 2 ? RGB565(38, 49, 61) : RGB565(23, 32, 42));
      gfx::line(S, 0, 130, 320, 130, 3.0f, RGB565(23, 32, 42));
      gfx::line(S, 100, 0, 100, 240, 3.0f, RGB565(23, 32, 42));
      drawMapOverlay(band, by, v, 1, NB_PAGES);
      band.pushSprite(0, by);
    }
    save(tft, dir, "map");
  }
  return 0;
}
