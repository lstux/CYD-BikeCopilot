#include "portal.h"
#include "netconfig.h"
#include "finder.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <vector>

String gStatusLine;
String gGpsLine;
bool portalPick = false;
IPAddress portalPickIp;
uint16_t portalPickPort = 0;
bool portalRescan = false;

static WebServer server(80);
static DNSServer dns;
static bool apActive = false;
static uint32_t restartAt = 0;
static std::vector<String> scanSsids;

void portalScanWifi() {
  int n = WiFi.scanNetworks(false, true);
  scanSsids.clear();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    bool dup = false;
    for (auto &x : scanSsids) if (x == s) { dup = true; break; }
    if (!dup) scanSsids.push_back(s);
  }
  WiFi.scanDelete();
}

bool portalSsidVisible(const String &ssid) {
  for (auto &x : scanSsids) if (x == ssid) return true;
  return false;
}

static String esc(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': o += F("&amp;"); break;
      case '<': o += F("&lt;"); break;
      case '>': o += F("&gt;"); break;
      case '"': o += F("&quot;"); break;
      case '\'': o += F("&#39;"); break;
      default: o += c;
    }
  }
  return o;
}

static void head(String &h) {
  h += F("<!doctype html><html><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>BikeCopilot</title><style>"
         "body{font-family:sans-serif;margin:1em;max-width:32em}"
         "input,button{width:100%;padding:.6em;margin:.3em 0;box-sizing:border-box;font-size:1em}"
         "pre{background:#eee;padding:.5em;white-space:pre-wrap}"
         "a.b{display:block;padding:.6em;background:#ddd;margin:.3em 0;text-align:center;"
         "text-decoration:none;color:#000}"
         "</style></head><body>");
}

static void redirectHome() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

static void handleRoot() {
  String h;
  h.reserve(5000);
  head(h);
  h += F("<h2>BikeCopilot</h2><pre>");
  h += esc(gStatusLine);
  h += "\n";
  h += esc(gGpsLine);
  h += F("</pre>");

  // hôtes SensorServer trouvés
  if (foundCount) {
    h += F("<h3>SensorServer trouv&eacute;</h3>");
    for (uint8_t i = 0; i < foundCount; i++) {
      h += F("<a class=b href='/pick?i=");
      h += i;
      h += F("'>");
      h += foundHosts[i].ip.toString();
      h += ':';
      h += foundHosts[i].port;
      h += F(" (");
      h += esc(foundHosts[i].how);
      h += F(")</a>");
    }
  }

  // formulaire
  h += F("<h3>Configuration</h3><form method=post action=/save>"
         "<label>R&eacute;seau Wi-Fi (SSID)</label>"
         "<input name=ssid list=nets autocomplete=off>"
         "<datalist id=nets>");
  for (auto &s : scanSsids) {
    h += F("<option value=\"");
    h += esc(s);
    h += F("\">");
  }
  h += F("</datalist><label>Mot de passe</label><input name=pass type=password>"
         "<label>Port SensorServer</label><input name=port type=number min=1 max=65535 value=");
  h += netCfg.port;
  h += F("><label>Nom du r&eacute;seau du module (mode AP)</label><input name=apssid value=\"");
  h += esc(netCfg.apSsid);
  h += F("\"><label>Mot de passe AP (8 car. min, vide = inchang&eacute;)</label>"
         "<input name=appass type=password>"
         "<button>Enregistrer et red&eacute;marrer</button></form>");

  // réseaux mémorisés
  h += F("<h3>R&eacute;seaux m&eacute;moris&eacute;s</h3>");
  if (netCfg.netCount == 0) h += F("<p>Aucun.</p>");
  else {
    h += F("<ul>");
    for (uint8_t i = 0; i < netCfg.netCount; i++) {
      h += F("<li>");
      h += esc(netCfg.nets[i].ssid);
      h += F("</li>");
    }
    h += F("</ul><form method=post action=/forget><button>Oublier tous les r&eacute;seaux</button></form>");
  }

  h += F("<h3>Actions</h3>"
         "<a class=b href=/scan>Rescanner les r&eacute;seaux Wi-Fi</a>"
         "<a class=b href=/rescan>Relancer la recherche de SensorServer</a>"
         "<a class=b href=/reboot>Red&eacute;marrer</a>"
         "</body></html>");
  server.send(200, "text/html", h);
}

static void handleSave() {
  String ssid = server.arg("ssid");
  ssid.trim();
  String pass = server.arg("pass");
  int port = server.arg("port").toInt();
  String apssid = server.arg("apssid");
  apssid.trim();
  String appass = server.arg("appass");

  String note;
  if (ssid.length()) netConfigAddNetwork(ssid, pass);
  if (port > 0 && port < 65536) netCfg.port = (uint16_t)port;
  if (apssid.length() && apssid.length() <= 32) netCfg.apSsid = apssid;
  if (appass.length()) {
    if (appass.length() >= 8 && appass.length() <= 63) netCfg.apPass = appass;
    else note = F("<p>Mot de passe AP ignor&eacute; (8 &agrave; 63 caract&egrave;res).</p>");
  }
  netConfigSave();

  String h;
  head(h);
  h += F("<h2>Enregistr&eacute;</h2><p>Le module red&eacute;marre...</p>");
  h += note;
  h += F("</body></html>");
  server.send(200, "text/html", h);
  restartAt = millis() + 2000;
}

static void handleForget() {
  netConfigForgetNetworks();
  redirectHome();
}

static void handlePick() {
  int i = server.arg("i").toInt();
  if (i >= 0 && i < foundCount) {
    portalPickIp = foundHosts[i].ip;
    portalPickPort = foundHosts[i].port;
    portalPick = true;
    netCfg.lastHost = portalPickIp.toString();
    netConfigSave();
  }
  redirectHome();
}

static void handleScan() {
  portalScanWifi();
  redirectHome();
}

static void handleRescan() {
  portalRescan = true;
  redirectHome();
}

static void handleReboot() {
  server.send(200, "text/plain", "Redemarrage...");
  restartAt = millis() + 1000;
}

static void handleNotFound() {
  if (apActive) {
    // portail captif : tout est renvoyé vers la page de config
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

void portalBegin(bool apMode) {
  apActive = apMode;
  if (apMode) dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/forget", HTTP_POST, handleForget);
  server.on("/pick", HTTP_GET, handlePick);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/rescan", HTTP_GET, handleRescan);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.onNotFound(handleNotFound);
  server.begin();
}

void portalLoop() {
  if (apActive) dns.processNextRequest();
  server.handleClient();
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) {
    delay(100);
    ESP.restart();
  }
}
