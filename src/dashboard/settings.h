#pragma once
// Réglages modifiables depuis la page « Réglages », mémorisés dans la flash (Preferences).
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// Source de la destination (page Direction et guidage sur la carte)
enum DestMode { DM_AUTO, DM_HOME, DM_TRACK, DM_POINT };

struct Settings {
  bool logOn = true;              // enregistrer les trajets sur la SD
  int tz = TZ_OFFSET_H;           // décalage horaire par rapport à UTC (heures)
  int gaugeMin = GAUGE_MIN_MAX;   // échelle de départ de la jauge (km/h)
  bool homeOk = false;            // position « Domicile » enregistrée
  double homeLat = 0, homeLon = 0;
  int destMode = DM_AUTO;         // choix de la destination (automatique par défaut)
  bool pointOk = false;           // point choisi sur la carte ou fin de trajet
  double pointLat = 0, pointLon = 0;

  void load() {
    Preferences p;
    p.begin("cfg", true);
    logOn = p.getBool("log", logOn);
    tz = p.getInt("tz", tz);
    gaugeMin = p.getInt("gmin", gaugeMin);
    homeOk = p.getBool("hok", false);
    homeLat = p.getDouble("hlat", 0);
    homeLon = p.getDouble("hlon", 0);
    destMode = p.getInt("dmode", DM_AUTO);
    pointOk = p.getBool("pok", false);
    pointLat = p.getDouble("plat", 0);
    pointLon = p.getDouble("plon", 0);
    p.end();
    if (tz < -12 || tz > 14) tz = TZ_OFFSET_H;
    if (gaugeMin < 5 || gaugeMin > 30 || gaugeMin % 5) gaugeMin = GAUGE_MIN_MAX;
    if (destMode < DM_AUTO || destMode > DM_POINT) destMode = DM_AUTO;
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
    p.putInt("dmode", destMode);
    p.putBool("pok", pointOk);
    p.putDouble("plat", pointLat);
    p.putDouble("plon", pointLon);
    p.end();
  }
};
