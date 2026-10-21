#include "finder.h"
#include "netconfig.h"
#include <WiFi.h>
#include <ESPmDNS.h>

FoundHost foundHosts[MAX_FOUND];
uint8_t foundCount = 0;

static const uint32_t PROBE_TIMEOUT_MS = 150;

static bool probe(IPAddress ip, uint16_t port) {
  WiFiClient c;
  bool ok = c.connect(ip, port, PROBE_TIMEOUT_MS);
  c.stop();
  return ok;
}

static void addFound(IPAddress ip, uint16_t port, const char *how) {
  for (uint8_t i = 0; i < foundCount; i++)
    if (foundHosts[i].ip == ip && foundHosts[i].port == port) return;
  if (foundCount >= MAX_FOUND) return;
  foundHosts[foundCount].ip = ip;
  foundHosts[foundCount].port = port;
  foundHosts[foundCount].how = how;
  foundCount++;
}

static bool choose(IPAddress &outIp, uint16_t &outPort) {
  if (foundCount == 0) return false;
  uint8_t pick = 0;
  IPAddress last;
  if (netCfg.lastHost.length() && last.fromString(netCfg.lastHost)) {
    for (uint8_t i = 0; i < foundCount; i++)
      if (foundHosts[i].ip == last) { pick = i; break; }
  }
  outIp = foundHosts[pick].ip;
  outPort = foundHosts[pick].port;
  String s = outIp.toString();
  if (s != netCfg.lastHost) { netCfg.lastHost = s; netConfigSave(); }
  Serial.printf("[find] %u hote(s), choix: %s:%u (%s)\n", foundCount, s.c_str(),
                outPort, foundHosts[pick].how.c_str());
  return true;
}

bool findSensorServer(IPAddress &outIp, uint16_t &outPort, bool apMode, TickFn tick) {
  foundCount = 0;
  const uint16_t port = netCfg.port;

  if (apMode) {
    // Le téléphone reçoit une adresse du DHCP du module : 192.168.4.2, .3, ...
    if (WiFi.softAPgetStationNum() == 0) return false;
    IPAddress base = WiFi.softAPIP();
    for (uint8_t last = 2; last <= 20; last++) {
      IPAddress ip(base[0], base[1], base[2], last);
      if (probe(ip, port)) addFound(ip, port, "ap");
      if (tick) tick();
    }
    return choose(outIp, outPort);
  }

  // --- mode station ---
  // 1) dernier hôte connu
  IPAddress last;
  if (netCfg.lastHost.length() && last.fromString(netCfg.lastHost)) {
    if (probe(last, port)) addFound(last, port, "dernier");
    if (tick) tick();
  }
  // 2) passerelle (cas du partage de connexion du téléphone)
  IPAddress gw = WiFi.gatewayIP();
  if (gw != IPAddress(0, 0, 0, 0) && !(gw == last)) {
    if (probe(gw, port)) addFound(gw, port, "passerelle");
    if (tick) tick();
  }
  if (foundCount) return choose(outIp, outPort);

  // 3) mDNS : SensorServer annonce _websocket._tcp si l'option est activée dans l'appli
  static bool mdnsStarted = false;
  if (!mdnsStarted) mdnsStarted = MDNS.begin("bikecopilot");
  if (mdnsStarted) {
    int n = MDNS.queryService("websocket", "tcp");
    for (int i = 0; i < n; i++) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      addFound(MDNS.address(i), MDNS.port(i), "mdns");   // core 3.x
#else
      addFound(MDNS.IP(i), MDNS.port(i), "mdns");        // core 2.0.x (celui de ce projet)
#endif
    }
    if (tick) tick();
  }
  if (foundCount) return choose(outIp, outPort);

  // 4) balayage du /24 local sur le port configuré
  IPAddress me = WiFi.localIP();
  for (int h = 1; h <= 254; h++) {
    if (h == me[3]) continue;
    IPAddress ip(me[0], me[1], me[2], (uint8_t)h);
    if (probe(ip, port)) addFound(ip, port, "scan");
    if (tick) tick();
    if (foundCount >= MAX_FOUND) break;
  }
  return choose(outIp, outPort);
}
