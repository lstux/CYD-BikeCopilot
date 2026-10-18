#pragma once
// Réglages modifiables depuis la page « Réglages », mémorisés dans la flash (Preferences).
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct Settings {
  bool logOn = true;              // enregistrer les trajets sur la SD
  int tz = TZ_OFFSET_H;           // décalage horaire par rapport à UTC (heures)
  int gaugeMin = GAUGE_MIN_MAX;   // échelle de départ de la jauge (km/h)
  bool homeOk = false;            // position « Domicile » enregistrée
  double homeLat = 0, homeLon = 0;

  void load() {
    Preferences p;
    p.begin("cfg", true);
    logOn = p.getBool("log", logOn);
    tz = p.getInt("tz", tz);
    gaugeMin = p.getInt("gmin", gaugeMin);
    homeOk = p.getBool("hok", false);
    homeLat = p.getDouble("hlat", 0);
    homeLon = p.getDouble("hlon", 0);
    p.end();
    if (tz < -12 || tz > 14) tz = TZ_OFFSET_H;
    if (gaugeMin < 5 || gaugeMin > 30 || gaugeMin % 5) gaugeMin = GAUGE_MIN_MAX;
  }

  void save() {
    Preferences p;
    p.begin("cfg", false);
    p.putBool("log", logOn);
    p.putInt("tz", tz);
    p.putInt("gmin", gaugeMin);
    p.putBool("hok", homeOk);
    p.putDouble("hlat", homeLat);
    p.putDouble("hlon", homeLon);
    p.end();
  }
};
