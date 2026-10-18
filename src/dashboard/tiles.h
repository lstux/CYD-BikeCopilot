#pragma once
// Lecture des tuiles de carte sur la SD.
// Format : /tiles/<z>/<x>/<y>.raw, 256x256 pixels en RGB565 "big-endian" (131072 octets),
// c'est-à-dire exactement l'ordre des octets d'un sprite TFT_eSPI en 16 bits : on peut donc
// lire une ligne de la tuile directement dans la mémoire du sprite, sans conversion.
#include <Arduino.h>
#include <SD.h>

class TileCache {
 public:
  // Lit len pixels de la ligne ry de la tuile (z,tx,ty), à partir de la colonne cx.
  // Retourne false si la tuile est absente.
  bool readRow(int z, int tx, int ty, int ry, int cx, int len, uint8_t *dst) {
    Slot *s = get(z, tx, ty);
    if (!s || !s->ok) return false;
    if (!s->f.seek((uint32_t)(ry * 256 + cx) * 2)) return false;
    return s->f.read(dst, len * 2) == len * 2;
  }

  void closeAll() {
    for (auto &s : slots_) {
      if (s.f) s.f.close();
      s.z = -1;
      s.ok = false;
    }
  }

 private:
  struct Slot {
    int z = -1, x = 0, y = 0;
    File f;
    bool ok = false;
    uint32_t used = 0;
  };
  Slot slots_[8];
  uint32_t tick_ = 0;

  Slot *get(int z, int x, int y) {
    tick_++;
    Slot *victim = &slots_[0];
    for (auto &s : slots_) {
      if (s.z == z && s.x == x && s.y == y) {
        s.used = tick_;
        return &s;
      }
      if (s.used < victim->used) victim = &s;
    }
    if (victim->f) victim->f.close();
    char path[40];
    snprintf(path, sizeof(path), "/tiles/%d/%d/%d.raw", z, x, y);
    victim->f = SD.open(path, FILE_READ);
    victim->ok = (bool)victim->f;
    victim->z = z;
    victim->x = x;
    victim->y = y;
    victim->used = tick_;
    return victim;
  }
};
