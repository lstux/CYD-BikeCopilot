#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// Portail de configuration (serveur web, + DNS captif en mode AP).

extern String gStatusLine;   // renseigné par main, affiché sur la page
extern String gGpsLine;

// Actions demandées depuis la page web, consommées par main
extern bool portalPick;           // l'utilisateur a choisi un hôte
extern IPAddress portalPickIp;
extern uint16_t portalPickPort;
extern bool portalRescan;         // relancer la recherche de SensorServer

void portalScanWifi();                         // scan Wi-Fi (bloquant, ~2-3 s)
bool portalSsidVisible(const String &ssid);    // d'après le dernier scan
void portalBegin(bool apMode);
void portalLoop();
