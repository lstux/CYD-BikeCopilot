// Simulateur d'interface : dessine chaque page avec des données d'exemple et écrit des images.
// Compilation : voir tools/sim/build.sh
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "page_gauge.h"
#include "page_compass.h"

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
  return 0;
}
