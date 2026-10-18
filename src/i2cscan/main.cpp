// Test 1 : scan I2C
// Essaie d'abord SDA=27 / SCL=22, puis l'inverse, pour vérifier le câblage.
#include <Arduino.h>
#include <Wire.h>

static int scanBus(int sda, int scl) {
  static bool started = false;
  if (started) Wire.end();
  started = true;
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  Serial.printf("\n--- Scan SDA=IO%d SCL=IO%d ---\n", sda, scl);
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      const char *hint = "inconnu";
      if (addr == 0x0D) hint = "QMC5883L";
      else if (addr == 0x2C) hint = "QMC5883P";
      else if (addr == 0x1E) hint = "HMC5883L";
      Serial.printf("  0x%02X  (%s)\n", addr, hint);
      found++;
    }
  }
  if (!found) Serial.println("  rien trouvé");
  return found;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Scan I2C - CYD");
}

void loop() {
  int n = scanBus(27, 22);
  if (n == 0) {
    n = scanBus(22, 27);
    if (n > 0) Serial.println(">> SDA et SCL semblent inversés !");
  }
  delay(3000);
}
