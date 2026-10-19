#pragma once
// Page « Réglages » : trois groupes de lignes (carte arrondie, une ligne = libellé + contrôle).
// Inclus par main.cpp (utilise tft, col::, cfg, logger, trip, gps, updateDestination()).
#include "hud.h"

enum SettingsAction { SA_NONE, SA_CALIB_TOUCH, SA_RESET_COMPASS };

#define SET_ROWS   8
#define SET_ROW_H  20

// Groupes : [première ligne, nombre de lignes], position du titre
static const char *SET_TITLES[3] = {"ENREGISTREMENT", "AFFICHAGE", "CAPTEURS ET POSITION"};
static const int SET_G_FIRST[3] = {0, 2, 5};
static const int SET_G_COUNT[3] = {2, 3, 3};
static const int SET_G_Y[3] = {21, 77, 153};  // haut du bloc titre ; la carte commence 11 px plus bas

static bool setFirstRow(int i) { return i == 0 || i == 2 || i == 5; }
static bool setLastRow(int i) { return i == 1 || i == 4 || i == 7; }

static int setRowY(int i) {
  for (int g = 0; g < 3; g++)
    if (i >= SET_G_FIRST[g] && i < SET_G_FIRST[g] + SET_G_COUNT[g])
      return SET_G_Y[g] + 11 + (i - SET_G_FIRST[g]) * SET_ROW_H;
  return 0;
}

static int settingsNoteRow = -1;
static char settingsNote[40];
static uint8_t homeMsg = 0;  // 0 : rien, 1 : enregistré, 2 : pas de position GPS

static void settingsSetNote(int row, const char *txt) {
  settingsNoteRow = row;
  snprintf(settingsNote, sizeof(settingsNote), "%s", txt);
}

static void setToggle(const gfx::Surf &s, int xr, int cy, bool on) {
  using namespace gfx;
  const float w = 28, h = 16, x = xr - w, y = cy - h / 2;
  roundRect(s, x, y, w, h, 8, on ? col::OK : col::DOT);
  disc(s, on ? x + w - 8 : x + 8, cy, 6.0f, on ? col::BG : col::SEC);
}

static void setStepBtn(const gfx::Surf &s, int x, int cy, bool plus) {
  using namespace gfx;
  roundRect(s, x, cy - 8, 22, 16, 6, col::SURF2);
  line(s, x + 6.5f, cy, x + 15.5f, cy, 1.6f, col::TXT);
  if (plus) line(s, x + 11, cy - 4.5f, x + 11, cy + 4.5f, 1.6f, col::TXT);
}

static void setStepper(const gfx::Surf &s, int xr, int cy, const char *val) {
  using namespace gfx;
  setStepBtn(s, xr - 22, cy, true);
  setStepBtn(s, xr - 22 - 4 - 52 - 4 - 22, cy, false);
  Surf c = withBg(s, col::CARD);
  text(c, FONT_B13, xr - 22 - 4 - 26, cy + 5, val, col::TXT, AL_C);
}

// Choix à trois états (Auto / Sombre / Clair), aligné à droite
static const int SEG_W[3] = {34, 46, 38}, SEG_GAP = 3;
static int segX0(int xr) { return xr - (SEG_W[0] + SEG_W[1] + SEG_W[2] + 2 * SEG_GAP); }
static void setSegments(const gfx::Surf &s, int xr, int cy, const char *const labels[3], int sel) {
  using namespace gfx;
  int x = segX0(xr);
  for (int k = 0; k < 3; k++) {
    uint16_t bg = k == sel ? col::ACCENT : col::SURF2;
    roundRect(s, x, cy - 8, SEG_W[k], 16, 6, bg);
    text(withBg(s, bg), FONT_S11, x + SEG_W[k] / 2, cy + 4, labels[k], k == sel ? col::BG : col::SEC, AL_C);
    x += SEG_W[k] + SEG_GAP;
  }
}
static int segHit(int xr, int x) {
  int x0 = segX0(xr);
  for (int k = 0; k < 3; k++) {
    if (x >= x0 - 1 && x < x0 + SEG_W[k] + SEG_GAP) return k;
    x0 += SEG_W[k] + SEG_GAP;
  }
  return -1;
}

static void setLink(const gfx::Surf &s, int xr, int cy, const char *label, uint16_t color) {
  using namespace gfx;
  line(s, xr - 6, cy - 4.5f, xr - 1.5f, cy, 1.5f, color);
  line(s, xr - 1.5f, cy, xr - 6, cy + 4.5f, 1.5f, color);
  Surf c = withBg(s, col::CARD);
  text(c, FONT_S11, xr - 11, cy + 4, label, color, AL_R);
}

static void drawSettingsRow(int i) {
  using namespace gfx;
  Surf s = onScreen(tft, col::BG);
  const int y = setRowY(i), cy = y + SET_ROW_H / 2, xr = 307;
  // fond de la ligne (à l'intérieur de la bordure de la carte) et filet de séparation
  rect(s, 7, y + (setFirstRow(i) ? 1 : 0), 306, SET_ROW_H - (setFirstRow(i) ? 1 : 0), col::CARD);
  if (!setLastRow(i)) hline(s, 6, y + SET_ROW_H - 1, 308, col::BORDER);
  Surf c = withBg(s, col::CARD);
  char v[24];
  const char *label = "";
  switch (i) {
    case 0: label = "Journal des trajets"; break;
    case 1: label = logger.state() == TripLogger::ACTIVE ? "Trajet en cours" : (logger.state() == TripLogger::PENDING ? "Départ détecté…" : "Aucun trajet en cours"); break;
    case 2: label = "Fuseau horaire"; break;
    case 3: label = "Échelle mini de la jauge"; break;
    case 4: label = "Thème de la carte"; break;
    case 5: label = "Écran tactile"; break;
    case 6: label = "Boussole"; break;
    case 7: label = "Domicile"; break;
  }
  int lw = text(c, FONT_S13, 14, cy + 5, label, col::TXT);
  switch (i) {
    case 0: setToggle(s, xr, cy, cfg.logOn); break;
    case 1: {
      if (logger.state() == TripLogger::ACTIVE) {
        snprintf(v, sizeof(v), "%.1f km", trip.distM / 1000.0);
        text(c, FONT_M9, 14 + lw + 8, cy + 4, v, col::SEC);
        roundRect(s, xr - 58, cy - 8, 58, 16, 6, col::RED);
        text(withBg(s, col::RED), FONT_S11, xr - 29, cy + 4, "Terminer", col::BG, AL_C);
      } else {
        roundRect(s, xr - 58, cy - 8, 58, 16, 6, col::SURF2);
        text(withBg(s, col::SURF2), FONT_S11, xr - 29, cy + 4, "Terminer", col::TER, AL_C);
      }
      break;
    }
    case 2:
      snprintf(v, sizeof(v), "UTC%+d", cfg.tz);
      setStepper(s, xr, cy, v);
      break;
    case 3:
      snprintf(v, sizeof(v), "%d km/h", cfg.gaugeMin);
      setStepper(s, xr, cy, v);
      break;
    case 4: {
      static const char *const T[3] = {"Auto", "Sombre", "Clair"};
      setSegments(s, xr, cy, T, cfg.mapTheme);
      if (cfg.mapTheme == MT_AUTO && ldrRaw >= 0) {  // lecture de la photorésistance, pour régler les seuils
        snprintf(v, sizeof(v), "lum. %d", ldrRaw);
        text(c, FONT_M9, 14 + lw + 8, cy + 4, v, col::SEC);
      }
      break;
    }
    case 5: setLink(s, xr, cy, "Calibrer", col::ACCENT); break;
    case 6:
      setLink(s, xr, cy, "Recalibrer", col::ACCENT);
      if (settingsNoteRow == 6) text(c, FONT_M9, 14 + lw + 8, cy + 4, settingsNote, col::AMBER);
      break;
    case 7:
      if (homeMsg == 2) text(c, FONT_M9, 14 + lw + 8, cy + 4, "pas de position GPS", col::AMBER);
      else if (homeMsg == 1) text(c, FONT_M9, 14 + lw + 8, cy + 4, "enregistré", col::OK);
      else if (cfg.homeOk) {
        snprintf(v, sizeof(v), "%.4f, %.4f", cfg.homeLat, cfg.homeLon);
        text(c, FONT_M9, 14 + lw + 8, cy + 4, v, col::SEC);
      } else {
        text(c, FONT_M9, 14 + lw + 8, cy + 4, "non défini", col::TER);
      }
      setLink(s, xr, cy, "Définir ici", col::ACCENT);
      break;
  }
}

static void settingsEnter() {
  using namespace gfx;
  Surf s = onScreen(tft, col::BG);
  homeMsg = 0;
  settingsNoteRow = -1;
  for (int g = 0; g < 3; g++) {
    text(s, FONT_S9, 12, SET_G_Y[g] + 8, SET_TITLES[g], col::SEC, AL_L, 1);
    card(s, 5, SET_G_Y[g] + 11, 310, SET_G_COUNT[g] * SET_ROW_H, 11, col::CARD, col::BORDER);
  }
  for (int i = 0; i < SET_ROWS; i++) drawSettingsRow(i);
  hud::pageDots(s, 4, NB_PAGES);
}

// Rafraîchit la ligne « trajet en cours » si l'état a changé
static void settingsRefresh() {
  static int lastKey = -1, lastLdr = -1000;
  int key = (int)logger.state() * 100000 + (int)(trip.distM / 100);
  if (key != lastKey) {
    lastKey = key;
    drawSettingsRow(1);
  }
  if (cfg.mapTheme == MT_AUTO && abs(ldrRaw - lastLdr) >= 8) {  // lecture de la photorésistance
    lastLdr = ldrRaw;
    drawSettingsRow(4);
  }
}

static SettingsAction settingsTap(int x, int y) {
  int i = -1;
  for (int k = 0; k < SET_ROWS; k++)
    if (y >= setRowY(k) && y < setRowY(k) + SET_ROW_H) i = k;
  if (i < 0) return SA_NONE;
  SettingsAction act = SA_NONE;
  switch (i) {
    case 0:
      if (x >= 200) { cfg.logOn = !cfg.logOn; cfg.save(); }
      break;
    case 1:
      if (x >= 220 && logger.state() == TripLogger::ACTIVE) logger.finish();
      break;
    case 2:
      if (x >= 185 && x < 235 && cfg.tz > -12) { cfg.tz--; cfg.save(); }
      else if (x >= 262 && cfg.tz < 14) { cfg.tz++; cfg.save(); }
      break;
    case 3:
      if (x >= 185 && x < 235 && cfg.gaugeMin > 5) { cfg.gaugeMin -= 5; cfg.save(); }
      else if (x >= 262 && cfg.gaugeMin < 30) { cfg.gaugeMin += 5; cfg.save(); }
      break;
    case 4: {
      int k = segHit(307, x);
      if (k >= 0 && k != cfg.mapTheme) { cfg.mapTheme = k; cfg.save(); }
      break;
    }
    case 5:
      if (x >= 200) act = SA_CALIB_TOUCH;
      break;
    case 6:
      if (x >= 200) act = SA_RESET_COMPASS;
      break;
    case 7:
      if (x >= 200) {
        if (gps.location.isValid() && gps.location.age() < 3000) {
          cfg.homeOk = true;
          cfg.homeLat = gps.location.lat();
          cfg.homeLon = gps.location.lng();
          cfg.save();
          updateDestination();
          homeMsg = 1;
        } else {
          homeMsg = 2;
        }
      }
      break;
  }
  if (act == SA_NONE) drawSettingsRow(i);
  return act;
}
