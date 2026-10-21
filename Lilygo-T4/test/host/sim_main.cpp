// Simulateur PC de la T4 : compile le vrai main.cpp avec de fausses bibliothèques (voir shim/),
// joue un scénario (pas de liaison, portail Wi-Fi, trajet à vitesse variable, confirmation de
// remise à zéro) et écrit des captures d'écran. Voir run_sim.sh.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "phonelink.h"

uint32_t simMillis = 0;
int simPin[64];
SerialShim Serial;

// --- fausse liaison téléphone (remplace phonelink.cpp) ---
static PhoneSample fakeSample;
static PhoneLinkInfo fakeInfo;
bool phoneLinkGetSample(PhoneSample &out) {
  out = fakeSample;
  return out.have;
}
void phoneLinkGetInfo(PhoneLinkInfo &out) { out = fakeInfo; }
void phoneLinkBegin() {}
void phoneLinkForgetWifi() { printf("[sim] oubli du Wi-Fi demande\n"); }

// main.cpp (le vrai)
extern TFT_eSPI tft;
void setup();
void loop();

static const char *outDir = ".";

static void shot(const char *name) {
  char p[256];
  snprintf(p, sizeof(p), "%s/%s.ppm", outDir, name);
  tft.writePPM(p);
}

// Fait tourner la boucle pendant ms millisecondes simulées
static void run(uint32_t ms) {
  uint32_t end = simMillis + ms;
  while (simMillis < end) {
    uint32_t before = simMillis;
    loop();
    if (simMillis == before) simMillis++;  // le vrai loop() tourne en continu : le temps avance
  }
}

static void press(int pin, uint32_t holdMs) {
  simPin[pin] = LOW;
  run(holdMs);
  simPin[pin] = HIGH;
  run(60);
}

// Un point par seconde, vitesse en km/h, cap nord, +1 m d'altitude par point
static double simLat = 47.0;
static uint64_t simEpoch = 1760000000000ULL;
static float simAlt = 100;
static void feed(float kmh, uint32_t ms) {
  uint32_t t0 = simMillis;
  while (simMillis - t0 < ms) {
    simEpoch += 1000;
    simLat += (kmh / 3.6) / 111195.0;
    simAlt += 0.4f;
    fakeSample.have = true;
    fakeSample.lat = simLat;
    fakeSample.lon = -0.5;
    fakeSample.timeMs = simEpoch;
    fakeSample.speedValid = true;
    fakeSample.speedMs = kmh / 3.6f;
    fakeSample.bearingValid = true;
    fakeSample.bearing = 0;
    fakeSample.altValid = true;
    fakeSample.alt = simAlt;
    fakeSample.accValid = true;
    fakeSample.acc = 7;
    run(1000);
  }
}

int main(int argc, char **argv) {
  if (argc > 1) outDir = argv[1];
  for (int i = 0; i < 64; i++) simPin[i] = HIGH;
  simMillis = 5000;
  setup();

  // 1. démarrage : pas de liaison
  fakeInfo.mode = 0;
  run(800);
  shot("t4_1_demarrage");

  // 2. portail Wi-Fi (aucun réseau mémorisé), pas encore de client
  fakeInfo.mode = 2;
  strcpy(fakeInfo.ssid, "BikeCopilot-A1B2");
  strcpy(fakeInfo.apPass, "bikecopilot");
  strcpy(fakeInfo.ip, "192.168.4.1");
  fakeInfo.apClients = 0;
  run(600);
  shot("t4_2_barre_ap");
  press(38, 200);  // BTN2 : page suivante (carte)
  run(600);
  shot("t4_3_carte_sans_sd");
  press(38, 1200);  // BTN2 maintenu sur la carte : page suivante (liaison)
  run(600);
  shot("t4_3_liaison_ap");

  // 3. réseau rejoint, SensorServer connecté, premiers points
  fakeInfo.mode = 1;
  strcpy(fakeInfo.ssid, "MonWifi");
  strcpy(fakeInfo.ip, "192.168.1.42");
  fakeInfo.wsStarted = fakeInfo.wsConnected = true;
  strcpy(fakeInfo.host, "192.168.1.20:8080");
  feed(0, 3000);
  run(600);
  shot("t4_4_liaison_ok");
  press(37, 200);  // BTN1 : page précédente (carte)
  run(300);
  press(37, 1200);  // BTN1 maintenu sur la carte : page précédente (jauge)
  run(300);

  // 4. trajet : accélération puis croisière à 24 km/h, avec une pointe
  feed(0, 3000);
  feed(8, 3000);
  feed(15, 5000);
  feed(24, 20000);
  feed(27, 4000);
  feed(24, 20000);
  run(800);
  shot("t4_5_jauge_24kmh");

  // 5. confirmation de remise à zéro : BTN3 maintenu 1,2 s
  simPin[39] = LOW;
  run(1200);
  simPin[39] = HIGH;
  run(100);
  shot("t4_6_confirmation");
  press(37, 200);  // annuler
  run(300);
  shot("t4_7_annule");

  // 6. à l'arrêt : le point devient périmé -> état de liaison dans la barre
  fakeInfo.wsConnected = false;
  run(8000);
  shot("t4_8_perdu");

  printf("simulation terminee (%u ms simules)\n", simMillis);
  return 0;
}
