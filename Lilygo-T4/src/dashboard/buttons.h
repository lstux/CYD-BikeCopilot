// Boutons : antirebond, appui court / long / très long. Logique pure (testable sur PC) :
// l'état physique est passé à update(), la lecture des broches se fait dans main.cpp.
#pragma once
#include <stdint.h>

enum BtnEvent : uint8_t {
  BE_NONE,
  BE_SHORT,  // relâché avant BTN_LONG_MS
  BE_LONG,   // maintenu BTN_LONG_MS (déclenché pendant l'appui)
  BE_VLONG   // maintenu BTN_VLONG_MS (déclenché pendant l'appui)
};

#ifndef BTN_DEBOUNCE_MS
#define BTN_DEBOUNCE_MS 30
#endif
#ifndef BTN_LONG_MS
#define BTN_LONG_MS 1000
#endif
#ifndef BTN_VLONG_MS
#define BTN_VLONG_MS 5000
#endif

struct Button {
  bool down = false;
  bool firedLong = false, firedVLong = false;
  uint32_t downMs = 0;

  // pressed : le bouton est enfoncé à cet instant. Retourne l'événement éventuel.
  BtnEvent update(bool pressed, uint32_t now) {
    if (pressed && !down) {
      down = true;
      downMs = now;
      firedLong = firedVLong = false;
      return BE_NONE;
    }
    if (pressed && down) {
      uint32_t held = now - downMs;
      if (!firedVLong && held >= BTN_VLONG_MS) {
        firedVLong = true;
        return BE_VLONG;
      }
      if (!firedLong && held >= BTN_LONG_MS) {
        firedLong = true;
        return BE_LONG;
      }
      return BE_NONE;
    }
    if (!pressed && down) {
      down = false;
      uint32_t held = now - downMs;
      if (!firedLong && !firedVLong && held >= BTN_DEBOUNCE_MS) return BE_SHORT;
    }
    return BE_NONE;
  }
};
