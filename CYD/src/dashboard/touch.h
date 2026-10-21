#pragma once
// Écran tactile XPT2046 de la CYD, lu en SPI logiciel (les deux bus matériels sont pris :
// HSPI par l'écran, VSPI par la carte SD). Contient aussi la calibration (sauvée en mémoire
// interne) et la détection des gestes : appui, appui long, glissé gauche / droite.
#include <Arduino.h>
#include <Preferences.h>

enum TouchEvent { TE_NONE, TE_TAP, TE_LONG, TE_SWIPE_L, TE_SWIPE_R, TE_DRAG };
// Mode de glissé : DRAG_XY (carte : glissé libre, sauf depuis les bords gauche/droit qui gardent le
// changement de page), DRAG_Y (listes : glissé vertical).
enum DragMode { DRAG_NONE, DRAG_XY, DRAG_Y };

class Touch {
 public:
  int x = 0, y = 0;  // position de l'évènement (pixels écran)
  int dx = 0, dy = 0;  // TE_DRAG : déplacement depuis l'évènement précédent
  DragMode dragMode = DRAG_NONE;

  void begin(int clk, int mosi, int miso, int cs, int irq, int w, int h) {
    clk_ = clk; mosi_ = mosi; miso_ = miso; cs_ = cs; irq_ = irq; w_ = w; h_ = h;
    pinMode(cs_, OUTPUT);
    digitalWrite(cs_, HIGH);
    pinMode(clk_, OUTPUT);
    digitalWrite(clk_, LOW);
    pinMode(mosi_, OUTPUT);
    pinMode(miso_, INPUT);
    pinMode(irq_, INPUT);
    Preferences p;
    p.begin("touch", true);
    if (p.getBool("ok", false)) {
      swap_ = p.getBool("sw", false);
      x0_ = p.getInt("x0", 0);
      x1_ = p.getInt("x1", 4095);
      y0_ = p.getInt("y0", 0);
      y1_ = p.getInt("y1", 4095);
      calibrated_ = true;
    }
    p.end();
  }

  bool calibrated() const { return calibrated_; }
  int curX() const { return cx_; }  // position actuelle du doigt (pendant un glissé)
  int curY() const { return cy_; }

  // Valeurs brutes (médiane de 5 mesures). false si pas d'appui.
  bool readRaw(int &rx, int &ry) {
    if (digitalRead(irq_) != LOW) return false;
    int xs[5], ys[5];
    xfer(0xD0);  // mesure à blanc
    for (int i = 0; i < 5; i++) {
      xs[i] = xfer(0xD0);
      ys[i] = xfer(0x90);
    }
    xfer(0x80);  // remet le contrôleur en veille, IRQ réactivée
    if (digitalRead(irq_) != LOW) return false;
    rx = median5(xs);
    ry = median5(ys);
    return rx > 50 && rx < 4050 && ry > 50 && ry < 4050;
  }

  // Calibration à partir de trois appuis : coin haut-gauche, haut-droite, bas-droite
  // (centres des croix à `margin` pixels des bords).
  void calibrate(int tlx, int tly, int trx, int try_, int brx, int bry, int margin) {
    (void)tly;
    (void)bry;
    // L'axe brut qui varie le plus entre haut-gauche et haut-droite est celui de l'axe x de l'écran
    swap_ = abs(try_ - tly) > abs(trx - tlx);
    if (!swap_) {
      x0_ = tlx; x1_ = trx;
      y0_ = tly; y1_ = bry;
    } else {
      x0_ = tly; x1_ = try_;
      y0_ = tlx; y1_ = brx;
    }
    margin_ = margin;
    calibrated_ = true;
    Preferences p;
    p.begin("touch", false);
    p.putBool("ok", true);
    p.putBool("sw", swap_);
    p.putInt("x0", x0_);
    p.putInt("x1", x1_);
    p.putInt("y0", y0_);
    p.putInt("y1", y1_);
    p.putInt("m", margin_);
    p.end();
  }

  void toScreen(int rx, int ry, int &sx, int &sy) const {
    int ax = swap_ ? ry : rx, ay = swap_ ? rx : ry;
    int dx = x1_ - x0_, dy = y1_ - y0_;
    if (dx == 0) dx = 1;
    if (dy == 0) dy = 1;
    sx = margin_ + (int)((long)(ax - x0_) * (w_ - 2 * margin_) / dx);
    sy = margin_ + (int)((long)(ay - y0_) * (h_ - 2 * margin_) / dy);
    sx = constrain(sx, 0, w_ - 1);
    sy = constrain(sy, 0, h_ - 1);
  }

  // À appeler souvent ; renvoie un évènement quand un geste est terminé (ou un appui long atteint)
  TouchEvent poll(uint32_t now) {
    if (now - lastPoll_ < 10) return TE_NONE;
    lastPoll_ = now;
    int rx, ry;
    bool down = calibrated_ && readRaw(rx, ry);
    if (down) {
      int sx, sy;
      toScreen(rx, ry, sx, sy);
      lastSeen_ = now;
      if (!pressed_) {
        pressed_ = true;
        longSent_ = false;
        dragging_ = false;
        sx0_ = sx; sy0_ = sy; t0_ = now;
      }
      cx_ = sx; cy_ = sy;
      if (!dragging_ && !longSent_ && dragMode != DRAG_NONE) {
        int adx = abs(cx_ - sx0_), ady = abs(cy_ - sy0_);
        bool xy = dragMode == DRAG_XY && sx0_ >= 30 && sx0_ <= 290 && (adx > 10 || ady > 10);
        bool yy = dragMode == DRAG_Y && ady > 10 && ady > adx;
        if (xy || yy) {
          dragging_ = true;
          gx_ = sx0_; gy_ = sy0_;
        }
      }
      if (dragging_) {
        dx = dragMode == DRAG_Y ? 0 : cx_ - gx_;
        dy = cy_ - gy_;
        if (dx || dy) {
          gx_ = cx_; gy_ = cy_;
          return TE_DRAG;
        }
        return TE_NONE;
      }
      if (!longSent_ && now - t0_ > 900 && abs(cx_ - sx0_) < 20 && abs(cy_ - sy0_) < 20) {
        longSent_ = true;
        x = sx0_; y = sy0_;
        return TE_LONG;
      }
      return TE_NONE;
    }
    // relâché : on attend 40 ms sans contact pour éviter les rebonds
    if (pressed_ && now - lastSeen_ > 40) {
      pressed_ = false;
      if (dragging_) {
        dragging_ = false;
        return TE_NONE;
      }
      if (longSent_) return TE_NONE;
      int dx = cx_ - sx0_, dy = cy_ - sy0_;
      uint32_t d = lastSeen_ - t0_;
      x = sx0_; y = sy0_;
      if (abs(dx) > 50 && abs(dx) > 2 * abs(dy) && d < 900) return dx < 0 ? TE_SWIPE_L : TE_SWIPE_R;
      if (abs(dx) < 20 && abs(dy) < 20 && d < 900) return TE_TAP;
    }
    return TE_NONE;
  }

 private:
  int clk_ = 25, mosi_ = 32, miso_ = 39, cs_ = 33, irq_ = 36, w_ = 320, h_ = 240;
  bool calibrated_ = false, swap_ = false;
  int x0_ = 0, x1_ = 4095, y0_ = 0, y1_ = 4095, margin_ = 20;
  bool pressed_ = false, longSent_ = false, dragging_ = false;
  int sx0_ = 0, sy0_ = 0, cx_ = 0, cy_ = 0, gx_ = 0, gy_ = 0;
  uint32_t t0_ = 0, lastSeen_ = 0, lastPoll_ = 0;

  static int median5(int *v) {
    for (int i = 1; i < 5; i++)
      for (int j = i; j > 0 && v[j] < v[j - 1]; j--) {
        int t = v[j]; v[j] = v[j - 1]; v[j - 1] = t;
      }
    return v[2];
  }

  // Une conversion : commande 8 bits puis lecture de 16 bits (bit nul + 12 bits + 3 bits)
  int xfer(uint8_t cmd) {
    digitalWrite(cs_, LOW);
    for (int i = 7; i >= 0; i--) {
      digitalWrite(mosi_, (cmd >> i) & 1);
      digitalWrite(clk_, HIGH);
      delayMicroseconds(1);
      digitalWrite(clk_, LOW);
      delayMicroseconds(1);
    }
    uint16_t v = 0;
    for (int i = 0; i < 16; i++) {
      digitalWrite(clk_, HIGH);
      delayMicroseconds(1);
      v = (v << 1) | (digitalRead(miso_) ? 1 : 0);
      digitalWrite(clk_, LOW);
      delayMicroseconds(1);
    }
    digitalWrite(cs_, HIGH);
    return (v >> 3) & 0xFFF;
  }
};
