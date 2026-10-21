// Simulateur PC de la variante Bluetooth LE de la T4 : compile le vrai main.cpp avec -DPHONE_LINK_BLE=1 et de fausses
// bibliothèques (voir shim/), joue : attente du téléphone -> connecté -> positions reçues -> perte, et écrit des
// captures d'écran. Voir run_sim_ble.sh.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "phonelink.h"

uint32_t simMillis = 0;
int simPin[64];
SerialShim Serial;

// --- fausse liaison BLE (remplace blelink.cpp) ---
static PhoneSample fakeSample;
static PhoneLinkInfo fakeInfo;
bool phoneLinkGetSample(PhoneSample &out) {
  out = fakeSample;
  return out.have;
}
void phoneLinkGetInfo(PhoneLinkInfo &out) { out = fakeInfo; }
void phoneLinkBegin() {}
void phoneLinkDisconnect() { printf("[sim] deconnexion du telephone demandee\n"); }

extern TFT_eSPI tft;
void setup();
void loop();

static const char *outDir = ".";

static void shot(const char *name) {
  char p[256];
  snprintf(p, sizeof(p), "%s/%s.ppm", outDir, name);
  tft.writePPM(p);
}

static void run(uint32_t ms) {
  uint32_t end = simMillis + ms;
  while (simMillis < end) {
    uint32_t before = simMillis;
    loop();
    if (simMillis == before) simMillis++;
  }
}

static void press(int pin, uint32_t holdMs) {
  simPin[pin] = LOW;
  run(holdMs);
  simPin[pin] = HIGH;
  run(60);
}

static double simLat = 47.0;
static uint64_t simEpoch = 1760000000000ULL;
static void feed(float kmh, uint32_t ms) {
  uint32_t t0 = simMillis;
  while (simMillis - t0 < ms) {
    simEpoch += 1000;
    simLat += (kmh / 3.6) / 111195.0;
    fakeSample.have = true;
    fakeSample.lat = simLat;
    fakeSample.lon = -0.5;
    fakeSample.timeMs = simEpoch;
    fakeSample.speedValid = true;
    fakeSample.speedMs = kmh / 3.6f;
    fakeSample.bearingValid = true;
    fakeSample.bearing = 0;
    fakeSample.altValid = true;
    fakeSample.alt = 120;
    fakeSample.accValid = true;
    fakeSample.acc = 6;
    fakeInfo.bleMsgs++;
    run(1000);
  }
}

int main(int argc, char **argv) {
  if (argc > 1) outDir = argv[1];
  for (int i = 0; i < 64; i++) simPin[i] = HIGH;
  simMillis = 5000;
  setup();
  strcpy(fakeInfo.bleName, "BikeCopilot-T4-A1B2");

  // 1. démarrage : l'annonce n'a pas commencé
  run(800);
  shot("ble_1_demarrage");

  // 2. la carte s'annonce, aucun téléphone
  fakeInfo.bleAdvertising = true;
  run(600);
  press(38, 200);  // BTN2 : page Liaison
  run(600);
  shot("ble_2_attente");

  // 3. téléphone connecté, positions reçues
  fakeInfo.bleAdvertising = false;
  fakeInfo.bleConnected = true;
  fakeInfo.bleMtu = 247;
  feed(0, 3000);
  run(600);
  shot("ble_3_connecte");

  // 4. retour à la jauge, croisière
  press(37, 200);
  feed(15, 5000);
  feed(24, 8000);
  run(400);
  shot("ble_4_jauge");

  // 5. le téléphone s'éloigne : plus de positions, le point devient périmé
  run(7000);
  shot("ble_5_perdu");
  return 0;
}
