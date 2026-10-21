// Barre d'état de la T4 : comme hud::statusBar, mais le téléphone ne donne ni satellites ni HDOP.
// Les barres de signal reflètent la précision du point (en mètres), et le texte indique l'état
// de la liaison tant qu'il n'y a pas de position.
#pragma once
#include "gfx.h"
#include "theme.h"

namespace t4 {
using namespace gfx;

struct Status {
  bool fix = false;      // position valide
  bool accValid = false;
  float accM = 0;        // précision en mètres
  const char *link = "";  // texte affiché sans position (état de la liaison)
  char clock[8] = "--:--";
};

// 0 à 4 barres : meilleure précision = plus de barres
static inline int accBars(const Status &st) {
  if (!st.fix) return 0;
  if (!st.accValid) return 2;
  if (st.accM <= 8) return 4;
  if (st.accM <= 15) return 3;
  if (st.accM <= 30) return 2;
  return 1;
}

static void statusBar(const Surf &s, const Status &st) {
  rect(s, 0, 0, 320, 20, col::BG);
  int lit = accBars(st);
  for (int i = 0; i < 4; i++) {
    int h = 3 + 2 * i;
    rect(s, 5 + i * 5, 16 - h, 3, h, i < lit ? col::OK : col::DOT);
  }
  char b[24];
  if (st.fix && st.accValid) snprintf(b, sizeof(b), "préc. %d m", (int)(st.accM + 0.5f));
  else if (st.fix) snprintf(b, sizeof(b), "GPS");
  else snprintf(b, sizeof(b), "%s", st.link);
  text(s, FONT_S11, 28, 15, b, st.fix ? col::SEC : col::AMBER);
  text(s, FONT_S13, 316, 16, st.clock, col::TXT, AL_R);
}

}  // namespace t4
