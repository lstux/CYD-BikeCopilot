// Liens entre la page carte commune (common/src/mappage.h) et la LilyGO T4.
// La carte demande une boussole, un thème lié à la lumière, une destination et une carte SD : la T4 n'a
// que la position du téléphone, donc ces éléments sont remplacés ici par des versions simples.
// Inclus par main.cpp, après la définition de tft, fix, cfg, speed et des fonctions de vitesse/GPS.
#pragma once
#include <SD.h>
#include <SPI.h>

// Pas de boussole sur la T4 : le cap vient du téléphone (fix.course) quand on roule.
struct NoCompass {
  bool ok() const { return false; }
  bool calibrated() const { return false; }
  float heading(double, int) const { return 0; }
  void update() {}
};
static NoCompass compass;

// Thème : fixé par le réglage (clair / sombre). Pas de capteur de lumière sur la T4, donc « Auto » = sombre.
static bool mapLight = false;
static bool mapThemeUpdate(uint32_t) { return false; }  // le thème ne change qu'au réglage, pas en continu

// Destination : point choisi, domicile, ou fin de la trace GPX (même ordre de priorité que la CYD).
static double destLat = 0, destLon = 0;
static bool destValid = false;

// Carte SD : bus HSPI (l'écran est sur VSPI, voir platformio.ini), broches de config.h.
static SPIClass sdSPI(HSPI);
static bool sdOk = false;

// Vitesse et GPS : la T4 n'a pas de GPS série, la page carte n'a pas à interroger de matériel.
static void pollGps() {}
