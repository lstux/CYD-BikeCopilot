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
#include <TinyGPSPlus.h>
#include "config.h"
#include "compass.h"
#include "ui.h"

TFT_eSPI tft;
TFT_eSprite spr(&tft);
TinyGPSPlus gps;
Compass compass;

// --- Géométrie (écran 320x240) ---
// Jauge (page 0) : centre (160,131), sprite x 64..256, y 26..214
static const int DIAL_CX = 160, DIAL_CY = 131;
static const int DIAL_X0 = 64, DIAL_Y0 = 26, DIAL_W = 192, DIAL_H = 188;
// Boussole (page 1) : centre (105,131), sprite x 9..201, y 34..228
static const int CMP_CX = 105, CMP_CY = 131;
static const int CMP_X0 = 9, CMP_Y0 = 34, CMP_W = 192, CMP_H = 194;

// --- État du trajet ---
struct Trip {
  double distM = 0;
  uint32_t movingMs = 0;
  float maxKmh = 0;
  bool hasLast = false;
  double lastLat = 0, lastLon = 0;
  uint32_t lastTickMs = 0;
  void reset() { *this = Trip(); }
} trip;

int page = 0;
bool pageDirty = true;
bool sprOk = false;
static const int NB_PAGES = 3;

// Destination : dernier point de la trace GPX si elle existe, sinon celle de config.h
double destLat = DEST_LAT, destLon = DEST_LON;

static float currentKmh();
static void pollGps() {
  while (Serial2.available()) gps.encode(Serial2.read());
}

#include "mappage.h"

// --- Vitesse filtrée (anti-dérive GPS à l'arrêt) ---
static float speedKmh = 0;
static bool speedMoving = false;
static uint32_t speedAboveSince = 0, speedLastMs = 0;

static void updateSpeed(uint32_t now) {
  float raw = (gps.speed.isValid() && gps.speed.age() < 3000) ? gps.speed.kmph() : 0;
  if (!speedMoving) {
    bool good = gps.location.isValid() && gps.satellites.value() >= SPEED_MIN_SATS &&
                gps.hdop.isValid() && gps.hdop.hdop() <= SPEED_HDOP_MAX;
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
static int gaugeMax = GAUGE_MIN_MAX;
static uint32_t belowSince = 0;
static float needleKmh = 0;

static void updateGaugeScale(float kmh, uint32_t now) {
  if (kmh > gaugeMax) {
    // Montée immédiate au palier juste au-dessus
    int m = ((int)ceilf(kmh / GAUGE_STEP)) * GAUGE_STEP;
    gaugeMax = m < GAUGE_MIN_MAX ? GAUGE_MIN_MAX : m;
    belowSince = 0;
    return;
  }
  // Descente d'un cran seulement après un moment nettement sous le palier inférieur
  if (gaugeMax > GAUGE_MIN_MAX && kmh < (gaugeMax - GAUGE_STEP) * GAUGE_DOWN_RATIO) {
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
static void localTime(char *out, size_t n) {
  if (!gps.time.isValid()) {
    snprintf(out, n, "--:--");
    return;
  }
  int h = (gps.time.hour() + TZ_OFFSET_H + 24) % 24;
  snprintf(out, n, "%02d:%02d", h, gps.time.minute());
}

static void drawStatusBar() {
  char buf[48], t[8];
  localTime(t, sizeof(t));
  bool fix = gps.location.isValid() && gps.location.age() < 3000;
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(230);
  tft.setTextColor(fix ? col::GREEN : col::ORANGE, col::BG);
  if (fix)
    snprintf(buf, sizeof(buf), "GPS %d sat  hdop %.1f", (int)gps.satellites.value(), gps.hdop.hdop());
  else
    snprintf(buf, sizeof(buf), "Recherche GPS... %d sat", (int)gps.satellites.value());
  tft.drawString(buf, 4, 3, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(60);
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString(t, 316, 3, 2);
  tft.setTextPadding(0);
}

// =====================================================================
// Page 0 : jauge
// =====================================================================
static void drawDial(int maxKmh, float needle, int shownKmh) {
  const float cx = DIAL_CX - DIAL_X0, cy = DIAL_CY - DIAL_Y0;
  const float R_IN = 83.5f, R_OUT = 92.5f;
  spr.fillSprite(col::BG);

  // Piste
  for (int a = -135; a < 135; a += 3) wedge(spr, cx, cy, R_IN, R_OUT, a, a + 3.6f, col::TRACK);

  // Arc de valeur, en dégradé
  float ratio = needle / (float)maxKmh;
  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  float aEnd = -135.0f + 270.0f * ratio;
  for (float a = -135.0f; a < aEnd; a += 3.0f) {
    float a1 = a + 3.6f;
    if (a1 > aEnd) a1 = aEnd;
    wedge(spr, cx, cy, R_IN, R_OUT, a, a1, gradient((a + 136.5f) / 270.0f));
  }

  // Graduations : une par km/h, plus longues tous les 5
  for (int i = 0; i <= maxKmh; i++) {
    float a = -135.0f + 270.0f * i / maxKmh;
    if (i % 5 == 0) {
      Pt p0 = polar(cx, cy, 72, a), p1 = polar(cx, cy, 80, a);
      thickLine(spr, p0.x, p0.y, p1.x, p1.y, 2.2f, col::TICK_MAJOR);
    } else {
      Pt p0 = polar(cx, cy, 74.5f, a), p1 = polar(cx, cy, 79.5f, a);
      spr.drawLine(p0.x, p0.y, p1.x, p1.y, col::TICK);
    }
  }

  // Chiffres tous les 5 km/h
  spr.setTextDatum(MC_DATUM);
  char buf[8];
  for (int v = 0; v <= maxKmh; v += 5) {
    float a = -135.0f + 270.0f * v / maxKmh;
    Pt p = polar(cx, cy, 58, a);
    spr.setTextColor(v == maxKmh ? col::WHITE : col::LABEL, col::BG);
    snprintf(buf, sizeof(buf), "%d", v);
    spr.drawString(buf, p.x, p.y, 2);
  }

  // Aiguille (corps large + petite queue), puis moyeu
  Pt tip = polar(cx, cy, 73, aEnd);
  Pt bl = polar(cx, cy, 3.5f, aEnd - 90);
  Pt br = polar(cx, cy, 3.5f, aEnd + 90);
  Pt tail = polar(cx, cy, 10, aEnd + 180);
  spr.fillTriangle(tip.x, tip.y, bl.x, bl.y, br.x, br.y, col::NEEDLE);
  spr.fillTriangle(tail.x, tail.y, bl.x, bl.y, br.x, br.y, col::NEEDLE);
  spr.fillCircle((int)cx, (int)cy, 8, col::TRACK);
  spr.drawCircle((int)cx, (int)cy, 8, col::NEEDLE);
  spr.drawCircle((int)cx, (int)cy, 7, col::NEEDLE);
  spr.fillCircle((int)cx, (int)cy, 3, col::NEEDLE);

  // Vitesse numérique au centre
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(col::WHITE, col::BG);
  snprintf(buf, sizeof(buf), "%d", shownKmh);
  spr.drawString(buf, (int)cx, 166 - DIAL_Y0, 6);
  spr.setTextColor(col::DIM, col::BG);
  spr.drawString("km/h", (int)cx, 206 - DIAL_Y0, 2);

  spr.pushSprite(DIAL_X0, DIAL_Y0);
}

// Bloc de statistique : libellé, valeur, unité. Redessiné seulement si le texte change.
static char statCache[4][24];

static void drawStat(int slot, bool right, int y, const char *label, const char *value,
                     const char *unit) {
  char key[24];
  snprintf(key, sizeof(key), "%s|%s", value, unit);
  if (strcmp(key, statCache[slot]) == 0) return;
  strncpy(statCache[slot], key, sizeof(statCache[slot]) - 1);

  const int x = right ? 316 : 4;
  tft.fillRect(right ? 256 : 0, y, 64, 62, col::BG);
  tft.setTextPadding(0);
  tft.setTextDatum(right ? TR_DATUM : TL_DATUM);
  tft.setTextColor(col::DIM, col::BG);
  tft.drawString(label, x, y, 2);
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString(value, x, y + 16, 4);
  tft.setTextColor(col::DIM, col::BG);
  tft.drawString(unit, x, y + 44, 2);
}

static void drawStats() {
  char v[16];
  snprintf(v, sizeof(v), "%.1f", trip.distM / 1000.0);
  drawStat(0, false, 28, "DIST", v, "km");

  uint32_t s = trip.movingMs / 1000;
  if (s >= 3600)
    snprintf(v, sizeof(v), "%luh%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60));
  else
    snprintf(v, sizeof(v), "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  drawStat(1, false, 168, "TEMPS", v, "");

  float avg = trip.movingMs > 0 ? (trip.distM / 1000.0) / (trip.movingMs / 3600000.0) : 0;
  snprintf(v, sizeof(v), "%.1f", avg);
  drawStat(2, true, 28, "MOY", v, "km/h");

  snprintf(v, sizeof(v), "%.1f", trip.maxKmh);
  drawStat(3, true, 168, "MAX", v, "km/h");
}

// =====================================================================
// Page 1 : boussole + destination
// =====================================================================
static char destCache[3][24];

static void drawCompass(float heading, bool haveArrow, float relDeg) {
  const float cx = CMP_CX - CMP_X0, cy = CMP_CY - CMP_Y0;
  spr.fillSprite(col::BG);

  // Anneau
  for (int r = 91; r <= 95; r++) spr.drawCircle((int)cx, (int)cy, r, col::TRACK);

  // Graduations tous les 5 degrés, plus longues tous les 45
  for (int i = 0; i < 72; i++) {
    float a = i * 5.0f - heading;
    if (i % 9 == 0) {
      Pt p0 = polar(cx, cy, 76, a), p1 = polar(cx, cy, 86, a);
      thickLine(spr, p0.x, p0.y, p1.x, p1.y, 2.2f, col::TICK_MAJOR);
    } else {
      Pt p0 = polar(cx, cy, 81, a), p1 = polar(cx, cy, 86, a);
      spr.drawLine(p0.x, p0.y, p1.x, p1.y, col::TICK);
    }
  }

  // Points cardinaux (le haut de l'écran = sens de marche)
  static const char *names[4] = {"N", "E", "S", "O"};
  spr.setTextDatum(MC_DATUM);
  for (int i = 0; i < 4; i++) {
    Pt p = polar(cx, cy, 64, i * 90.0f - heading);
    spr.setTextColor(i == 0 ? col::NEEDLE : col::LABEL, col::BG);
    spr.drawString(names[i], p.x, p.y, 4);
  }

  // Flèche vers la destination
  if (haveArrow) {
    float a = relDeg * (float)M_PI / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    const float P[4][2] = {{0, -54}, {-30, 38}, {0, 20}, {30, 38}};  // pointe, gauche, creux, droite
    int X[4], Y[4];
    for (int i = 0; i < 4; i++) {
      X[i] = (int)lroundf(cx + P[i][0] * ca - P[i][1] * sa);
      Y[i] = (int)lroundf(cy + P[i][0] * sa + P[i][1] * ca);
    }
    spr.fillTriangle(X[0], Y[0], X[1], Y[1], X[2], Y[2], col::CYAN);
    spr.fillTriangle(X[0], Y[0], X[3], Y[3], X[2], Y[2], col::BLUE);
  }

  spr.pushSprite(CMP_X0, CMP_Y0);
}

// Texte suivi d'un petit rond (symbole degré, absent des polices)
static void drawDegText(int x, int y, const char *txt, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, col::BG);
  tft.drawString(txt, x, y, 2);
  int w = tft.textWidth(txt, 2);
  tft.drawCircle(x + w + 3, y + 3, 2, color);
}

static void drawDestinationInfo(bool fix, double distM, double bearing, float heading,
                                const char *src, float kmh) {
  const int X = 209;
  char num[12], unit[4], l1[24], l2[24], l3[24];
  if (fix) {
    if (distM < 1000) {
      snprintf(num, sizeof(num), "%d", (int)distM);
      snprintf(unit, sizeof(unit), "m");
    } else if (distM < 10000) {
      snprintf(num, sizeof(num), "%.1f", distM / 1000.0);
      snprintf(unit, sizeof(unit), "km");
    } else {
      snprintf(num, sizeof(num), "%d", (int)(distM / 1000.0 + 0.5));
      snprintf(unit, sizeof(unit), "km");
    }
    snprintf(l1, sizeof(l1), "azimut %d", (int)bearing);
    snprintf(l2, sizeof(l2), "%s %d", src, (int)heading);
    snprintf(l3, sizeof(l3), "%d km/h", (int)(kmh + 0.5f));
  } else {
    snprintf(num, sizeof(num), "--");
    unit[0] = '\0';
    snprintf(l1, sizeof(l1), "attente GPS");
    l2[0] = '\0';
    l3[0] = '\0';
  }

  char k0[24], k1[24], k2[24];
  snprintf(k0, sizeof(k0), "%s|%s", num, unit);
  snprintf(k1, sizeof(k1), "%s|%s", l1, l2);
  snprintf(k2, sizeof(k2), "%s", l3);

  if (strcmp(k0, destCache[0]) != 0) {
    strncpy(destCache[0], k0, sizeof(destCache[0]) - 1);
    tft.fillRect(X, 60, 111, 54, col::BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
    tft.setTextColor(col::WHITE, col::BG);
    tft.drawString(num, X, 62, 6);
    int w = tft.textWidth(num, 6);
    tft.setTextColor(col::TEXT2, col::BG);
    tft.drawString(unit, X + w + 4, 84, 4);
  }
  if (strcmp(k1, destCache[1]) != 0) {
    strncpy(destCache[1], k1, sizeof(destCache[1]) - 1);
    tft.fillRect(X, 124, 111, 40, col::BG);
    tft.setTextPadding(0);
    if (fix) {
      drawDegText(X, 126, l1, col::TEXT2);
      drawDegText(X, 146, l2, col::TEXT2);
    } else {
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(col::ORANGE, col::BG);
      tft.drawString(l1, X, 126, 2);
    }
  }
  if (strcmp(k2, destCache[2]) != 0) {
    strncpy(destCache[2], k2, sizeof(destCache[2]) - 1);
    tft.fillRect(X, 170, 111, 30, col::BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
    tft.setTextColor(col::WHITE, col::BG);
    tft.drawString(l3, X, 172, 4);
  }
}

// =====================================================================
// Trajet
// =====================================================================
static void updateTrip() {
  if (!(gps.location.isValid() && gps.location.isUpdated())) return;
  uint32_t now = millis();
  float kmh = speedKmh;
  double lat = gps.location.lat(), lon = gps.location.lng();

  if (trip.hasLast) {
    uint32_t dt = now - trip.lastTickMs;
    if (speedMoving) {
      double d = TinyGPSPlus::distanceBetween(trip.lastLat, trip.lastLon, lat, lon);
      if (d >= MIN_STEP_M) {
        trip.distM += d;
        trip.lastLat = lat;
        trip.lastLon = lon;
      }
      trip.movingMs += dt;
      if (kmh > trip.maxKmh) trip.maxKmh = kmh;
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
    if (held > 1000) {
      if (page == 2) {
        mapCycleZoom();  // sur la carte : appui long = changer de zoom
      } else {
        trip.reset();
        compass.resetCalibration();
        gaugeMax = GAUGE_MIN_MAX;
        memset(statCache, 0, sizeof(statCache));
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
  tft.fillScreen(col::BG);
  tft.fillRect(0, 22, 320, 2, col::RULE);
  if (page == 0) {
    sprOk = makeSprite(DIAL_W, DIAL_H);
  } else if (page == 1) {
    sprOk = makeSprite(CMP_W, CMP_H);
  } else {
    spr.deleteSprite();
    band.deleteSprite();
    band.setColorDepth(16);
    sprOk = band.createSprite(320, BAND_H) != nullptr;
    mapEnter();
  }
  memset(statCache, 0, sizeof(statCache));
  memset(destCache, 0, sizeof(destCache));

  if (page == 1) {
    tft.fillTriangle(CMP_CX - 5, 24, CMP_CX + 5, 24, CMP_CX, 33, col::WHITE);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
    tft.setTextColor(col::DIM, col::BG);
    tft.drawString("DESTINATION", 209, 40, 2);
  }
  if (!sprOk) {
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
    bool fix = gps.location.isValid() && gps.location.age() < 3000;
    float heading = 0;
    if (kmh >= COURSE_MIN_KMH && gps.course.isValid())
      heading = gps.course.deg();
    else if (compass.ok() && compass.calibrated())
      heading = compass.heading(COMPASS_OFFSET_DEG, COMPASS_INVERT);

    double bearing = 0;
    if (fix) bearing = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), destLat, destLon);
    drawCompass(heading, fix, (float)bearing - heading);
  }
}

static void renderSlow() {
  drawStatusBar();
  float kmh = currentKmh();

  if (page == 0) {
    drawStats();
  } else if (page == 1) {
    bool fix = gps.location.isValid() && gps.location.age() < 3000;
    double dist = 0, bearing = 0;
    if (fix) {
      dist = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), destLat, destLon);
      bearing = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), destLat, destLon);
    }
    float heading = 0;
    const char *src = "cap";
    if (kmh >= COURSE_MIN_KMH && gps.course.isValid()) {
      heading = gps.course.deg();
      src = "cap GPS";
    } else if (compass.ok() && compass.calibrated()) {
      heading = compass.heading(COMPASS_OFFSET_DEG, COMPASS_INVERT);
      src = "boussole";
    }
    drawDestinationInfo(fix, dist, bearing, heading, src, kmh);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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
    destLat = track.destLat();
    destLon = track.destLon();
    Serial.printf("Trace GPX : %d points, %.1f km\n", track.count(), track.totalM() / 1000.0f);
  }
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  bool c = compass.begin();
  Serial.printf("Boussole : %s\n", compass.name());

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col::WHITE, col::BG);
  tft.drawString(c ? "Boussole OK" : "Boussole absente", 10, 10, 2);
  delay(800);
}

void loop() {
  static uint32_t lastFrame = 0, lastSlow = 0, lastCompass = 0;

  while (Serial2.available()) gps.encode(Serial2.read());
  updateSpeed(millis());
  updateTrip();
  if (compass.ok() && millis() - lastCompass >= 20) {  // 50 Hz max
    lastCompass = millis();
    compass.update();
  }
  handleButton();

  uint32_t now = millis();
  if (pageDirty) {
    enterPage();
    renderSlow();
  }
  if (now - lastFrame >= 40) {  // ~25 images/s
    lastFrame = now;
    renderFrame(now);
  }
  if (now - lastSlow >= 500) {
    lastSlow = now;
    renderSlow();
  }
}
