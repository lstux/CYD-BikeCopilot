#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// Recherche du serveur SensorServer (appli Android) sur le réseau.

#define MAX_FOUND 8

struct FoundHost {
  IPAddress ip;
  uint16_t port;
  String how;   // "dernier", "passerelle", "mdns", "scan"
};

extern FoundHost foundHosts[MAX_FOUND];
extern uint8_t foundCount;

typedef void (*TickFn)();

// apMode=true  : le téléphone est client de notre AP (192.168.4.2 et suivants)
// apMode=false : station, on cherche sur le réseau local.
// `tick` est appelé régulièrement pendant la recherche (garde le portail réactif).
// Retourne true si un hôte est choisi.
bool findSensorServer(IPAddress &outIp, uint16_t &outPort, bool apMode, TickFn tick);
