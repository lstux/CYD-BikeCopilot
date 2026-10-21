// Position courante, indépendante de la source (GPS série sur la CYD, téléphone sur la T4…).
// Chaque carte remplit une `Fix` ; tout le reste du code ne lit que celle-ci.
#pragma once
#include <stdint.h>

struct Fix {
  bool valid = false;  // position connue ET récente
  double lat = 0, lon = 0;
  uint32_t seq = 0;  // incrémenté à chaque nouvelle position (permet de ne traiter chaque point qu'une fois)

  bool speedValid = false;
  float kmh = 0;
  bool courseValid = false;
  float course = 0;  // degrés, 0 = nord
  bool altValid = false;
  float alt = 0;  // mètres

  uint8_t sats = 0;
  bool hdopValid = false;
  float hdop = 99;

  bool timeValid = false;  // heure UTC
  uint8_t hour = 0, minute = 0, second = 0;
  bool dateValid = false;
  uint16_t year = 0;
  uint8_t month = 0, day = 0;
};
