#include "netconfig.h"
#include <Preferences.h>

NetConfig netCfg;
static Preferences prefs;
static const char *NS = "netcfg";
static const char *DEFAULT_AP_PASS = "bikecopilot";

static String defaultApSsid() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[24];
  snprintf(buf, sizeof(buf), "BikeCopilot-%02X%02X",
           (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));
  return String(buf);
}

void netConfigLoad() {
  prefs.begin(NS, false);
  netCfg.netCount = prefs.getUChar("n", 0);
  if (netCfg.netCount > MAX_NETS) netCfg.netCount = MAX_NETS;
  for (uint8_t i = 0; i < netCfg.netCount; i++) {
    netCfg.nets[i].ssid = prefs.getString(("s" + String(i)).c_str(), "");
    netCfg.nets[i].pass = prefs.getString(("p" + String(i)).c_str(), "");
  }
  netCfg.port = prefs.getUShort("port", 8080);
  netCfg.lastHost = prefs.getString("last", "");
  netCfg.apSsid = prefs.getString("apssid", defaultApSsid());
  netCfg.apPass = prefs.getString("appass", DEFAULT_AP_PASS);
  if (netCfg.apPass.length() < 8) netCfg.apPass = DEFAULT_AP_PASS;
  prefs.end();
}

void netConfigSave() {
  prefs.begin(NS, false);
  prefs.putUChar("n", netCfg.netCount);
  for (uint8_t i = 0; i < MAX_NETS; i++) {
    String ks = "s" + String(i), kp = "p" + String(i);
    if (i < netCfg.netCount) {
      prefs.putString(ks.c_str(), netCfg.nets[i].ssid);
      prefs.putString(kp.c_str(), netCfg.nets[i].pass);
    } else {
      prefs.remove(ks.c_str());
      prefs.remove(kp.c_str());
    }
  }
  prefs.putUShort("port", netCfg.port);
  prefs.putString("last", netCfg.lastHost);
  prefs.putString("apssid", netCfg.apSsid);
  prefs.putString("appass", netCfg.apPass);
  prefs.end();
}

void netConfigClear() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}

bool netConfigAddNetwork(const String &ssid, const String &pass) {
  if (ssid.length() == 0 || ssid.length() > 32) return false;
  // retire un éventuel doublon
  int at = -1;
  for (uint8_t i = 0; i < netCfg.netCount; i++)
    if (netCfg.nets[i].ssid == ssid) { at = i; break; }
  if (at >= 0) {
    for (uint8_t i = at; i + 1 < netCfg.netCount; i++) netCfg.nets[i] = netCfg.nets[i + 1];
    netCfg.netCount--;
  }
  // décale tout et insère en tête (le plus récent est essayé en premier)
  if (netCfg.netCount >= MAX_NETS) netCfg.netCount = MAX_NETS - 1;
  for (int i = netCfg.netCount; i > 0; i--) netCfg.nets[i] = netCfg.nets[i - 1];
  netCfg.nets[0].ssid = ssid;
  netCfg.nets[0].pass = pass;
  netCfg.netCount++;
  return true;
}

void netConfigForgetNetworks() {
  netCfg.netCount = 0;
  for (uint8_t i = 0; i < MAX_NETS; i++) { netCfg.nets[i].ssid = ""; netCfg.nets[i].pass = ""; }
  netConfigSave();
}
