// Protocole BLE BikeCopilot v1 (voir common/ble/PROTOCOL.md) : constantes et décodage côté carte.
// Sans dépendance matérielle : la pile BLE de la carte (NimBLE, Bluedroid…) appelle bleHandleFeedWrite()
// avec les octets reçus dans la caractéristique Feed, et lit la Fix obtenue.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fix.h"

// ---- UUID (famille 0a2b00NN-ec15-422d-8e42-7dfd3f9bff50) ----
#define BLE_UUID_SERVICE "0a2b0001-ec15-422d-8e42-7dfd3f9bff50"
#define BLE_UUID_INFO "0a2b0002-ec15-422d-8e42-7dfd3f9bff50"   // Read
#define BLE_UUID_FEED "0a2b0003-ec15-422d-8e42-7dfd3f9bff50"   // Write sans réponse
#define BLE_UUID_EVENT "0a2b0004-ec15-422d-8e42-7dfd3f9bff50"  // Notify (réservé)

#define BLE_PROTO_MAJOR 1
#define BLE_PROTO_MINOR 0
#define BLE_MIN_WRITE_LEN 22   // taille du plus gros message v1 (GPS_FIX) : la carte doit accepter au moins ça
#define BLE_FIX_TIMEOUT_MS 5000  // sans message depuis ce délai, la Fix n'est plus valide

enum BleMsgType : uint8_t {
  BLE_MSG_GPS_FIX = 0x01,
  // 0x02 DEST, 0x03 TIME : prévus, pas encore définis
};

enum BleBoardKind : uint8_t {
  BLE_BOARD_UNKNOWN = 0,
  BLE_BOARD_CYD = 1,
  BLE_BOARD_LILYGO_T4 = 2,
  BLE_BOARD_WAVESHARE_C6 = 3,
  BLE_BOARD_OTHER = 255,
};

// ---- Device Info (valeur à fournir en lecture) ----
// Écrit dans `out` (au moins 4 + nTypes octets) ; renvoie la taille.
inline size_t bleBuildDeviceInfo(uint8_t* out, BleBoardKind board, const uint8_t* types, uint8_t nTypes) {
  out[0] = BLE_PROTO_MAJOR;
  out[1] = BLE_PROTO_MINOR;
  out[2] = board;
  out[3] = nTypes;
  for (uint8_t i = 0; i < nTypes; i++) out[4 + i] = types[i];
  return 4 + nTypes;
}

// ---- Décodage (little-endian, indépendant du CPU) ----
inline uint16_t bleU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t bleU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Jour civil (calendrier grégorien) depuis le nombre de jours depuis 1970-01-01 (algorithme de H. Hinnant).
inline void bleCivilFromDays(int32_t z, uint16_t& y, uint8_t& m, uint8_t& d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = (uint32_t)(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t yy = (int32_t)yoe + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  d = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
  m = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
  y = (uint16_t)(yy + (m <= 2));
}

// Décode un message GPS_FIX (charge utile seule, sans les 2 octets d'en-tête) dans `f`.
// Les champs absents (message plus court : ancienne appli) restent « inconnus ». `f.valid` et `f.seq` sont mis à jour.
inline bool bleDecodeGpsFix(const uint8_t* p, size_t len, Fix& f) {
  if (len < 8) return false;  // lat + lon obligatoires
  f.lat = (int32_t)bleU32(p) / 1e7;
  f.lon = (int32_t)bleU32(p + 4) / 1e7;

  f.speedValid = len >= 10 && bleU16(p + 8) != 0xFFFF;
  f.kmh = f.speedValid ? bleU16(p + 8) / 10.0f : 0;

  f.courseValid = len >= 12 && bleU16(p + 10) != 0xFFFF;
  f.course = f.courseValid ? bleU16(p + 10) / 10.0f : 0;

  const int16_t alt = len >= 14 ? (int16_t)bleU16(p + 12) : (int16_t)-32768;
  f.altValid = alt != -32768;
  f.alt = f.altValid ? alt : 0;

  const uint8_t acc = len >= 15 ? p[14] : 255;
  f.hdopValid = acc != 255;
  f.hdop = f.hdopValid ? acc / 5.0f : 99;

  f.sats = len >= 16 ? p[15] : 0;

  const uint32_t utc = len >= 20 ? bleU32(p + 16) : 0;
  f.timeValid = f.dateValid = utc != 0;
  if (f.timeValid) {
    const uint32_t secs = utc % 86400;
    f.hour = (uint8_t)(secs / 3600);
    f.minute = (uint8_t)(secs % 3600 / 60);
    f.second = (uint8_t)(secs % 60);
    bleCivilFromDays((int32_t)(utc / 86400), f.year, f.month, f.day);
  } else {
    f.hour = f.minute = f.second = 0;
    f.year = 0;
    f.month = f.day = 0;
  }

  f.valid = true;
  f.seq++;
  return true;
}

// À appeler avec chaque écriture reçue dans Feed. Renvoie true si le message a mis la Fix à jour.
// Les types inconnus et les messages tronqués sont ignorés.
inline bool bleHandleFeedWrite(const uint8_t* data, size_t len, Fix& f) {
  if (len < 2) return false;
  const uint8_t type = data[0];
  const size_t plen = data[1];
  if (plen > len - 2) return false;  // annonce plus longue que ce qui est arrivé
  switch (type) {
    case BLE_MSG_GPS_FIX:
      return bleDecodeGpsFix(data + 2, plen, f);
    default:
      return false;
  }
}

// À appeler régulièrement (loop) : invalide la Fix si plus rien n'est arrivé depuis BLE_FIX_TIMEOUT_MS.
inline void bleExpireFix(Fix& f, uint32_t nowMs, uint32_t lastMsgMs) {
  if (f.valid && (uint32_t)(nowMs - lastMsgMs) > BLE_FIX_TIMEOUT_MS) f.valid = false;
}
