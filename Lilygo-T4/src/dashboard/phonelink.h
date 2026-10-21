// Liaison avec le téléphone, en Wi-Fi (phonelink.cpp) ou en Bluetooth LE (blelink.cpp, -DPHONE_LINK_BLE=1).
// Version Wi-Fi : Wi-Fi (réseau mémorisé ou point d'accès + portail), recherche de
// l'appli Android SensorServer, WebSocket /gps. Tout tourne dans une tâche FreeRTOS sur le
// cœur 0 : les recherches réseau (plusieurs secondes) ne figent pas l'affichage.
// Le reste du code ne voit qu'un PhoneSample (voir phonefix.h) et un résumé d'état.
#pragma once
#include <Arduino.h>
#include "phonefix.h"

struct PhoneLinkInfo {
  uint8_t mode = 0;  // 0 démarrage, 1 station (Wi-Fi existant), 2 point d'accès (portail)
  bool wsStarted = false;
  bool wsConnected = false;
  char ssid[34] = "";  // réseau rejoint (mode 1) ou réseau créé par la carte (mode 2)
  char apPass[34] = "";
  char ip[16] = "";  // IP de la carte
  char host[28] = "";  // « ip:port » de SensorServer
  uint16_t port = 8080;
  uint8_t apClients = 0;
  // --- variante Bluetooth LE (blelink.cpp, environnement dashboard-ble) ---
  bool bleAdvertising = false;  // la carte s'annonce, en attente d'un téléphone
  bool bleConnected = false;    // un téléphone est connecté
  uint16_t bleMtu = 0;          // taille de paquet négociée (0 = pas encore)
  uint32_t bleMsgs = 0;         // positions reçues depuis le démarrage
  char bleName[24] = "";        // nom de la carte dans l'appli, ex. BikeCopilot-T4-A1B2
};

void phoneLinkBegin();                       // crée la tâche (à appeler une fois, dans setup)
bool phoneLinkGetSample(PhoneSample &out);   // copie du dernier point ; false si aucun reçu
void phoneLinkGetInfo(PhoneLinkInfo &out);   // copie de l'état de la liaison
#ifdef PHONE_LINK_BLE
void phoneLinkDisconnect();                  // BLE : déconnecte le téléphone (et reprend l'annonce)
#else
void phoneLinkForgetWifi();                  // efface la configuration Wi-Fi et redémarre
#endif
