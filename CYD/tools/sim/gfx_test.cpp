#include <Arduino.h>
#include <TFT_eSPI.h>
#include "gfx.h"

#define RGB565(r, g, b) ((uint16_t)(((((int)(r)) & 0xF8) << 8) | ((((int)(g)) & 0xFC) << 3) | ((((int)(b)) & 0xFF) >> 3)))
int main() {
  TFT_eSPI tft;
  uint16_t BG = RGB565(5, 7, 10);
  tft.fillScreen(BG);
  gfx::Surf s = gfx::onScreen(tft, BG);
  uint16_t W = 0xFFFF, G = RGB565(142, 153, 166);
  gfx::text(s, FONT_B56, 10, 60, "18 07", W);
  gfx::text(s, FONT_B30, 10, 100, "12.4 41:20 -+°", W);
  gfx::text(s, FONT_B21, 10, 130, "12.4 km 0123456789", W);
  gfx::text(s, FONT_B17, 10, 155, "Nord-ouest Tourner à droite", W);
  gfx::text(s, FONT_S13, 10, 175, "Dim 18 oct · 13:56 Échelle", W);
  gfx::text(s, FONT_S11, 10, 192, "DISTANCE TEMPS MOY. MAX 9 sat", G);
  gfx::text(s, FONT_S9, 10, 206, "DISTANCE TEMPS MOY. MAX ALT.", G, gfx::AL_L, 1);
  gfx::text(s, FONT_M10, 10, 220, "km/h m 41:20 · moy 18.0 · max 34.2", G);
  gfx::text(s, FONT_M9, 10, 234, "arrivée 14:31  © OpenStreetMap", G);
  auto grad = [](float t) { return RGB565(25 + 200 * t, 211 - 40 * t, 255 - 220 * t); };
  gfx::arc(s, 250, 70, 40, 51, -135, 90, grad);
  gfx::arc(s, 250, 70, 40, 51, 100, 135, [](float) { return (uint16_t)RGB565(30, 40, 50); });
  gfx::disc(s, 250, 70, 8, RGB565(255, 77, 46));
  float xs[] = {247, 253, 251, 249}, ys[] = {70, 70, 20, 20};
  gfx::polygon(s, xs, ys, 4, RGB565(255, 77, 46));
  gfx::line(s, 200, 150, 300, 200, 3, RGB565(25, 211, 255));
  gfx::card(s, 200, 170, 100, 50, 8, RGB565(14, 19, 25), RGB565(28, 36, 46));
  tft.writePPM("/tmp/claude-0/-home-claude/efc6d012-5887-52da-8b13-5ae337dfcb64/scratchpad/gfx_test.ppm");
  return 0;
}
