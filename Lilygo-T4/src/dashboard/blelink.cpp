// Liaison téléphone en Bluetooth LE : la carte est PÉRIPHÉRIQUE, l'appli Android BLE-Companion se connecte
// et écrit sa position dans la caractéristique Feed (protocole : common/ble/PROTOCOL.md).
// Même interface que phonelink.cpp (Wi-Fi / SensorServer) : le reste du code ne voit qu'un PhoneSample.
// Compilé seulement avec -DPHONE_LINK_BLE=1 (environnement dashboard-ble de platformio.ini).
#include "phonelink.h"

#ifdef PHONE_LINK_BLE
#include <NimBLEDevice.h>
#include "ble_proto.h"
#include "blesample.h"

// --- état partagé avec l'interface (cœur 1) ; les rappels BLE arrivent sur la tâche de NimBLE ---
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

void phoneLinkDisconnect() {
  // Déconnecte le téléphone courant ; onDisconnect() relance l'annonce (utile pour changer de téléphone).
  NimBLEServer *server = NimBLEDevice::getServer();
  if (server && server->getConnectedCount() > 0) {
    std::vector<uint16_t> peers = server->getPeerDevices();
    for (uint16_t h : peers) server->disconnect(h);
  }
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *) override {
    xSemaphoreTake(mtx, portMAX_DELAY);
    sharedInfo.bleConnected = true;
    sharedInfo.bleAdvertising = false;
    xSemaphoreGive(mtx);
  }
  void onDisconnect(NimBLEServer *) override {
    xSemaphoreTake(mtx, portMAX_DELAY);
    sharedInfo.bleConnected = false;
    sharedInfo.bleMtu = 0;
    sharedInfo.bleAdvertising = true;
    xSemaphoreGive(mtx);
    NimBLEDevice::startAdvertising();  // l'annonce s'arrête à la connexion : on la reprend
  }
  void onMTUChange(uint16_t mtu, ble_gap_conn_desc *) override {
    xSemaphoreTake(mtx, portMAX_DELAY);
    sharedInfo.bleMtu = mtu;
    xSemaphoreGive(mtx);
  }
};

class FeedCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {
    auto v = c->getValue();
    PhoneSample s;
    if (!bleFeedToSample((const uint8_t *)v.data(), v.size(), millis(), s)) return;  // type inconnu : ignoré
    xSemaphoreTake(mtx, portMAX_DELAY);
    shared = s;
    sharedInfo.bleMsgs++;
    xSemaphoreGive(mtx);
  }
};

void phoneLinkBegin() {
  mtx = xSemaphoreCreateMutex();

  // Nom visible dans l'appli : BikeCopilot-T4-XXXX (XXXX = fin de l'adresse, pour distinguer plusieurs cartes)
  char name[24];
  snprintf(name, sizeof(name), "BikeCopilot-T4-%04X", (unsigned)((ESP.getEfuseMac() >> 32) & 0xFFFF));
  strncpy(sharedInfo.bleName, name, sizeof(sharedInfo.bleName) - 1);

  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(247);  // le téléphone en demande autant ; minimum exigé par le protocole : 25

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService *svc = server->createService(BLE_UUID_SERVICE);

  // Device Info (lecture) : version du protocole, type de carte, messages acceptés
  uint8_t info[8];
  const uint8_t accepted[] = {BLE_MSG_GPS_FIX};
  size_t n = bleBuildDeviceInfo(info, BLE_BOARD_LILYGO_T4, accepted, sizeof(accepted));
  NimBLECharacteristic *ci = svc->createCharacteristic(BLE_UUID_INFO, NIMBLE_PROPERTY::READ);
  ci->setValue(info, n);

  // Feed (écriture, avec ou sans réponse) : messages du téléphone
  NimBLECharacteristic *cf = svc->createCharacteristic(BLE_UUID_FEED, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cf->setCallbacks(new FeedCallbacks());

  // Event (notification) : réservée pour plus tard, présente pour que les UUID ne changent plus
  svc->createCharacteristic(BLE_UUID_EVENT, NIMBLE_PROPERTY::NOTIFY);

  svc->start();

  // L'UUID du service est dans l'annonce (c'est ce que filtre l'appli) ; le nom va dans la réponse au scan.
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_UUID_SERVICE);
  adv->setScanResponse(true);
  NimBLEDevice::startAdvertising();

  xSemaphoreTake(mtx, portMAX_DELAY);
  sharedInfo.bleAdvertising = true;
  xSemaphoreGive(mtx);
  Serial.printf("[ble] annonce : %s\n", name);
}

#endif  // PHONE_LINK_BLE
