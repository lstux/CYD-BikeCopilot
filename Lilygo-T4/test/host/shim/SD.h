// Simulateur PC : pas de carte SD (SD.begin échoue), donc la page carte affiche « Carte SD absente ».
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "SPI.h"
#define FILE_READ 0
struct File {
  bool seek(uint32_t) { return false; }
  size_t read(uint8_t *, size_t) { return 0; }
  size_t read() { return 0; }
  void close() {}
  uint32_t size() const { return 0; }
  bool available() const { return false; }
  int read(void) const { return -1; }
  bool isDirectory() const { return false; }
  File openNextFile() { return File(); }
  const char *name() const { return ""; }
  explicit operator bool() const { return false; }
};
struct SDClass {
  bool begin(uint8_t, SPIClass &, uint32_t, const char *, uint8_t) { return false; }
  bool exists(const char *) { return false; }
  File open(const char *, uint8_t = 0) { return File(); }
};
static SDClass SD;
