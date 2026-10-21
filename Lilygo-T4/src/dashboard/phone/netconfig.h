#pragma once
#include <Arduino.h>

// Configuration persistante (NVS / Preferences). Rien n'est écrit en dur :
// au premier démarrage la liste de réseaux est vide -> mode AP + portail.

#define MAX_NETS 5

struct NetCred {
  String ssid;
  String pass;
};

struct NetConfig {
  NetCred nets[MAX_NETS];   // le plus récent en premier
  uint8_t netCount = 0;
  uint16_t port = 8080;     // port affiché dans l'appli SensorServer
  String lastHost;          // dernier hôte SensorServer utilisé
  String apSsid;            // réseau créé par le module (mode AP)
  String apPass;            // 8 caractères minimum
};

extern NetConfig netCfg;

void netConfigLoad();
void netConfigSave();
void netConfigClear();                                        // efface tout
bool netConfigAddNetwork(const String &ssid, const String &pass);
void netConfigForgetNetworks();
