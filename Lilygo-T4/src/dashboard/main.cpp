// BikeCopilot - LilyGO T4 V1.3 (ESP32 + ILI9341 320x240, 4 boutons), position fournie par le téléphone.
// Page 0 : jauge de vitesse à échelle adaptative + statistiques du trajet
// Page 1 : carte OSM (tuiles sur la SD, trace GPX, position) : BTN1/BTN2 = zoom, BTN3 = recentrer
// Page 2 : état de la liaison avec le téléphone (Wi-Fi, SensorServer, dernière position)
//
// Le téléphone envoie sa position via l'appli Android SensorServer (voir phonelink.cpp, tâche sur
// le cœur 0). phoneApply() la convertit en Fix, comme gpsfix.h le fait pour le GPS série de la CYD.
// Les pages dessinent depuis ../../../common/src ; ici : entrées (boutons), liaison, main.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "fix.h"
#include "settings.h"
#include "hud.h"
#include "page_gauge.h"
#include "phonefix.h"
#include "phonelink.h"
#include "speed.h"
#include "buttons.h"
#include "status.h"
#include "mapboard.h"  // compass, thème, destination et carte SD pour la page carte

TFT_eSPI tft;
TFT_eSprite spr(&tft);
Settings cfg;  // réglages mémorisés (fuseau horaire, échelle mini de la jauge), lus par common/settings.h

static Fix fix;
static PhoneTracker phoneTrk;
static SpeedFilter speed;
static TripStats trip;
static GaugeScale scale;
static Button btn[4];
static const uint8_t BTN_PINS[4] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN};

static const int NB_PAGES = 3;  // 0 compteur, 1 carte, 2 liaison téléphone
#define MAP_PAGE_IDX 1
static const int PAGE_MAP = 1, PAGE_LINK = 2;
static int page = 0;
static bool pageDirty = true;
static bool sprOk = false;
static bool modalReset = false;  // fenêtre « Remettre à zéro ? » affichée
static bool frameForce = true;

static PhoneLinkInfo linkInfo;

// Variables de la page carte (common/src/mappage.h) : dernière position connue et appui long
bool lastFixOk = false;
double lastFixLat = 0, lastFixLon = 0;
bool mapPopOpen = false;
int mapPopX = 0, mapPopY = 0;
double mapPopLat = 0, mapPopLon = 0;

static float currentKmh() { return speed.kmh; }

static void localClock(char *out, size_t n);  // défini plus bas (heure locale)

// État affiché par la barre de la carte (pas de satellites : le téléphone ne les donne pas)
static hud::Status currentStatus() {
  hud::Status st;
  st.fix = fix.valid;
  st.sats = 0;
  st.rec = 0;
  localClock(st.clock, sizeof(st.clock));
  return st;
}

// Vitesse retenue pour l'heure d'arrivée estimée (même règle que la CYD)
static float etaSpeedKmh() {
  float v = 15.0f;
  if (trip.movingMs > 60000) v = (float)(trip.distM / 1000.0) / (trip.movingMs / 3600000.0f);
  else if (currentKmh() > 8) v = currentKmh();
  return v < 5.0f ? 5.0f : v;
}

#include "mappage.h"  // page carte commune : tuiles, trace, position (définit `band` et `track`)

// Destination : point choisi, domicile, sinon fin de la trace GPX (même ordre que la CYD, sans l'ordre des modes inconnus)
static void setDestPoint(double la, double lo) {
  destLat = la;
  destLon = lo;
  destValid = true;
}
static void updateDestination() {
  bool home = cfg.homeOk, trk = track.valid(), pt = cfg.pointOk;
  int m = cfg.destMode;
  if (m == DM_POINT && pt) setDestPoint(cfg.pointLat, cfg.pointLon);
  else if (m == DM_HOME && home) setDestPoint(cfg.homeLat, cfg.homeLon);
  else if (trk) setDestPoint(track.destLat(), track.destLon());
  else if (home) setDestPoint(cfg.homeLat, cfg.homeLon);
  else destValid = false;
}

static gfx::Surf scr() { return gfx::onScreen(tft, col::BG); }

// --- Sprite : 16 bits, sinon 8 bits si la mémoire manque ---
static bool makeSprite(int w, int h) {
  spr.deleteSprite();
  spr.setColorDepth(16);
  if (spr.createSprite(w, h) != nullptr) return true;
  spr.setColorDepth(8);
  return spr.createSprite(w, h) != nullptr;
}

// --- Barre d'état ---
static void localClock(char *out, size_t n) {
  if (!fix.timeValid) {
    snprintf(out, n, "--:--");
    return;
  }
  snprintf(out, n, "%02d:%02d", (fix.hour + cfg.tz + 24) % 24, fix.minute);
}

static const char *linkText() {
#ifdef PHONE_LINK_BLE
  if (!linkInfo.bleAdvertising && !linkInfo.bleConnected) return "démarrage";
  if (!linkInfo.bleConnected) return "attente du téléphone";
  return "attente GPS";
#else
  if (linkInfo.mode == 0) return "démarrage";
  if (linkInfo.mode == 2 && linkInfo.apClients == 0) return "Wi-Fi : mode AP";
  if (!linkInfo.wsConnected) return linkInfo.wsStarted ? "connexion" : "recherche appli";
  return "attente GPS";
#endif
}

static void drawStatusBar(bool force = false) {
  static char cache[48];
  t4::Status st;
  st.fix = fix.valid;
  st.accValid = phoneTrk.accValid;
  st.accM = phoneTrk.acc;
  st.link = linkText();
  localClock(st.clock, sizeof(st.clock));
  char key[48];
  snprintf(key, sizeof(key), "%d|%d|%d|%s|%s", st.fix, st.accValid, (int)(st.accM + 0.5f), st.link, st.clock);
  if (!force && strcmp(key, cache) == 0) return;
  strcpy(cache, key);
  t4::statusBar(scr(), st);
}

// =====================================================================
// Page 0 : compteur (jauge + tuiles)
// =====================================================================
static bool glowOn = true;  // halo de l'arc : désactivé si l'image prend trop de temps
static uint32_t slowFrames = 0;
static float needleKmh = 0;

static void drawDial(int maxKmh, float needle, int shownKmh) {
  static float lastNeedle = -1, lastSeen = -1;
  static int lastMax = -1, lastShown = -1;
  if (!frameForce && fabsf(needle - lastNeedle) < 0.12f && maxKmh == lastMax && shownKmh == lastShown &&
      fabsf(trip.maxKmh - lastSeen) < 0.05f)
    return;
  lastNeedle = needle;
  lastMax = maxKmh;
  lastShown = shownKmh;
  lastSeen = trip.maxKmh;
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
  if (sec >= 3600) snprintf(v, sizeof(v), "%luh%02lu", (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60));
  else snprintf(v, sizeof(v), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  drawStat(1, v, sec >= 3600 ? "" : "min");

  snprintf(v, sizeof(v), "%d", (int)(trip.climbM + 0.5f));
  drawStat(2, v, "m");

  snprintf(v, sizeof(v), "%.1f", trip.avgKmh());
  drawStat(3, v, "km/h");

  snprintf(v, sizeof(v), "%.1f", trip.maxKmh);
  drawStat(4, v, "km/h", trip.maxKmh > 0 ? col::TXT : col::SEC);

  if (fix.altValid) snprintf(v, sizeof(v), "%d", (int)(fix.alt + 0.5));
  else snprintf(v, sizeof(v), "--");
  drawStat(5, v, "m");
}

// =====================================================================
// Page 1 : liaison téléphone
// =====================================================================
static char lineCache[8][72];

// Une ligne de texte : effacée et redessinée seulement si elle change
static void drawLine(int i, const char *str, bool title = false, uint16_t color = col::TXT) {
  if (strcmp(str, lineCache[i]) == 0) return;
  strncpy(lineCache[i], str, sizeof(lineCache[i]) - 1);
  const int y0 = 26 + i * 25;
  gfx::rect(scr(), 0, y0, 320, 24, col::BG);
  if (title) gfx::text(scr(), FONT_B17, 8, y0 + 18, str, col::ACCENT);
  else gfx::text(scr(), FONT_S13, 8, y0 + 17, str, color);
}

// Lignes 5 et 6 : dernière position reçue (commun aux deux types de liaison)
static void drawPositionLines() {
  char b[72];
  if (fix.valid) {
    snprintf(b, sizeof(b), "Position : %.5f  %.5f", fix.lat, fix.lon);
    drawLine(5, b);
    float age = (millis() - phoneTrk.lastNewMs) / 1000.0f;
    if (phoneTrk.accValid) snprintf(b, sizeof(b), "Précision %d m  -  vitesse %.1f km/h", (int)(phoneTrk.acc + 0.5f), fix.kmh);
    else snprintf(b, sizeof(b), "Vitesse %.1f km/h  -  point reçu il y a %.0f s", fix.kmh, age);
    drawLine(6, b, false, col::SEC);
  } else {
    drawLine(5, "Position : aucune", false, col::SEC);
    drawLine(6, "");
  }
}

#ifdef PHONE_LINK_BLE
static void drawLinkPage() {
  char b[72];
  drawLine(0, "Liaison téléphone (Bluetooth)", true);
  snprintf(b, sizeof(b), "Carte : %s", linkInfo.bleName);
  drawLine(1, b);
  if (linkInfo.bleConnected) {
    drawLine(2, "Téléphone : connecté", false, col::OK);
    snprintf(b, sizeof(b), "MTU %u  -  %lu positions reçues", (unsigned)linkInfo.bleMtu, (unsigned long)linkInfo.bleMsgs);
    drawLine(3, b, false, col::SEC);
  } else {
    drawLine(2, "Téléphone : en attente...", false, col::AMBER);
    drawLine(3, "Lancer BLE-Companion, puis Démarrer", false, col::SEC);
  }
  drawLine(4, "");
  drawPositionLines();
}
#else
static void drawLinkPage() {
  char b[72];
  drawLine(0, "Liaison téléphone", true);
  if (linkInfo.mode == 2) {
    snprintf(b, sizeof(b), "Wi-Fi de la carte : %s", linkInfo.ssid);
    drawLine(1, b);
    snprintf(b, sizeof(b), "Mot de passe : %s", linkInfo.apPass);
    drawLine(2, b, false, col::SEC);
    snprintf(b, sizeof(b), "Portail : http://%s/  (%d connecté)", linkInfo.ip, linkInfo.apClients);
    drawLine(3, b, false, col::SEC);
  } else if (linkInfo.mode == 1) {
    snprintf(b, sizeof(b), "Wi-Fi : %s", linkInfo.ssid);
    drawLine(1, b);
    snprintf(b, sizeof(b), "IP de la carte : %s", linkInfo.ip);
    drawLine(2, b, false, col::SEC);
    drawLine(3, "Portail de réglage : même adresse", false, col::SEC);
  } else {
    drawLine(1, "Démarrage du Wi-Fi...");
    drawLine(2, "");
    drawLine(3, "");
  }
  if (linkInfo.wsConnected) snprintf(b, sizeof(b), "SensorServer : %s connecté", linkInfo.host);
  else if (linkInfo.wsStarted) snprintf(b, sizeof(b), "SensorServer : connexion à %s...", linkInfo.host);
  else snprintf(b, sizeof(b), "SensorServer : recherche (port %u)...", linkInfo.port);
  drawLine(4, b, false, linkInfo.wsConnected ? col::OK : col::AMBER);
  drawPositionLines();
}
#endif

// =====================================================================
// Remise à zéro du trajet
// =====================================================================
static void resetAll() {
  trip.reset();
  scale.maxKmh = cfg.gaugeMin;
  memset(statCache, 0, sizeof(statCache));
}

static void drawResetModal() {
  gfx::Surf s = scr();
  gfx::card(s, 30, 62, 260, 116, 10, col::TRACK, col::TICKMAJ);
  gfx::Surf c = gfx::withBg(s, col::TRACK);
  gfx::text(c, FONT_B17, 160, 98, "Remettre à zéro ?", col::WHITE, gfx::AL_C);
  gfx::text(c, FONT_S13, 160, 130, "Appui long BTN3 : remettre", col::AMBER, gfx::AL_C);
  gfx::text(c, FONT_S13, 160, 154, "Autre bouton : annuler", col::SEC, gfx::AL_C);
}

// =====================================================================
// Boutons
// =====================================================================
static void handleButtons(uint32_t now) {
  BtnEvent e[4];
  for (int i = 0; i < 4; i++) e[i] = btn[i].update(digitalRead(BTN_PINS[i]) == LOW, now);

#ifdef PHONE_LINK_BLE
  if (e[3] == BE_VLONG) {  // BTN4 (BOOT) 5 s : déconnecter le téléphone (pour en appairer un autre)
    phoneLinkDisconnect();
    pageDirty = true;
    return;
  }
#else
  if (e[3] == BE_VLONG) {  // BTN4 (BOOT) 5 s : oublier le Wi-Fi
    tft.fillScreen(col::BG);
    gfx::text(scr(), FONT_B17, 160, 120, "Configuration Wi-Fi effacée", col::WHITE, gfx::AL_C);
    phoneLinkForgetWifi();
    return;
  }
#endif

  if (modalReset) {
    if (e[2] == BE_LONG) {
      resetAll();
      modalReset = false;
      pageDirty = true;
    } else if (e[0] == BE_SHORT || e[1] == BE_SHORT || e[2] == BE_SHORT || e[3] == BE_SHORT || e[0] == BE_LONG ||
               e[1] == BE_LONG || e[3] == BE_LONG) {
      modalReset = false;
      pageDirty = true;
    }
    return;
  }

  if (page == PAGE_MAP) {  // carte : appuis courts = zoom / recentrer, appuis longs = page précédente / suivante
    if (e[0] == BE_SHORT) mapZoomStep(-1);
    else if (e[1] == BE_SHORT) mapZoomStep(+1);
    else if (e[2] == BE_SHORT && mapPanned()) mapRecenter();
    else if (e[0] == BE_LONG) {
      page = 0;
      pageDirty = true;
    } else if (e[1] == BE_LONG) {
      page = PAGE_LINK;
      pageDirty = true;
    }
    return;
  }
  if (e[0] == BE_SHORT) {  // page précédente
    page = (page + NB_PAGES - 1) % NB_PAGES;
    pageDirty = true;
  } else if (e[1] == BE_SHORT) {  // page suivante
    page = (page + 1) % NB_PAGES;
    pageDirty = true;
  }
  if (e[2] == BE_LONG && page == 0) {
    modalReset = true;
    drawResetModal();
  }
}

// =====================================================================
// Pages
// =====================================================================
static void enterPage() {
  modalReset = false;
  tft.fillScreen(col::BG);
  frameForce = true;
  if (page == 0) sprOk = makeSprite(pg_gauge::SW, pg_gauge::SH);
  else if (page == PAGE_MAP) {
    spr.deleteSprite();
    band.deleteSprite();
    band.setColorDepth(16);
    sprOk = band.createSprite(320, BAND_H) != nullptr;
    mapEnter();
  } else {
    spr.deleteSprite();
    sprOk = true;
  }
  memset(statCache, 0, sizeof(statCache));
  memset(lineCache, 0, sizeof(lineCache));

  if (page != PAGE_MAP) {  // la carte dessine sa propre barre d'état et ses puces
    drawStatusBar(true);
    hud::pageDots(scr(), page, NB_PAGES);
  }
  if (page == 0) pg_gauge::drawTileFrames(scr());
  if (!sprOk) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(col::ORANGE, col::BG);
    tft.drawString("Memoire insuffisante", 160, 120, 4);
  }
  pageDirty = false;
}

static void renderFrame(uint32_t now) {
  if (!sprOk) return;
  if (page == PAGE_MAP) {
    renderMapPage(now);
    return;
  }
  if (page != 0) return;
  float kmh = speed.kmh;
  scale.update(kmh, now, cfg.gaugeMin);
  needleKmh += (kmh - needleKmh) * 0.3f;  // lissage de l'aiguille
  if (fabsf(kmh - needleKmh) < 0.05f) needleKmh = kmh;
  drawDial(scale.maxKmh, needleKmh, (int)(kmh + 0.5f));
}

static void renderSlow() {
  phoneLinkGetInfo(linkInfo);
  if (page != PAGE_MAP) drawStatusBar();  // la carte dessine sa propre barre
  if (page == 0) drawStats();
  else if (page == PAGE_LINK) drawLinkPage();
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 3; i++) pinMode(BTN_PINS[i], INPUT);  // GPIO37-39 : pas de pull-up interne, la carte en a un
  pinMode(BTN4_PIN, INPUT_PULLUP);
  cfg.load();
  scale.maxKmh = cfg.gaugeMin;

  tft.init();
  tft.setRotation(TFT_ROTATION);  // paysage 320x240
  tft.fillScreen(col::BG);

  // Carte SD : tuiles, trace GPX
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  sdOk = SD.begin(SD_CS_PIN, sdSPI, 20000000, "/sd", 8);
  Serial.printf("Carte SD : %s\n", sdOk ? "OK" : "absente");
  if (sdOk && track.load(GPX_PATH)) Serial.printf("Trace GPX : %d points\n", (int)track.count());
  mapLight = cfg.mapTheme == MT_LIGHT;
  updateDestination();

  phoneLinkBegin();  // Wi-Fi + WebSocket (tâche sur le cœur 0), ou annonce Bluetooth LE (variante dashboard-ble)
}

void loop() {
  static uint32_t lastFrame = 0, lastSlow = 0;
  uint32_t now = millis();

  PhoneSample ps;
  phoneLinkGetSample(ps);
  phoneApply(fix, phoneTrk, ps, now);
  if (fix.valid) {  // dernière position connue (la carte s'y centre sans signal)
    lastFixOk = true;
    lastFixLat = fix.lat;
    lastFixLon = fix.lon;
  }
  bool accOk = !phoneTrk.accValid || phoneTrk.acc <= SPEED_ACC_MAX_M;
  speed.update(now, fix, accOk);
  trip.update(now, fix, speed);
  handleButtons(now);

  if (pageDirty) {
    phoneLinkGetInfo(linkInfo);
    enterPage();
    renderSlow();
  }
  if (modalReset) {  // fenêtre de confirmation : on ne redessine pas dessous
    delay(1);
    return;
  }
  if (now - lastFrame >= 40) {  // ~25 images/s
    lastFrame = now;
    renderFrame(now);
  }
  if (now - lastSlow >= 500) {
    lastSlow = now;
    renderSlow();
  }
  delay(1);
}
