// Voir phonelink.h. Repris du firmware phone-bridge (dépôt Waveshare-C6-1.47), adapté :
//  - tâche FreeRTOS au lieu de setup()/loop() ;
//  - le point reçu est publié dans un PhoneSample protégé par un mutex ;
//  - pas de bouton ici (géré par l'interface).
#include "phonelink.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "phone/netconfig.h"
#include "phone/portal.h"
#include "phone/finder.h"

#define STA_TRY_MS 12000           // délai de connexion par réseau mémorisé
#define STA_LOST_RESTART_MS 20000  // Wi-Fi perdu trop longtemps -> redémarrage
#define AP_IDLE_RETRY_MS 120000    // AP sans client -> on retente les réseaux connus
#define WS_LOST_RESEARCH_MS 20000  // WebSocket perdu -> on relance la recherche
#define GPS_PATH "/gps"

enum Mode { MODE_STA, MODE_AP };
static Mode mode = MODE_AP;

static WebSocketsClient ws;
static bool wsStarted = false;
static bool wsConnected = false;
static IPAddress hostIp;
static uint16_t hostPort = 0;

static uint32_t lastSearchMs = 0;
static uint32_t lastPollMs = 0;
static uint32_t lastStatusMs = 0;
static uint32_t lastPrintMs = 0;
static uint32_t wsLostMs = 0;
static uint32_t staLostMs = 0;
static uint32_t apIdleSince = 0;

// --- état partagé avec l'interface (cœur 1) ---
static SemaphoreHandle_t mtx = nullptr;
static PhoneSample shared;
static PhoneLinkInfo sharedInfo;

bool phoneLinkGetSample(PhoneSample &out) {
  if (!mtx) return false;
  xSemaphoreTake(mtx, portMAX_DELAY);
  out = shared;
  xSemaphoreGive(mtx);
  return out.have;
}

void phoneLinkGetInfo(PhoneLinkInfo &out) {
  if (!mtx) return;
  xSemaphoreTake(mtx, portMAX_DELAY);
  out = sharedInfo;
  xSemaphoreGive(mtx);
}

void phoneLinkForgetWifi() {
  netConfigClear();
  delay(200);
  ESP.restart();
}

// ---------------------------------------------------------------- Wi-Fi

static bool connectKnownNetworks() {
  if (netCfg.netCount == 0) return false;
  for (uint8_t i = 0; i < netCfg.netCount; i++) {
    const NetCred &n = netCfg.nets[i];
    // Réseau absent du scan : on ne perd pas 12 s dessus (SSID masqués non gérés).
    if (!portalSsidVisible(n.ssid)) {
      Serial.printf("[wifi] '%s' non visible, ignore\n", n.ssid.c_str());
      continue;
    }
    Serial.printf("[wifi] connexion a '%s'...\n", n.ssid.c_str());
    WiFi.begin(n.ssid.c_str(), n.pass.c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < STA_TRY_MS) delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[wifi] connecte, IP %s, passerelle %s\n", WiFi.localIP().toString().c_str(),
                    WiFi.gatewayIP().toString().c_str());
      return true;
    }
    WiFi.disconnect();
  }
  return false;
}

static void startAp() {
  mode = MODE_AP;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);  // STA reste disponible pour les scans
  WiFi.softAP(netCfg.apSsid.c_str(), netCfg.apPass.c_str());
  Serial.printf("[ap] reseau '%s' (mot de passe: %s), portail http://%s/\n", netCfg.apSsid.c_str(),
                netCfg.apPass.c_str(), WiFi.softAPIP().toString().c_str());
  apIdleSince = millis();
}

// ---------------------------------------------------------------- GPS / WebSocket

static void handleGps(uint8_t *payload, size_t len) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) {
    Serial.printf("[gps] JSON invalide: %s\n", err.c_str());
    return;
  }
  if (doc["latitude"].isNull() || doc["longitude"].isNull()) return;

  PhoneSample s;
  s.have = true;
  s.lat = doc["latitude"] | 0.0;
  s.lon = doc["longitude"] | 0.0;
  s.altValid = !doc["altitude"].isNull();
  s.alt = doc["altitude"] | 0.0f;
  s.bearingValid = !doc["bearing"].isNull();
  s.bearing = doc["bearing"] | 0.0f;
  s.accValid = !doc["accuracy"].isNull();
  s.acc = doc["accuracy"] | 0.0f;
  s.speedValid = !doc["speed"].isNull();
  s.speedMs = doc["speed"] | 0.0f;
  s.timeMs = doc["time"] | (uint64_t)0;

  xSemaphoreTake(mtx, portMAX_DELAY);
  shared = s;
  xSemaphoreGive(mtx);

  char line[160];
  snprintf(line, sizeof(line), "lat %.6f  lon %.6f  alt %.0f m  vit %.1f km/h  cap %.0f  prec %.0f m", s.lat, s.lon,
           s.alt, s.speedMs * 3.6f, s.bearing, s.acc);
  gGpsLine = line;
  if (millis() - lastPrintMs >= 5000) {  // une ligne de debug toutes les 5 s
    lastPrintMs = millis();
    Serial.printf("[gps] %s\n", line);
  }
}

static void wsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.printf("[ws] connecte a %s:%u%s\n", hostIp.toString().c_str(), hostPort, GPS_PATH);
      break;
    case WStype_DISCONNECTED:
      if (wsConnected) Serial.println("[ws] deconnecte");
      wsConnected = false;
      if (wsLostMs == 0) wsLostMs = millis();
      break;
    case WStype_TEXT:
      wsLostMs = 0;
      handleGps(payload, length);
      break;
    case WStype_ERROR:
      Serial.println("[ws] erreur");
      break;
    default:
      break;
  }
}

static void startWs(IPAddress ip, uint16_t port) {
  hostIp = ip;
  hostPort = port;
  String host = ip.toString();
  ws.begin(host.c_str(), port, GPS_PATH);
  ws.onEvent(wsEvent);
  ws.setReconnectInterval(2000);
  ws.enableHeartbeat(15000, 3000, 2);
  wsStarted = true;
  wsConnected = false;
  wsLostMs = millis();
  Serial.printf("[ws] connexion a ws://%s:%u%s\n", host.c_str(), port, GPS_PATH);
}

static void stopWs() {
  if (wsStarted) ws.disconnect();
  wsStarted = false;
  wsConnected = false;
  wsLostMs = 0;
}

// ---------------------------------------------------------------- état

static void updateStatus() {
  String s;
  if (mode == MODE_STA) s = "Mode STA : " + WiFi.SSID() + "  IP " + WiFi.localIP().toString();
  else
    s = "Mode AP : " + netCfg.apSsid + "  IP " + WiFi.softAPIP().toString() + "  clients " +
        String(WiFi.softAPgetStationNum());
  s += "\nSensorServer : ";
  if (wsConnected) s += hostIp.toString() + ":" + String(hostPort) + " connecte";
  else if (wsStarted) s += "connexion a " + hostIp.toString() + ":" + String(hostPort) + "...";
  else s += "recherche (port " + String(netCfg.port) + ")";
  gStatusLine = s;

  PhoneLinkInfo i;
  i.mode = mode == MODE_STA ? 1 : 2;
  i.wsStarted = wsStarted;
  i.wsConnected = wsConnected;
  i.port = netCfg.port;
  if (mode == MODE_STA) {
    strlcpy(i.ssid, WiFi.SSID().c_str(), sizeof(i.ssid));
    strlcpy(i.ip, WiFi.localIP().toString().c_str(), sizeof(i.ip));
  } else {
    strlcpy(i.ssid, netCfg.apSsid.c_str(), sizeof(i.ssid));
    strlcpy(i.apPass, netCfg.apPass.c_str(), sizeof(i.apPass));
    strlcpy(i.ip, WiFi.softAPIP().toString().c_str(), sizeof(i.ip));
    i.apClients = WiFi.softAPgetStationNum();
  }
  if (wsStarted) snprintf(i.host, sizeof(i.host), "%s:%u", hostIp.toString().c_str(), hostPort);
  xSemaphoreTake(mtx, portMAX_DELAY);
  sharedInfo = i;
  xSemaphoreGive(mtx);
}

static void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') {
      portalRescan = true;
      Serial.println("[cmd] nouvelle recherche");
    } else if (c == 's') {
      updateStatus();
      Serial.println(gStatusLine);
      Serial.println(gGpsLine);
    } else if (c == 'x') {
      Serial.println("[cmd] conf Wi-Fi effacee");
      phoneLinkForgetWifi();
    } else if (c == 'h' || c == '?') {
      Serial.println("r=rechercher SensorServer  s=statut  x=effacer conf Wi-Fi + reboot");
    }
  }
}

// ---------------------------------------------------------------- tâche

static void linkSetup() {
  netConfigLoad();
  Serial.printf("[cfg] %u reseau(x) memorise(s), port %u, dernier hote '%s'\n", netCfg.netCount, netCfg.port,
                netCfg.lastHost.c_str());

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("bikecopilot");
  portalScanWifi();  // sert à la fois aux tests de visibilité et à la liste du portail

  if (connectKnownNetworks()) {
    mode = MODE_STA;
    staLostMs = 0;
  } else {
    startAp();
  }
  portalBegin(mode == MODE_AP);
  if (mode == MODE_STA) Serial.printf("[portal] http://%s/\n", WiFi.localIP().toString().c_str());
  updateStatus();
}

static void linkLoop() {
  portalLoop();
  handleSerial();

  // surveillance de la liaison Wi-Fi
  if (mode == MODE_STA) {
    if (WiFi.status() != WL_CONNECTED) {
      if (staLostMs == 0) staLostMs = millis();
      else if (millis() - staLostMs > STA_LOST_RESTART_MS) {
        Serial.println("[wifi] liaison perdue, redemarrage");
        ESP.restart();
      }
    } else {
      staLostMs = 0;
    }
  } else if (netCfg.netCount > 0) {
    if (WiFi.softAPgetStationNum() > 0) apIdleSince = millis();
    else if (millis() - apIdleSince > AP_IDLE_RETRY_MS) {
      Serial.println("[ap] personne connecte, on retente les reseaux memorises");
      ESP.restart();
    }
  }

  // actions venant du portail
  if (portalPick) {
    portalPick = false;
    stopWs();
    startWs(portalPickIp, portalPickPort);
  }
  if (portalRescan) {
    portalRescan = false;
    stopWs();
    lastSearchMs = 0;
  }

  // WebSocket perdu depuis trop longtemps : on cherche à nouveau (l'IP du téléphone a pu changer)
  if (wsStarted && !wsConnected && wsLostMs && millis() - wsLostMs > WS_LOST_RESEARCH_MS) {
    Serial.println("[ws] injoignable, nouvelle recherche");
    stopWs();
    lastSearchMs = 0;
  }

  // recherche de SensorServer
  if (!wsStarted) {
    bool canSearch = (mode == MODE_STA) || WiFi.softAPgetStationNum() > 0;
    if (canSearch && (lastSearchMs == 0 || millis() - lastSearchMs > 5000)) {
      Serial.println("[find] recherche de SensorServer...");
      updateStatus();
      IPAddress ip;
      uint16_t port;
      bool ok = findSensorServer(ip, port, mode == MODE_AP, portalLoop);
      lastSearchMs = millis();
      if (ok) startWs(ip, port);
      else Serial.println("[find] rien trouve (appli lancee ? serveur demarre ? bon port ?)");
    }
  }

  if (wsStarted) {
    ws.loop();
    // SensorServer n'envoie une position que lorsqu'elle change : on demande la dernière connue
    if (wsConnected && millis() - lastPollMs >= 1000) {
      lastPollMs = millis();
      ws.sendTXT("getLastKnownLocation");
    }
  }

  if (millis() - lastStatusMs >= 1000) {
    lastStatusMs = millis();
    updateStatus();
  }
  delay(2);
}

static void linkTask(void *) {
  linkSetup();
  for (;;) linkLoop();
}

void phoneLinkBegin() {
  mtx = xSemaphoreCreateMutex();
  // Cœur 0 (avec la pile Wi-Fi) ; l'interface tourne sur le cœur 1 (loop d'Arduino).
  xTaskCreatePinnedToCore(linkTask, "phoneLink", 12288, nullptr, 1, nullptr, 0);
}
