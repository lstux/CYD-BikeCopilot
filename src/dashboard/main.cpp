// Tableau de bord vélo - CYD (ESP32-2432S028) + GPS + QMC5883
// Page 0 : vitesse, distance, moyenne, temps, max
// Page 1 : flèche vers la destination (cap GPS en roulant, boussole à l'arrêt)
// Bouton BOOT : appui court = page suivante, appui long (>1 s) = reset du trajet
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <TinyGPSPlus.h>
#include "config.h"
#include "compass.h"

TFT_eSPI tft;
TFT_eSprite arrow(&tft);
TinyGPSPlus gps;
Compass compass;

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

// --- Utilitaires d'affichage ---
static void drawCell(int x, int y, const char *label, const char *value) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextPadding(0);
  tft.drawString(label, x + 8, y, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(140);
  tft.drawString(value, x + 8, y + 18, 4);
}

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
  tft.setTextColor(fix ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
  if (fix)
    snprintf(buf, sizeof(buf), "GPS %d sat  hdop %.1f", (int)gps.satellites.value(), gps.hdop.hdop());
  else
    snprintf(buf, sizeof(buf), "Recherche GPS... %d sat", (int)gps.satellites.value());
  tft.drawString(buf, 4, 2, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(60);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(t, 316, 2, 2);
}

// --- Page 0 : compteur ---
static void drawPage0() {
  if (pageDirty) {
    tft.fillScreen(TFT_BLACK);
    tft.drawFastHLine(0, 22, 320, TFT_DARKGREY);
    tft.drawFastHLine(0, 122, 320, TFT_DARKGREY);
    tft.drawFastVLine(160, 122, 118, TFT_DARKGREY);
    tft.drawFastHLine(0, 180, 320, TFT_DARKGREY);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextPadding(0);
    tft.drawString("km/h", 316, 100, 2);
    pageDirty = false;
  }
  drawStatusBar();

  char buf[24];
  float kmh = gps.speed.isValid() ? gps.speed.kmph() : 0;
  if (kmh < 1.0) kmh = 0;  // évite le bruit à l'arrêt
  snprintf(buf, sizeof(buf), "%d", (int)(kmh + 0.5f));
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(220);
  tft.drawString(buf, 160, 34, 8);

  snprintf(buf, sizeof(buf), "%.2f km", trip.distM / 1000.0);
  drawCell(0, 126, "DISTANCE", buf);

  float avg = trip.movingMs > 0 ? (trip.distM / 1000.0) / (trip.movingMs / 3600000.0) : 0;
  snprintf(buf, sizeof(buf), "%.1f km/h", avg);
  drawCell(160, 126, "MOYENNE", buf);

  uint32_t s = trip.movingMs / 1000;
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)(s / 3600),
           (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  drawCell(0, 184, "TEMPS (roulant)", buf);

  snprintf(buf, sizeof(buf), "%.1f km/h", trip.maxKmh);
  drawCell(160, 184, "VITESSE MAX", buf);
}

// --- Page 1 : navigation à vol d'oiseau ---
static void drawArrow(float relDeg) {
  const int C = 60;  // centre du sprite 120x120
  float a = relDeg * (float)M_PI / 180.0f;
  float ca = cosf(a), sa = sinf(a);
  auto rot = [&](float x, float y, int &ox, int &oy) {
    ox = C + (int)lroundf(x * ca - y * sa);
    oy = C + (int)lroundf(x * sa + y * ca);
  };
  int tx, ty, lx, ly, nx, ny, rx, ry;
  rot(0, -54, tx, ty);
  rot(-30, 38, lx, ly);
  rot(0, 20, nx, ny);
  rot(30, 38, rx, ry);

  arrow.fillSprite(TFT_BLACK);
  arrow.drawCircle(C, C, 58, TFT_DARKGREY);
  arrow.fillTriangle(tx, ty, lx, ly, nx, ny, TFT_CYAN);
  arrow.fillTriangle(tx, ty, rx, ry, nx, ny, TFT_BLUE);
  arrow.pushSprite(10, 60);
}

static void drawPage1() {
  if (pageDirty) {
    tft.fillScreen(TFT_BLACK);
    tft.drawFastHLine(0, 22, 320, TFT_DARKGREY);
    pageDirty = false;
  }
  drawStatusBar();

  bool fix = gps.location.isValid() && gps.location.age() < 3000;
  if (!fix) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextPadding(320);
    tft.drawString("En attente du fix GPS", 160, 120, 4);
    return;
  }

  double dist = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), DEST_LAT, DEST_LON);
  double bearing = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), DEST_LAT, DEST_LON);

  // Cap : GPS en roulant, boussole sinon
  float heading = 0;
  const char *src = "cap ?";
  float kmh = gps.speed.isValid() ? gps.speed.kmph() : 0;
  if (kmh >= COURSE_MIN_KMH && gps.course.isValid()) {
    heading = gps.course.deg();
    src = "cap GPS";
  } else if (compass.ok() && compass.calibrated()) {
    heading = compass.heading(COMPASS_OFFSET_DEG, COMPASS_INVERT);
    src = "boussole";
  } else if (compass.ok()) {
    src = "calibrer: tourner";
  }

  float rel = (float)bearing - heading;
  drawArrow(rel);

  char buf[32];
  const char *unit;
  if (dist >= 1000) {
    snprintf(buf, sizeof(buf), "%.1f", dist / 1000.0);
    unit = "km";
  } else {
    snprintf(buf, sizeof(buf), "%d", (int)dist);
    unit = "m";
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextPadding(0);
  tft.drawString("DESTINATION", 150, 50, 2);
  // Police 6 = chiffres uniquement : l'unité est dessinée à part
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(110);
  tft.drawString(buf, 150, 70, 6);
  tft.setTextPadding(40);
  tft.drawString(unit, 266, 88, 4);

  tft.setTextPadding(160);
  snprintf(buf, sizeof(buf), "azimut %d deg", (int)bearing);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(buf, 150, 125, 2);
  snprintf(buf, sizeof(buf), "%s %d deg", src, (int)heading);
  tft.drawString(buf, 150, 145, 2);
  snprintf(buf, sizeof(buf), "%d km/h", (int)(kmh + 0.5f));
  tft.drawString(buf, 150, 165, 2);
}

// --- Mise à jour du trajet (à chaque nouvelle position) ---
static void updateTrip() {
  if (!(gps.location.isValid() && gps.location.isUpdated())) return;
  uint32_t now = millis();
  float kmh = gps.speed.isValid() ? gps.speed.kmph() : 0;
  double lat = gps.location.lat(), lon = gps.location.lng();

  if (trip.hasLast) {
    uint32_t dt = now - trip.lastTickMs;
    if (kmh >= MOVING_MIN_KMH) {
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

// --- Bouton BOOT ---
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
      trip.reset();
      compass.resetCalibration();
    } else if (held > 30) {
      page = (page + 1) % 2;
    }
    pageDirty = true;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);  // paysage 320x240
  tft.fillScreen(TFT_BLACK);
  arrow.setColorDepth(16);
  arrow.createSprite(120, 120);

  Serial2.setRxBufferSize(1024);  // marge pendant les redessins de l'écran
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, -1);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  bool c = compass.begin();
  Serial.printf("Boussole : %s\n", compass.name());

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(c ? "Boussole OK" : "Boussole absente", 10, 10, 2);
  delay(800);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  static uint32_t lastDraw = 0, lastCompass = 0;

  while (Serial2.available()) gps.encode(Serial2.read());
  updateTrip();
  if (compass.ok() && millis() - lastCompass >= 20) {  // 50 Hz max
    lastCompass = millis();
    compass.update();
  }
  handleButton();

  if (millis() - lastDraw >= 250) {
    lastDraw = millis();
    if (page == 0)
      drawPage0();
    else
      drawPage1();
  }
}
