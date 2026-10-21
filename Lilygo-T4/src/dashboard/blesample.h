// Adaptateur : message BLE (protocole BikeCopilot, voir common/ble/PROTOCOL.md) -> PhoneSample.
// Logique pure (aucune dépendance Arduino/BLE) : testable sur PC, voir ../../test/host.
// Ensuite, phoneApply() (phonefix.h) fait le reste, exactement comme pour SensorServer en Wi-Fi.
#pragma once
#include "ble_proto.h"
#include "phonefix.h"

// Décode un message écrit dans la caractéristique Feed ; renvoie faux si ce n'est pas un GPS_FIX valide.
// nowMs (millis) sert d'identifiant de point quand le téléphone n'envoie pas d'heure UTC :
// phoneApply() reconnaît un nouveau point à son horodatage, et nowMs + 1 est inférieur à
// l'an 2020, donc il n'est jamais pris pour une heure réelle.
static bool bleFeedToSample(const uint8_t *data, size_t len, uint32_t nowMs, PhoneSample &s) {
  BleGps g;
  if (!bleParseGpsFeed(data, len, g)) return false;
  s.have = true;
  s.lat = g.lat;
  s.lon = g.lon;
  s.speedValid = g.speedValid;
  s.speedMs = g.kmh / 3.6f;
  s.bearingValid = g.courseValid;
  s.bearing = g.course;
  s.altValid = g.altValid;
  s.alt = g.alt;
  s.accValid = g.accValid;
  s.acc = g.accM;
  s.timeMs = g.utc ? (uint64_t)g.utc * 1000ULL : (uint64_t)nowMs + 1;
  return true;
}
