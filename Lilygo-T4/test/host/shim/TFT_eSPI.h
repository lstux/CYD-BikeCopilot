#pragma once
// Simulation minimale de TFT_eSPI : tampon 16 bits en mémoire, export en PPM/PNG.
// (copie de CYD/tools/sim/TFT_eSPI.h + les appels utilisés par Lilygo-T4/src/dashboard/main.cpp)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MC_DATUM 4
class TFT_eSPI {
 public:
  TFT_eSPI(int w = 320, int h = 240) : W(w), H(h) { buf = (uint16_t *)calloc((size_t)w * h, 2); }
  virtual ~TFT_eSPI() { free(buf); }
  void init() {}
  void setRotation(int) {}
  void setTextDatum(int) {}
  void setTextColor(uint32_t, uint32_t = 0) {}
  void setTextPadding(int) {}
  int drawString(const char *, int, int, int = 1) { return 0; }
  int width() const { return W; }
  int height() const { return H; }
  void drawPixel(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) buf[y * W + x] = (uint16_t)c;
  }
  uint16_t readPixel(int x, int y) {
    return ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) ? buf[y * W + x] : 0;
  }
  void fillRect(int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) drawPixel(i, j, c);
  }
  void fillScreen(uint32_t c) { fillRect(0, 0, W, H, c); }
  void drawFastHLine(int x, int y, int w, uint32_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint32_t c) { fillRect(x, y, 1, h, c); }
  void writePPM(const char *path) {
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
      uint16_t c = buf[i];
      uint8_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      uint8_t px[3] = {(uint8_t)((r << 3) | (r >> 2)), (uint8_t)((g << 2) | (g >> 4)), (uint8_t)((b << 3) | (b >> 2))};
      fwrite(px, 1, 3, f);
    }
    fclose(f);
  }
  uint16_t *buf;
  int W, H;
};

class TFT_eSprite : public TFT_eSPI {
 public:
  TFT_eSprite(TFT_eSPI *p) : TFT_eSPI(1, 1), parent(p) {}
  void setColorDepth(int) {}
  void *createSprite(int w, int h) {
    free(buf);
    W = w;
    H = h;
    buf = (uint16_t *)calloc((size_t)w * h, 2);
    return buf;
  }
  void deleteSprite() {}
  void fillSprite(uint32_t c) { fillRect(0, 0, W, H, c); }
  void pushSprite(int x, int y) {
    for (int j = 0; j < H; j++)
      for (int i = 0; i < W; i++) parent->drawPixel(x + i, y + j, buf[j * W + i]);
  }
  TFT_eSPI *parent;
};
