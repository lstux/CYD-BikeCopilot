// Tableau de bord vélo - CYD (ESP32-2432S028) + GPS + QMC5883
// Page 0 : jauge de vitesse à échelle adaptative + statistiques du trajet
// Page 1 : boussole tournante + flèche vers la destination
// Bouton BOOT : appui court = page suivante, appui long (> 1 s) = reset du trajet
//
// Pour éviter le scintillement, la jauge et la boussole sont dessinées dans un sprite
// (mémoire tampon) puis envoyées d'un bloc à l'écran. Les textes autour ne sont
// redessinés que quand leur valeur change.
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "gpsfix.h"
#include "geo.h"
#include "compass.h"
#include "ui.h"
#include "page_gauge.h"
#include "page_compass.h"
#include "touch.h"
#include "settings.h"
#include "trips.h"

TFT_eSPI tft;
TFT_eSprite spr(&tft);
TinyGPSPlus gps;  // GPS série (propre à la CYD)
Fix fix;          // position courante lue par tout le reste du code
Touch touch;
Settings cfg;
TripLogger logger;
static bool modalReset = false;  // fenêtre « Remettre à zéro ? » affichée
Compass compass;

// --- État du trajet ---
struct Trip {
  double distM = 0;
  uint32_t movingMs = 0;
  float maxKmh = 0;
  bool hasLast = false;
  double lastLat = 0, lastLon = 0;
  uint32_t lastTickMs = 0;
  float climbM = 0, altRef = 0, altFilt = 0;
  bool hasAlt = false;
  void reset() { *this = Trip(); }
} trip;

int page = 0;
bool pageDirty = true;
bool sprOk = false;
static const int NB_PAGES = 5;  // 0 compteur, 1 direction, 2 carte, 3 trajets, 4 réglages

// Destination : dernier point de la trace GPX si elle existe, sinon celle de config.h
double destLat = 0, destLon = 0;
bool destValid = false;  // destination connue (voir updateDestination)
// Dernière position avec fix (RAM seulement) : centre de la carte quand le GPS ne répond plus
bool lastFixOk = false;
double lastFixLat = 0, lastFixLon = 0;
// Menu « Destination » (page Direction) et fenêtre d'appui long (page Carte)
bool destMenuOpen = false;
bool mapPopOpen = false;
int mapPopX = 0, mapPopY = 0;
double mapPopLat = 0, mapPopLon = 0;

static float currentKmh();
static void pollGps() {
  while (Serial2.available()) gps.encode(Serial2.read());
  fixFromGps(gps, fix);
}

static void localTime(char *out, size_t n) {
  if (!fix.timeValid) {
    snprintf(out, n, "--:--");
    return;
  }
  int h = (fix.hour + cfg.tz + 24) % 24;
  snprintf(out, n, "%02d:%02d", h, fix.minute);
}

static gfx::Surf scr() { return gfx::onScreen(tft, col::BG); }

static hud::Status currentStatus() {
  hud::Status st;
  st.sats = fix.sats;
  st.fix = fix.valid;
  TripLogger::State ls = logger.state();
  st.rec = ls == TripLogger::ACTIVE ? 2 : (ls == TripLogger::PENDING ? 1 : 0);
  localTime(st.clock, sizeof(st.clock));
  return st;
}

// Vitesse retenue pour l'heure d'arrivée estimée : moyenne du trajet, sinon vitesse actuelle, sinon 15 km/h
static float etaSpeedKmh() {
  float v = 15.0f;
  if (trip.movingMs > 60000) v = (float)(trip.distM / 1000.0) / (trip.movingMs / 3600000.0f);
  else if (currentKmh() > 8) v = currentKmh();
  return v < 5.0f ? 5.0f : v;
}

#include <SD.h>
#include <SPI.h>
SPIClass sdSPI(VSPI);  // broches de la SD propres à la carte (voir config.h)
bool sdOk = false;
#include "maptheme.h"
#include "mappage.h"

static void setDestPoint(double la, double lo) {
  destLat = la;
  destLon = lo;
  destValid = true;
}

// Destination : choix explicite (cfg.destMode) si disponible, sinon automatique (trace, puis domicile)
static void updateDestination() {
  bool home = cfg.homeOk, trk = track.valid(), pt = cfg.pointOk;
  int m = cfg.destMode;
  if (m == DM_POINT && pt) setDestPoint(cfg.pointLat, cfg.pointLon);
  else if (m == DM_HOME && home) setDestPoint(cfg.homeLat, cfg.homeLon);
  else if (m == DM_TRACK && trk) setDestPoint(track.destLat(), track.destLon());
  else if (trk) setDestPoint(track.destLat(), track.destLon());
  else if (home) setDestPoint(cfg.homeLat, cfg.homeLon);
  else destValid = false;
}

static void chooseDest(int mode, double la = 0, double lo = 0) {
  cfg.destMode = mode;
  if (mode == DM_POINT) {
    cfg.pointOk = true;
    cfg.pointLat = la;
    cfg.pointLon = lo;
  }
  cfg.save();
  updateDestination();
}

static void setHome(double la, double lo) {
  cfg.homeOk = true;
  cfg.homeLat = la;
  cfg.homeLon = lo;
  cfg.save();
  updateDestination();
}
#include "histpage.h"
#include "histtouch.h"
#include "settingspage.h"

// --- Menu « Destination » (page Direction) ----------------------------------------------------
static void openDestMenu() {
  if (sdOk) histN = indexLoad(hist, HIST_MAX);
  pg_compass::DestMenuState st;
  st.active = cfg.destMode == DM_POINT ? -1 : cfg.destMode;
  st.en[0] = true;
  st.en[1] = cfg.homeOk;
  st.en[2] = track.valid();
  st.en[3] = histN > 0;
  static const char *names[4] = {"auto", "domicile", "trace GPX", "point choisi"};
  st.cur = names[cfg.destMode];
  pg_compass::drawDestMenu(scr(), st);
  destMenuOpen = true;
}

static void applyDestMenu(int r) {
  switch (r) {
    case 0: chooseDest(DM_AUTO); break;
    case 1: if (cfg.homeOk) chooseDest(DM_HOME); break;
    case 2: if (track.valid()) chooseDest(DM_TRACK); break;
    case 3:
      if (histN > 0) {  // fin du dernier trajet enregistré
        char p[64];
        double la, lo;
        tripPath(hist[histN - 1].name, p, sizeof(p));
        if (tripLastPoint(p, la, lo)) chooseDest(DM_POINT, la, lo);
      }
      break;
  }
}

// --- Vitesse filtrée (anti-dérive GPS à l'arrêt) ---
static float speedKmh = 0;
static bool speedMoving = false;
static uint32_t speedAboveSince = 0, speedLastMs = 0;

static void updateSpeed(uint32_t now) {
  float raw = fix.speedValid ? fix.kmh : 0;
  if (!speedMoving) {
    bool good = fix.valid && fix.sats >= SPEED_MIN_SATS && fix.hdopValid && fix.hdop <= SPEED_HDOP_MAX;
    if (good && raw >= SPEED_START_KMH) {
      if (speedAboveSince == 0) speedAboveSince = now;
      if (now - speedAboveSince >= SPEED_START_MS) {
        speedMoving = true;
        speedKmh = raw;
      }
    } else {
      speedAboveSince = 0;
    }
    if (!speedMoving) speedKmh = 0;
  } else if (raw < SPEED_STOP_KMH) {
    speedMoving = false;
    speedAboveSince = 0;
    speedKmh = 0;
  } else {
    float alpha = 1.0f - expf(-(float)(now - speedLastMs) / SPEED_TAU_MS);
    speedKmh += alpha * (raw - speedKmh);
  }
  speedLastMs = now;
}

// --- Échelle adaptative de la jauge ---
static int gaugeMax = GAUGE_MIN_MAX;  // ajusté dans setup() d'après les réglages
static uint32_t belowSince = 0;
static float needleKmh = 0;

static void updateGaugeScale(float kmh, uint32_t now) {
  if (gaugeMax < cfg.gaugeMin) gaugeMax = cfg.gaugeMin;
  if (kmh > gaugeMax) {
    // Montée immédiate au palier juste au-dessus
    int m = ((int)ceilf(kmh / GAUGE_STEP)) * GAUGE_STEP;
    gaugeMax = m < cfg.gaugeMin ? cfg.gaugeMin : m;
    belowSince = 0;
    return;
  }
  // Descente d'un cran seulement après un moment nettement sous le palier inférieur
  if (gaugeMax > cfg.gaugeMin && kmh < (gaugeMax - GAUGE_STEP) * GAUGE_DOWN_RATIO) {
    if (belowSince == 0) {
      belowSince = now;
    } else if (now - belowSince >= GAUGE_DOWN_MS) {
      gaugeMax -= GAUGE_STEP;
      belowSince = now;
    }
  } else {
    belowSince = 0;
  }
}

// --- Sprite : 16 bits, sinon 8 bits si la mémoire manque ---
static bool makeSprite(int w, int h) {
  band.deleteSprite();
  spr.deleteSprite();
  spr.setColorDepth(16);
  if (spr.createSprite(w, h) != nullptr) return true;
  spr.setColorDepth(8);
  return spr.createSprite(w, h) != nullptr;
}

// --- Barre d'état ---
static void drawStatusBar(bool force = false) {
  static char cache[32];
  hud::Status st = currentStatus();
  char key[32];
  snprintf(key, sizeof(key), "%d|%d|%d|%s", st.sats, st.fix, st.rec, st.clock);
  if (!force && strcmp(key, cache) == 0) return;
  strcpy(cache, key);
  hud::statusBar(scr(), st);
}

// =====================================================================
// Page 0 : compteur (jauge + tuiles)
// =====================================================================
static bool glowOn = true;        // halo de l'arc : désactivé si l'image prend trop de temps
static bool frameForce = true;    // forcer le prochain dessin (changement de page)
static uint32_t slowFrames = 0;

static void drawDial(int maxKmh, float needle, int shownKmh) {
  static float lastNeedle = -1, lastSeen = -1;
  static int lastMax = -1, lastShown = -1;
  if (!frameForce && fabsf(needle - lastNeedle) < 0.12f && maxKmh == lastMax && shownKmh == lastShown &&
      fabsf(trip.maxKmh - lastSeen) < 0.05f)
    return;
  lastNeedle = needle; lastMax = maxKmh; lastShown = shownKmh; lastSeen = trip.maxKmh;
  frameForce = false;

  uint32_t t0 = millis();
  gfx::Surf s = gfx::onSprite(spr, pg_gauge::SX, pg_gauge::SY);
  pg_gauge::draw(s, maxKmh, needle, shownKmh, trip.maxKmh, glowOn);
  spr.pushSprite(pg_gauge::SX, pg_gauge::SY);
  uint32_t dt = millis() - t0;
  if (dt > 55) {
    if (++slowFrames >= 8 && glowOn) {
      glowOn = false;
      Serial.println("Jauge lente : halo desactive");
    }
  } else if (slowFrames > 0) {
    slowFrames--;
  }
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();
    Serial.printf("jauge : %lu ms/image (halo %s)\n", (unsigned long)dt, glowOn ? "oui" : "non");
  }
}

// Tuiles : valeur redessinée seulement si le texte change
static char statCache[6][24];

static void drawStat(int slot, const char *value, const char *unit, uint16_t color = col::TXT) {
  char key[24];
  snprintf(key, sizeof(key), "%s|%s", value, unit);
  if (strcmp(key, statCache[slot]) == 0) return;
  strncpy(statCache[slot], key, sizeof(statCache[slot]) - 1);
  pg_gauge::drawTileValue(scr(), slot, value, unit, color);
}

static void drawStats() {
  char v[16];
  snprintf(v, sizeof(v), "%.1f", trip.distM / 1000.0);
  drawStat(0, v, "km");

  uint32_t sec = trip.movingMs / 1000;
  if (sec >= 3600)
    snprintf(v, sizeof(v), "%luh%02lu", (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60));
  else
    snprintf(v, sizeof(v), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  drawStat(1, v, sec >= 3600 ? "" : "min");

  snprintf(v, sizeof(v), "%d", (int)(trip.climbM + 0.5f));
  drawStat(2, v, "m");

  float avg = trip.movingMs > 0 ? (trip.distM / 1000.0) / (trip.movingMs / 3600000.0) : 0;
  snprintf(v, sizeof(v), "%.1f", avg);
  drawStat(3, v, "km/h");

  snprintf(v, sizeof(v), "%.1f", trip.maxKmh);
  drawStat(4, v, "km/h", trip.maxKmh > 0 ? col::TXT : col::SEC);

  if (fix.altValid) snprintf(v, sizeof(v), "%d", (int)(fix.alt + 0.5));
  else snprintf(v, sizeof(v), "--");
  drawStat(5, v, "m");
}

// =====================================================================
// Page 1 : direction
// =====================================================================
struct DirState {
  bool fix = false, haveDest = false;
  float heading = 0, bearing = 0, rel = 0;  // rel : -180..180, > 0 = à droite
  double distM = 0;
  const char *src = "boussole";
};

static DirState dirState() {
  DirState d;
  float kmh = currentKmh();
  d.fix = fix.valid;
  if (kmh >= COURSE_MIN_KMH && fix.courseValid) {
    d.heading = fix.course;
    d.src = "cap GPS";
  } else if (compass.ok() && compass.calibrated()) {
    d.heading = compass.heading(COMPASS_OFFSET_DEG, COMPASS_INVERT);
  }
  d.haveDest = d.fix && destValid;
  if (d.haveDest) {
    d.distM = distanceM(fix.lat, fix.lon, destLat, destLon);
    d.bearing = (float)bearingDeg(fix.lat, fix.lon, destLat, destLon);
    d.rel = pg_compass::norm360(d.bearing - d.heading + 180.0f) - 180.0f;
  }
  return d;
}

static void distStrings(const DirState &d, char *num, size_t n, const char *&unit) {
  if (!d.haveDest) { snprintf(num, n, "--"); unit = ""; }
  else if (d.distM < 1000) { snprintf(num, n, "%d", (int)d.distM); unit = "m"; }
  else if (d.distM < 10000) { snprintf(num, n, "%.1f", d.distM / 1000.0); unit = "km"; }
  else { snprintf(num, n, "%d", (int)(d.distM / 1000.0 + 0.5)); unit = "km"; }
}

static void drawCompass() {
  static float lastH = -999, lastB = -999;
  static char lastNum[12];
  DirState d = dirState();
  char num[12];
  const char *unit;
  distStrings(d, num, sizeof(num), unit);
  if (!frameForce && fabsf(d.heading - lastH) < 0.4f && fabsf(d.bearing - lastB) < 0.4f && strcmp(num, lastNum) == 0)
    return;
  lastH = d.heading; lastB = d.bearing; strcpy(lastNum, num);
  frameForce = false;
  gfx::Surf s = gfx::onSprite(spr, pg_compass::SX, pg_compass::SY);
  pg_compass::draw(s, d.heading, d.bearing, d.haveDest, num, unit);
  spr.pushSprite(pg_compass::SX, pg_compass::SY);
}

static char destCache[2][40];

static void drawDirectionTiles() {
  DirState d = dirState();
  char v[12], s1[24], s2[24], key[40];
  snprintf(v, sizeof(v), "%d°", (int)(d.heading + 0.5f) % 360);
  snprintf(key, sizeof(key), "%s|%s|%s", v, pg_compass::cardinalName(d.heading), d.src);
  if (strcmp(key, destCache[0]) != 0) {
    strcpy(destCache[0], key);
    pg_compass::tileUpdate(scr(), 4, v, col::TXT, pg_compass::cardinalName(d.heading), d.src);
  }
  if (d.haveDest) {
    snprintf(v, sizeof(v), "%d°", (int)(d.bearing + 0.5f) % 360);
    snprintf(s2, sizeof(s2), "%s %d°", d.rel >= 0 ? "droite" : "gauche", (int)(fabsf(d.rel) + 0.5f));
    strcpy(s1, pg_compass::cardinal16(d.bearing));
  } else {
    snprintf(v, sizeof(v), "--");
    s1[0] = '\0';
    snprintf(s2, sizeof(s2), d.fix ? "aucune dest." : "attente GPS");
  }
  snprintf(key, sizeof(key), "%s|%s|%s", v, s1, s2);
  if (strcmp(key, destCache[1]) != 0) {
    strcpy(destCache[1], key);
    pg_compass::tileUpdate(scr(), 251, v, d.haveDest ? col::ACCENT : col::SEC, s1, s2);
  }
}

// =====================================================================
// Trajet
// =====================================================================
static void updateTrip() {
  static uint32_t lastSeq = 0;
  if (!fix.valid || fix.seq == lastSeq) return;  // un seul traitement par nouvelle position
  lastSeq = fix.seq;
  uint32_t now = millis();
  float kmh = speedKmh;
  double lat = fix.lat, lon = fix.lon;

  if (trip.hasLast) {
    uint32_t dt = now - trip.lastTickMs;
    if (speedMoving) {
      double d = distanceM(trip.lastLat, trip.lastLon, lat, lon);
      if (d >= MIN_STEP_M) {
        trip.distM += d;
        trip.lastLat = lat;
        trip.lastLon = lon;
      }
      trip.movingMs += dt;
      if (kmh > trip.maxKmh) trip.maxKmh = kmh;
      if (fix.altValid) {  // dénivelé positif : lissage + hystérésis de 3 m
        float alt = fix.alt;
        if (!trip.hasAlt) { trip.hasAlt = true; trip.altFilt = trip.altRef = alt; }
        trip.altFilt += (alt - trip.altFilt) * 0.2f;
        if (trip.altFilt - trip.altRef >= 3.0f) { trip.climbM += trip.altFilt - trip.altRef; trip.altRef = trip.altFilt; }
        else if (trip.altFilt < trip.altRef) trip.altRef = trip.altFilt;
      }
    }
  } else {
    trip.lastLat = lat;
    trip.lastLon = lon;
    trip.hasLast = true;
  }
  trip.lastTickMs = now;
}

// =====================================================================
// Bouton BOOT
// =====================================================================
static void resetAll() {
  trip.reset();
  compass.resetCalibration();
  gaugeMax = cfg.gaugeMin;
  memset(statCache, 0, sizeof(statCache));
}

// --- Calibration du tactile : trois croix à toucher ---
static void drawCross(int x, int y, uint16_t c) {
  tft.drawLine(x - 12, y, x + 12, y, c);
  tft.drawLine(x, y - 12, x, y + 12, c);
  tft.drawCircle(x, y, 6, c);
}

static bool waitPress(int &rx, int &ry) {
  int a, b;
  while (!touch.readRaw(a, b)) {
    pollGps();
    delay(10);
  }
  long sx = 0, sy = 0;
  int n = 0;
  uint32_t t = millis();
  while (millis() - t < 350) {
    if (touch.readRaw(a, b)) {
      sx += a;
      sy += b;
      n++;
    }
    pollGps();
    delay(5);
  }
  while (touch.readRaw(a, b)) {
    pollGps();
    delay(10);
  }
  delay(150);
  if (n == 0) return false;
  rx = sx / n;
  ry = sy / n;
  return true;
}

static void touchCalibrate() {
  const int M = 20;
  const int px[3] = {M, 320 - M, 320 - M};
  const int py[3] = {M, M, 240 - M};
  int rx[3], ry[3];
  tft.fillScreen(col::BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(0);
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString("Calibration tactile", 160, 100, 4);
  tft.setTextColor(col::DIM, col::BG);
  tft.drawString("Touchez les croix", 160, 135, 2);
  for (int i = 0; i < 3; i++) {
    drawCross(px[i], py[i], col::ORANGE);
    while (!waitPress(rx[i], ry[i])) {
    }
    drawCross(px[i], py[i], col::BG);
  }
  touch.calibrate(rx[0], ry[0], rx[1], ry[1], rx[2], ry[2], M);
  Serial.printf("Tactile calibré : (%d,%d) (%d,%d) (%d,%d)\n", rx[0], ry[0], rx[1], ry[1], rx[2], ry[2]);
  pageDirty = true;
}

static void drawResetModal() {
  tft.fillRoundRect(30, 70, 260, 110, 10, col::TRACK);
  tft.drawRoundRect(30, 70, 260, 110, 10, col::TICK_MAJOR);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(0);
  tft.setTextColor(col::WHITE, col::TRACK);
  tft.drawString("Remettre a zero ?", 160, 95, 4);
  tft.fillRoundRect(45, 125, 110, 40, 6, col::BG);
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString("Annuler", 100, 145, 2);
  tft.fillRoundRect(165, 125, 110, 40, 6, col::ORANGE);
  tft.setTextColor(col::BG, col::ORANGE);
  tft.drawString("Remettre", 220, 145, 2);
}

static void handleTouch() {
  touch.dragMode = modalReset ? DRAG_NONE : (page == 2 ? DRAG_XY : (page == 3 ? (histSel >= 0 ? DRAG_XY : DRAG_Y) : DRAG_NONE));
  TouchEvent e = touch.poll(millis());
  if (e == TE_NONE) return;
  if (e == TE_DRAG) {
    if (page == 2) {
      mapPopOpen = false;
      mapDrag(touch.dx, touch.dy);
    }
    else if (page == 3) {
      if (histSel >= 0) histCursorAt(touch.curX());
      else histDrag(touch.dy);
    }
    return;
  }
  if (destMenuOpen) {  // menu Destination : une ligne le valide, tout autre toucher le ferme
    if (e == TE_TAP) {
      int r = pg_compass::destMenuHit(touch.x, touch.y);
      if (r != -2) {
        destMenuOpen = false;
        pageDirty = true;
        if (r >= 0) applyDestMenu(r);
      }
    }
    return;
  }
  if (mapPopOpen) {  // fenêtre d'appui long sur la carte
    if (e == TE_TAP) {
      int a = popHit(mapPopX, mapPopY, touch.x, touch.y);
      mapPopOpen = false;
      mapForce = true;
      if (a == 1) chooseDest(DM_POINT, mapPopLat, mapPopLon);
      else if (a == 2) setHome(mapPopLat, mapPopLon);
    }
    return;
  }
  if (page == 1 && e == TE_TAP && touch.x >= 251 && touch.y >= 80 && touch.y <= 140) {  // case DEST
    openDestMenu();
    return;
  }
  if (modalReset) {
    if (e == TE_TAP && touch.y >= 120 && touch.y <= 170) {
      if (touch.x >= 165 && touch.x <= 275) {
        resetAll();
        modalReset = false;
        pageDirty = true;
      } else if (touch.x >= 45 && touch.x < 165) {
        modalReset = false;
        pageDirty = true;
      }
    }
    return;
  }
  // Détail d'un trajet : un glissé ramène à la liste au lieu de changer de page
  if (page == 3 && histSel >= 0 && (e == TE_SWIPE_L || e == TE_SWIPE_R)) {
    histBack();
    return;
  }
  if (e == TE_TAP && page == 3) {
    histTap(touch.x, touch.y);
    return;
  }
  if (e == TE_TAP && page == 4) {
    SettingsAction a = settingsTap(touch.x, touch.y);
    if (a == SA_CALIB_TOUCH) touchCalibrate();
    if (a == SA_RESET_COMPASS) {
      compass.resetCalibration();
      settingsSetNote(6, "tournez la carte sur elle-même");
      drawSettingsRow(5);
    }
    return;
  }
  switch (e) {
    case TE_SWIPE_L:
      page = (page + 1) % NB_PAGES;
      pageDirty = true;
      break;
    case TE_SWIPE_R:
      page = (page + NB_PAGES - 1) % NB_PAGES;
      pageDirty = true;
      break;
    case TE_TAP:
      if (page == 2) {
        if (touch.x >= 268 && touch.y >= 80 && touch.y <= 166) mapZoomStep(touch.y < 122 ? +1 : -1);
        else if (mapPanned() && touch.x < 46 && touch.y >= 58 && touch.y <= 102) mapRecenter();
      }
      break;
    case TE_LONG:
      if (page <= 1) {
        modalReset = true;
        drawResetModal();
      } else if (page == 2) {  // appui long sur la carte : destination ou domicile ici
        mapPopOpen = true;
        mapPopX = touch.x;
        mapPopY = touch.y;
        mapScreenToLatLon(touch.x, touch.y, mapPopLat, mapPopLon);
        mapForce = true;
      }
      break;
    default:
      break;
  }
}

static void updateLogger(uint32_t now) {
  LogSample s;
  bool fixOk = fix.valid && fix.dateValid && fix.timeValid;
  if (fixOk) {
    s.epoch = epochFromDate(fix.year, fix.month, fix.day, fix.hour, fix.minute, fix.second);
    s.lat = fix.lat;
    s.lon = fix.lon;
    s.alt = fix.altValid ? fix.alt : 0;
    s.kmh = speedKmh;
    s.sats = fix.sats;
    s.hdop = fix.hdopValid ? fix.hdop : 0;
  }
  logger.update(now, cfg.logOn && sdOk, speedMoving, fixOk, s, cfg.tz);
}

static void handleButton() {
  static bool down = false;
  static uint32_t downMs = 0;
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  if (pressed && !down) {
    down = true;
    downMs = millis();
  } else if (!pressed && down) {
    down = false;
    uint32_t held = millis() - downMs;
    if (held > 4000) {
      touchCalibrate();  // appui très long : recalibrer le tactile
    } else if (held > 1000) {
      if (page == 2) {
        mapCycleZoom();  // sur la carte : appui long = changer de zoom
      } else {
        resetAll();
      }
    } else if (held > 30) {
      page = (page + 1) % NB_PAGES;
      pageDirty = true;
    }
  }
}

// =====================================================================
// Pages
// =====================================================================
static void enterPage() {
  modalReset = false;
  destMenuOpen = false;
  mapPopOpen = false;
  tft.fillScreen(col::BG);
  frameForce = true;
  if (page == 0) {
    sprOk = makeSprite(pg_gauge::SW, pg_gauge::SH);
  } else if (page == 1) {
    sprOk = makeSprite(pg_compass::SW, pg_compass::SH);
  } else if (page == 2) {
    spr.deleteSprite();
    band.deleteSprite();
    band.setColorDepth(16);
    sprOk = band.createSprite(320, BAND_H) != nullptr;
    mapEnter();
  } else {  // trajets, réglages : dessin direct, pas de sprite
    spr.deleteSprite();
    band.deleteSprite();
    sprOk = false;
  }
  memset(statCache, 0, sizeof(statCache));
  memset(destCache, 0, sizeof(destCache));

  drawStatusBar(true);
  if (page == 0) pg_gauge::drawTileFrames(scr());
  if (page == 1) {
    pg_compass::tile(scr(), 4, "CAP", "", col::TXT, "", "");
    pg_compass::tile(scr(), 251, "DEST.", "", col::ACCENT, "", "");
  }
  if (page <= 1) hud::pageDots(scr(), page, NB_PAGES);
  if (page == 3) histEnter();
  if (page == 4) settingsEnter();
  if (!sprOk && page < 3) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(col::ORANGE, col::BG);
    tft.drawString("Memoire insuffisante", 160, 120, 4);
  }
  pageDirty = false;
}

static float currentKmh() {
  return speedKmh;
}

static void renderFrame(uint32_t now) {
  if (!sprOk) return;
  float kmh = currentKmh();

  if (page == 2) {
    renderMapPage(now);
  } else if (page == 0) {
    updateGaugeScale(kmh, now);
    needleKmh += (kmh - needleKmh) * 0.3f;  // lissage de l'aiguille
    if (fabsf(kmh - needleKmh) < 0.05f) needleKmh = kmh;
    drawDial(gaugeMax, needleKmh, (int)(kmh + 0.5f));
  } else {
    drawCompass();
  }
}

static void renderSlow() {
  if (page != 2 || !sdOk) drawStatusBar();
  if (page == 0) drawStats();
  else if (page == 1) drawDirectionTiles();
  else if (page == 4) settingsRefresh();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  cfg.load();
  gaugeMax = cfg.gaugeMin;

  tft.init();
  tft.setRotation(1);  // paysage 320x240
  tft.fillScreen(col::BG);

  Serial2.setRxBufferSize(4096);  // marge pendant les redessins de la carte (~1 s de trames)
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, -1);

  // Carte SD (bus VSPI ; l'écran est sur HSPI, voir USE_HSPI_PORT dans platformio.ini)
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  sdOk = SD.begin(SD_CS_PIN, sdSPI, 20000000, "/sd", 8);
  Serial.printf("Carte SD : %s\n", sdOk ? "OK" : "absente");
  if (sdOk && track.load(GPX_PATH)) {
    Serial.printf("Trace GPX : %d points, %.1f km\n", track.count(), track.totalM() / 1000.0f);
  }
  updateDestination();
  if (sdOk) {
    logger.begin();
    tripsRebuildIndex();
  }
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  bool c = compass.begin();
  Serial.printf("Boussole : %s\n", compass.name());

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString(c ? "Boussole OK" : "Boussole absente", 10, 10, 2);
  delay(800);

  touch.begin(TOUCH_CLK_PIN, TOUCH_MOSI_PIN, TOUCH_MISO_PIN, TOUCH_CS_PIN, TOUCH_IRQ_PIN, 320, 240);
  if (!touch.calibrated()) touchCalibrate();
}

void loop() {
  static uint32_t lastFrame = 0, lastSlow = 0, lastCompass = 0;

  pollGps();
  if (fix.valid) {  // mémorise la dernière position connue
    lastFixOk = true;
    lastFixLat = fix.lat;
    lastFixLon = fix.lon;
  }
  updateSpeed(millis());
  updateTrip();
  updateLogger(millis());
  if (compass.ok() && millis() - lastCompass >= 20) {  // 50 Hz max
    lastCompass = millis();
    compass.update();
  }
  handleButton();
  handleTouch();

  uint32_t now = millis();
  if (mapThemeUpdate(now)) {  // thème de la carte changé (réglage ou luminosité) : autres tuiles
    Serial.printf("Thème de la carte : %s (lum. %d)\n", mapLight ? "clair" : "sombre", ldrRaw);
    mapUseTheme(mapLight);
  }
  if (pageDirty) {
    enterPage();
    renderSlow();
  }
  if (modalReset || destMenuOpen) return;  // fenêtre de confirmation : on ne redessine pas dessous
  if (now - lastFrame >= 40) {  // ~25 images/s
    lastFrame = now;
    renderFrame(now);
  }
  if (now - lastSlow >= 500) {
    lastSlow = now;
    renderSlow();
  }
}
