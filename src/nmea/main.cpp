// Test 2 : trames NMEA brutes sur IO35 (RX seul)
// Si rien n'arrive pendant 5 s, on essaie d'autres vitesses.
#include <Arduino.h>

static const int GPS_RX = 35;
static const uint32_t BAUDS[] = {9600, 38400, 115200, 4800};
static const int NB_BAUDS = sizeof(BAUDS) / sizeof(BAUDS[0]);

static int baudIdx = 0;
static uint32_t lastByteMs = 0;
static uint32_t startMs = 0;

static void startGps() {
  Serial2.end();
  Serial2.begin(BAUDS[baudIdx], SERIAL_8N1, GPS_RX, -1);
  startMs = millis();
  lastByteMs = 0;
  Serial.printf("\n=== GPS RX=IO%d @ %lu bauds ===\n", GPS_RX,
                (unsigned long)BAUDS[baudIdx]);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  startGps();
}

void loop() {
  while (Serial2.available()) {
    Serial.write(Serial2.read());
    lastByteMs = millis();
  }
  // Rien reçu depuis 5 s : on change de vitesse
  if (lastByteMs == 0 && millis() - startMs > 5000) {
    Serial.println("\n(aucune donnée, vitesse suivante)");
    baudIdx = (baudIdx + 1) % NB_BAUDS;
    startGps();
  }
}
