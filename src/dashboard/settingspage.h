#pragma once
// Page « Réglages ». Chaque ligne : libellé à gauche, contrôle à droite.
// Inclus par main.cpp (utilise tft, col::, cfg, logger).

enum SettingsAction { SA_NONE, SA_CALIB_TOUCH, SA_RESET_COMPASS };

#define SET_ROWS   6
#define SET_Y0     28
#define SET_ROW_H  34

static void drawSetBtn(int x, int y, int w, const char *txt, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(x, y, w, 26, 5, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(txt, x + w / 2, y + 13, 2);
}

static void drawSettingsRow(int i) {
  int y = SET_Y0 + i * SET_ROW_H;
  tft.fillRect(0, y, 320, SET_ROW_H, col::BG);
  tft.setTextPadding(0);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(col::WHITE, col::BG);
  char v[24];
  int by = y + 4;
  switch (i) {
    case 0:
      tft.drawString("Journal des trajets", 8, y + 17, 2);
      drawSetBtn(236, by, 76, cfg.logOn ? "OUI" : "NON", cfg.logOn ? col::GREEN : col::TRACK,
                 cfg.logOn ? col::BG : col::LABEL);
      break;
    case 1:
      tft.drawString("Fuseau horaire", 8, y + 17, 2);
      drawSetBtn(180, by, 40, "-", col::TRACK, col::WHITE);
      snprintf(v, sizeof(v), "UTC%+d", cfg.tz);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(col::WHITE, col::BG);
      tft.drawString(v, 248, y + 17, 2);
      drawSetBtn(272, by, 40, "+", col::TRACK, col::WHITE);
      break;
    case 2:
      tft.drawString("Jauge : echelle mini", 8, y + 17, 2);
      drawSetBtn(180, by, 40, "-", col::TRACK, col::WHITE);
      snprintf(v, sizeof(v), "%d km/h", cfg.gaugeMin);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(col::WHITE, col::BG);
      tft.drawString(v, 246, y + 17, 2);
      drawSetBtn(272, by, 40, "+", col::TRACK, col::WHITE);
      break;
    case 3: {
      bool rec = logger.state() == TripLogger::ACTIVE;
      tft.drawString(rec ? "Trajet en cours" : (logger.state() == TripLogger::PENDING ? "Depart detecte..." : "Aucun trajet en cours"), 8, y + 17, 2);
      drawSetBtn(212, by, 100, "Terminer", rec ? col::ORANGE : col::TRACK, rec ? col::BG : col::DIM);
      break;
    }
    case 4:
      tft.drawString("Tactile", 8, y + 17, 2);
      drawSetBtn(212, by, 100, "Calibrer", col::TRACK, col::WHITE);
      break;
    case 5:
      tft.drawString("Boussole", 8, y + 17, 2);
      drawSetBtn(212, by, 100, "Recalibrer", col::TRACK, col::WHITE);
      break;
  }
  tft.drawFastHLine(8, y + SET_ROW_H - 1, 304, col::RULE);
}

static void settingsEnter() {
  for (int i = 0; i < SET_ROWS; i++) drawSettingsRow(i);
}

static SettingsAction settingsTap(int x, int y) {
  if (y < SET_Y0) return SA_NONE;
  int i = (y - SET_Y0) / SET_ROW_H;
  if (i >= SET_ROWS) return SA_NONE;
  SettingsAction act = SA_NONE;
  switch (i) {
    case 0:
      if (x >= 200) {
        cfg.logOn = !cfg.logOn;
        cfg.save();
      }
      break;
    case 1:
      if (x >= 170 && x < 230 && cfg.tz > -12) { cfg.tz--; cfg.save(); }
      else if (x >= 262 && cfg.tz < 14) { cfg.tz++; cfg.save(); }
      break;
    case 2:
      if (x >= 170 && x < 230 && cfg.gaugeMin > 5) { cfg.gaugeMin -= 5; cfg.save(); }
      else if (x >= 262 && cfg.gaugeMin < 30) { cfg.gaugeMin += 5; cfg.save(); }
      break;
    case 3:
      if (x >= 200 && logger.state() == TripLogger::ACTIVE) logger.finish();
      break;
    case 4:
      if (x >= 200) act = SA_CALIB_TOUCH;
      break;
    case 5:
      if (x >= 200) act = SA_RESET_COMPASS;
      break;
  }
  if (act == SA_NONE) drawSettingsRow(i);
  return act;
}
