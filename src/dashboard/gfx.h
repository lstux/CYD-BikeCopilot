#pragma once
// Primitives de dessin anti-crénelées (AA) et texte lissé, au-dessus de TFT_eSPI.
//
// Tout se dessine dans une « surface » (Surf) : l'écran ou un sprite. Les coordonnées passées
// aux fonctions sont des coordonnées ÉCRAN ; la surface les translate (ox, oy) et rogne.
//   - sprite : on peut relire les pixels (readable) pour mélanger correctement les bords ;
//   - écran  : relire serait trop lent, on suppose donc un fond uni connu (bg).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "fonts_data.h"

namespace gfx {

struct Surf {
  TFT_eSPI *g;
  int ox, oy;     // position de la surface sur l'écran
  int w, h;       // dimensions
  bool readable;  // true : sprite, les pixels peuvent être relus
  uint16_t bg;    // fond supposé si non relisible
};

static inline Surf onScreen(TFT_eSPI &t, uint16_t bg, int w = 320, int h = 240) {
  Surf s = {&t, 0, 0, w, h, false, bg};
  return s;
}
static inline Surf onSprite(TFT_eSprite &sp, int ox, int oy) {
  Surf s = {&sp, ox, oy, (int)sp.width(), (int)sp.height(), true, 0};
  return s;
}
// Même surface mais avec un autre fond supposé (texte sur une carte colorée, par exemple)
static inline Surf withBg(Surf s, uint16_t bg) {
  s.bg = bg;
  return s;
}

// --- couleurs ----------------------------------------------------------------------------
static inline uint16_t mix(uint16_t fg, uint16_t bg, uint8_t a) {
  if (a == 255) return fg;
  if (a == 0) return bg;
  uint32_t rxb = bg & 0xF81F;
  rxb += ((fg & 0xF81F) - rxb) * (a >> 2) >> 6;
  uint32_t xgx = bg & 0x07E0;
  xgx += ((fg & 0x07E0) - xgx) * a >> 8;
  return (uint16_t)((rxb & 0xF81F) | (xgx & 0x07E0));
}

static inline uint8_t covToAlpha(float c) {
  if (c <= 0) return 0;
  if (c >= 1) return 255;
  return (uint8_t)(c * 255.0f + 0.5f);
}

// --- pixels et rectangles ----------------------------------------------------------------
static inline void px(const Surf &s, int x, int y, uint16_t c, uint8_t a = 255) {
  x -= s.ox;
  y -= s.oy;
  if ((unsigned)x >= (unsigned)s.w || (unsigned)y >= (unsigned)s.h || a == 0) return;
  if (a < 255) c = mix(c, s.readable ? s.g->readPixel(x, y) : s.bg, a);
  s.g->drawPixel(x, y, c);
}

// La boîte [x0,x1]x[y0,y1] (coordonnées écran) touche-t-elle la surface ?
static inline bool hit(const Surf &s, int x0, int y0, int x1, int y1) {
  return !(x1 < s.ox || y1 < s.oy || x0 >= s.ox + s.w || y0 >= s.oy + s.h);
}

static inline void rect(const Surf &s, int x, int y, int w, int h, uint16_t c) {
  int x1 = x + w, y1 = y + h;
  if (x < s.ox) x = s.ox;
  if (y < s.oy) y = s.oy;
  if (x1 > s.ox + s.w) x1 = s.ox + s.w;
  if (y1 > s.oy + s.h) y1 = s.oy + s.h;
  if (x1 <= x || y1 <= y) return;
  s.g->fillRect(x - s.ox, y - s.oy, x1 - x, y1 - y, c);
}

static inline void hline(const Surf &s, int x, int y, int w, uint16_t c) { rect(s, x, y, w, 1, c); }
static inline void vline(const Surf &s, int x, int y, int h, uint16_t c) { rect(s, x, y, 1, h, c); }

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// --- disque et anneau --------------------------------------------------------------------
static void disc(const Surf &s, float cx, float cy, float r, uint16_t c, uint8_t alpha = 255) {
  int y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);
  if (!hit(s, (int)floorf(cx - r - 1), y0, (int)ceilf(cx + r + 1), y1)) return;
  if (y0 < s.oy) y0 = s.oy;
  if (y1 > s.oy + s.h - 1) y1 = s.oy + s.h - 1;
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy;
    float ro = r + 1;
    if (fabsf(dy) > ro) continue;
    int xo = (int)ceilf(sqrtf(ro * ro - dy * dy));
    float ri = r - 1;
    int xi = (ri > 0 && fabsf(dy) < ri) ? (int)floorf(sqrtf(ri * ri - dy * dy)) : -1;
    int xa = (int)floorf(cx) - xo, xb = (int)floorf(cx) + xo;
    for (int x = xa; x <= xb; x++) {
      float dx = x + 0.5f - cx;
      if (xi >= 0 && fabsf(dx) < xi) {  // intérieur plein : on saute directement
        int xe = (int)floorf(cx + xi - 0.5f);
        if (alpha == 255) rect(s, x, y, xe - x + 1, 1, c);
        else for (int k = x; k <= xe; k++) px(s, k, y, c, alpha);
        x = xe;
        continue;
      }
      float d = sqrtf(dx * dx + dy * dy);
      px(s, x, y, c, (uint8_t)((uint16_t)covToAlpha(r - d + 0.5f) * alpha / 255));
    }
  }
}

// Contour circulaire d'épaisseur t
static void circle(const Surf &s, float cx, float cy, float r, float t, uint16_t c) {
  float ro = r + t / 2, ri = r - t / 2;
  int y0 = (int)floorf(cy - ro - 1), y1 = (int)ceilf(cy + ro + 1);
  if (!hit(s, (int)floorf(cx - ro - 1), y0, (int)ceilf(cx + ro + 1), y1)) return;
  if (y0 < s.oy) y0 = s.oy;
  if (y1 > s.oy + s.h - 1) y1 = s.oy + s.h - 1;
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy;
    if (fabsf(dy) > ro + 1) continue;
    int xo = (int)ceilf(sqrtf((ro + 1) * (ro + 1) - dy * dy));
    for (int x = (int)floorf(cx) - xo; x <= (int)floorf(cx) + xo; x++) {
      float dx = x + 0.5f - cx;
      float d = sqrtf(dx * dx + dy * dy);
      px(s, x, y, c, covToAlpha(clamp01(ro - d + 0.5f) * clamp01(d - ri + 0.5f)));
    }
  }
}

// --- arctangente rapide (erreur ~1e-5 rad) ----------------------------------------------------
static inline float fatan2(float y, float x) {
  float ax = fabsf(x), ay = fabsf(y);
  float mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
  if (mx == 0) return 0;
  float a = mn / mx, sq = a * a;
  float r = ((-0.0464964749f * sq + 0.15931422f) * sq - 0.327622764f) * sq * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0) r = 3.14159274f - r;
  if (y < 0) r = -r;
  return r;
}

// Arc d'anneau entre les rayons rIn et rOut, de l'angle a0 à a1 (degrés, 0 = haut, sens horaire).
// color(t) donne la couleur à la fraction t (0..1) de l'arc. Extrémités arrondies si round.
template <class F>
static void arc(const Surf &s, float cx, float cy, float rIn, float rOut, float a0, float a1, F color,
                bool round = true) {
  float sweep = a1 - a0;
  if (sweep <= 0.01f) return;
  const float D2R = 0.017453292f;
  float u0x = sinf(a0 * D2R), u0y = -cosf(a0 * D2R);
  float u1x = sinf(a1 * D2R), u1y = -cosf(a1 * D2R);
  bool big = sweep > 180.0f;
  bool full = sweep >= 359.9f;
  int y0 = (int)floorf(cy - rOut - 1), y1 = (int)ceilf(cy + rOut + 1);
  if (hit(s, (int)floorf(cx - rOut - 1), y0, (int)ceilf(cx + rOut + 1), y1)) {
    if (y0 < s.oy) y0 = s.oy;
    if (y1 > s.oy + s.h - 1) y1 = s.oy + s.h - 1;
    float ro1 = (rOut - 1) * (rOut - 1), ro2 = (rOut + 1) * (rOut + 1);
    float ri1 = (rIn + 1) * (rIn + 1), ri2 = (rIn > 1 ? (rIn - 1) * (rIn - 1) : 0);
    float inv = 1.0f / sweep;
    for (int y = y0; y <= y1; y++) {
      float dy = y + 0.5f - cy;
      if (fabsf(dy) > rOut + 1) continue;
      int xo = (int)ceilf(sqrtf(ro2 - dy * dy));
      int xa = (int)floorf(cx) - xo, xb = (int)floorf(cx) + xo;
      if (xa < s.ox) xa = s.ox;
      if (xb > s.ox + s.w - 1) xb = s.ox + s.w - 1;
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - cx;
        float d2 = dx * dx + dy * dy;
        if (d2 > ro2 || d2 < ri2) continue;
        if (!full) {
          float c0 = u0x * dy - u0y * dx, c1 = u1x * dy - u1y * dx;
          bool in = big ? !(c1 > 0 && c0 < 0) : (c0 >= 0 && c1 <= 0);
          if (!in) continue;
        }
        uint8_t a = 255;
        if (d2 > ro1 || d2 < ri1) {
          float d = sqrtf(d2);
          a = covToAlpha(clamp01(rOut - d + 0.5f) * clamp01(d - rIn + 0.5f));
        }
        float ang = fatan2(dx, -dy) * 57.2957795f - a0;
        while (ang < 0) ang += 360.0f;
        while (ang >= 360.0f) ang -= 360.0f;
        float t = ang * inv;
        px(s, x, y, color(t > 1 ? 1.0f : t), a);
      }
    }
  }
  if (round && !full) {
    float rm = (rIn + rOut) * 0.5f, rc = (rOut - rIn) * 0.5f;
    disc(s, cx + rm * u0x, cy + rm * u0y, rc, color(0.0f));
    disc(s, cx + rm * u1x, cy + rm * u1y, rc, color(1.0f));
  }
}

// --- segment épais et polygone convexe -------------------------------------------------------
static void line(const Surf &s, float x0, float y0, float x1, float y1, float w, uint16_t c, bool roundEnds = true) {
  float h = w * 0.5f;
  int ax = (int)floorf((x0 < x1 ? x0 : x1) - h - 1), bx = (int)ceilf((x0 > x1 ? x0 : x1) + h + 1);
  int ay = (int)floorf((y0 < y1 ? y0 : y1) - h - 1), by = (int)ceilf((y0 > y1 ? y0 : y1) + h + 1);
  if (!hit(s, ax, ay, bx, by)) return;
  if (ay < s.oy) ay = s.oy;
  if (by > s.oy + s.h - 1) by = s.oy + s.h - 1;
  if (ax < s.ox) ax = s.ox;
  if (bx > s.ox + s.w - 1) bx = s.ox + s.w - 1;
  float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
  for (int y = ay; y <= by; y++) {
    for (int x = ax; x <= bx; x++) {
      float pxx = x + 0.5f - x0, pyy = y + 0.5f - y0;
      float t = l2 > 0 ? (pxx * dx + pyy * dy) / l2 : 0;
      if (!roundEnds && (t < 0 || t > 1)) continue;
      t = t < 0 ? 0 : (t > 1 ? 1 : t);
      float ex = pxx - t * dx, ey = pyy - t * dy;
      float d2 = ex * ex + ey * ey;
      float lim = (h + 1) * (h + 1);
      if (d2 > lim) continue;
      px(s, x, y, c, covToAlpha(h - sqrtf(d2) + 0.5f));
    }
  }
}

// Polygone convexe (sommets dans un sens quelconque, au plus 8)
static void polygon(const Surf &s, const float *xs, const float *ys, int n, uint16_t c) {
  if (n < 3 || n > 8) return;
  float minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0], area = 0;
  for (int i = 0; i < n; i++) {
    if (xs[i] < minx) minx = xs[i];
    if (xs[i] > maxx) maxx = xs[i];
    if (ys[i] < miny) miny = ys[i];
    if (ys[i] > maxy) maxy = ys[i];
    int j = (i + 1) % n;
    area += xs[i] * ys[j] - xs[j] * ys[i];
  }
  int ax = (int)floorf(minx - 1), bx = (int)ceilf(maxx + 1), ay = (int)floorf(miny - 1), by = (int)ceilf(maxy + 1);
  if (!hit(s, ax, ay, bx, by)) return;
  if (ay < s.oy) ay = s.oy;
  if (by > s.oy + s.h - 1) by = s.oy + s.h - 1;
  if (ax < s.ox) ax = s.ox;
  if (bx > s.ox + s.w - 1) bx = s.ox + s.w - 1;
  float nx[8], ny[8], nc[8];
  float sgn = area > 0 ? 1.0f : -1.0f;
  for (int i = 0; i < n; i++) {  // normales intérieures normalisées
    int j = (i + 1) % n;
    float ex = xs[j] - xs[i], ey = ys[j] - ys[i];
    float l = sqrtf(ex * ex + ey * ey);
    if (l < 1e-6f) l = 1e-6f;
    nx[i] = -ey / l * sgn;
    ny[i] = ex / l * sgn;
    nc[i] = nx[i] * xs[i] + ny[i] * ys[i];
  }
  for (int y = ay; y <= by; y++) {
    for (int x = ax; x <= bx; x++) {
      float fx = x + 0.5f, fy = y + 0.5f, m = 1.0f;
      for (int i = 0; i < n; i++) {
        float d = nx[i] * fx + ny[i] * fy - nc[i];
        if (d < m) m = d;
        if (m <= -0.5f) break;
      }
      if (m > -0.5f) px(s, x, y, c, covToAlpha(m + 0.5f));
    }
  }
}

// --- rectangle arrondi -------------------------------------------------------------------------
static void roundRect(const Surf &s, float x, float y, float w, float h, float r, uint16_t c) {
  int ax = (int)floorf(x), bx = (int)ceilf(x + w), ay = (int)floorf(y), by = (int)ceilf(y + h);
  if (!hit(s, ax, ay, bx, by)) return;
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  int ir = (int)ceilf(r);
  // bandes pleines
  rect(s, ax, ay + ir, bx - ax, by - ay - 2 * ir, c);
  rect(s, ax + ir, ay, bx - ax - 2 * ir, ir, c);
  rect(s, ax + ir, by - ir, bx - ax - 2 * ir, ir, c);
  // coins avec AA
  for (int k = 0; k < 4; k++) {
    int px0 = (k & 1) ? bx - ir : ax;
    int py0 = (k & 2) ? by - ir : ay;
    float ccx = (k & 1) ? x + w - r : x + r;
    float ccy = (k & 2) ? y + h - r : y + r;
    for (int yy = py0; yy < py0 + ir; yy++)
      for (int xx = px0; xx < px0 + ir; xx++) {
        float dx = xx + 0.5f - ccx, dy = yy + 0.5f - ccy;
        bool outerX = (k & 1) ? dx > 0 : dx < 0;
        bool outerY = (k & 2) ? dy > 0 : dy < 0;
        if (!(outerX && outerY)) {
          px(s, xx, yy, c, 255);
          continue;
        }
        float d = sqrtf(dx * dx + dy * dy);
        px(s, xx, yy, c, covToAlpha(r - d + 0.5f));
      }
  }
}

// Carte : fond + bordure fine
static void card(const Surf &s, float x, float y, float w, float h, float r, uint16_t fill, uint16_t border) {
  roundRect(s, x, y, w, h, r, border);
  roundRect(withBg(s, border), x + 1, y + 1, w - 2, h - 2, r - 1, fill);
}

// --- texte -----------------------------------------------------------------------------------
enum Align { AL_L, AL_C, AL_R };

static inline const GlyphDef *findGlyph(const FontDef &f, uint32_t code) {
  int lo = 0, hi = (int)f.n - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    if (f.glyphs[mid].code == code) return &f.glyphs[mid];
    if (f.glyphs[mid].code < code) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

static inline uint32_t nextCode(const char *&p) {
  uint8_t c = (uint8_t)*p++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0) {
    uint32_t r = (c & 0x1F) << 6;
    r |= (uint8_t)*p++ & 0x3F;
    return r;
  }
  if ((c & 0xF0) == 0xE0) {
    uint32_t r = (c & 0x0F) << 12;
    r |= ((uint8_t)*p++ & 0x3F) << 6;
    r |= (uint8_t)*p++ & 0x3F;
    return r;
  }
  return '?';
}

static int textWidth(const FontDef &f, const char *str, int spacing = 0) {
  int w = 0, n = 0;
  while (*str) {
    const GlyphDef *g = findGlyph(f, nextCode(str));
    if (g) {
      w += g->adv + spacing;
      n++;
    }
  }
  return n ? w - spacing : 0;
}

// Écrit `str` avec sa ligne de base à y. Retourne la largeur dessinée.
static int text(const Surf &s, const FontDef &f, int x, int baseline, const char *str, uint16_t color,
                Align al = AL_L, int spacing = 0) {
  int w = textWidth(f, str, spacing);
  int pen = al == AL_L ? x : (al == AL_C ? x - w / 2 : x - w);
  if (!hit(s, pen, baseline - f.ascent, pen + w, baseline + f.lineH - f.ascent)) return w;
  while (*str) {
    const GlyphDef *g = findGlyph(f, nextCode(str));
    if (!g) continue;
    if (g->w) {
      const uint8_t *b = f.bmp + g->off;
      int gx = pen + g->dx, gy = baseline - g->dy;
      for (int j = 0; j < g->h; j++) {
        int yy = gy + j;
        if (yy < s.oy || yy >= s.oy + s.h) {
          b += g->w;
          continue;
        }
        for (int i = 0; i < g->w; i++) {
          uint8_t a = *b++;
          if (a > 3) px(s, gx + i, yy, color, a);
        }
      }
    }
    pen += g->adv + spacing;
  }
  return w;
}

}  // namespace gfx
