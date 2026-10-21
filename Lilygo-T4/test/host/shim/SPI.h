// Simulateur PC : bus SPI factice (voir ../run_sim.sh).
#pragma once
#include <stdint.h>
#define VSPI 3
#define HSPI 2
struct SPIClass {
  explicit SPIClass(int) {}
  void begin(int, int, int, int) {}
};
