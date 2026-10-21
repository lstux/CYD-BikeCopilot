// Test hôte du décodeur BLE. Depuis la racine du dépôt :
//   g++ -std=c++17 -Wall -Wextra -I common/src common/tools/ble_proto_test.cpp -o /tmp/ble_proto_test && /tmp/ble_proto_test
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "ble_proto.h"

static int fails = 0;
#define CHECK(c)                                              \
  do {                                                        \
    if (!(c)) {                                               \
      printf("ECHEC ligne %d : %s\n", __LINE__, #c);          \
      fails++;                                                \
    }                                                         \
  } while (0)
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

// Vecteurs de common/ble/PROTOCOL.md
static const uint8_t FULL[] = {0x01, 0x14, 0xf0, 0xec, 0x1e, 0x1d, 0xd0, 0xea, 0x66, 0x01, 0xea,
                               0x00, 0x53, 0x07, 0x23, 0x00, 0x06, 0x0c, 0x5b, 0x95, 0xd8, 0x6a};
static const uint8_t UNKNOWN[] = {0x01, 0x14, 0xf0, 0xec, 0x1e, 0x1d, 0xd0, 0xea, 0x66, 0x01, 0xff,
                                  0xff, 0xff, 0xff, 0x00, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};

int main() {
  // Message complet
  {
    Fix f;
    CHECK(bleHandleFeedWrite(FULL, sizeof FULL, f));
    CHECK(f.valid && f.seq == 1);
    CHECK(near(f.lat, 48.8566) && near(f.lon, 2.3522));
    CHECK(f.speedValid && near(f.kmh, 23.4, 1e-4));
    CHECK(f.courseValid && near(f.course, 187.5, 1e-4));
    CHECK(f.altValid && near(f.alt, 35));
    CHECK(f.hdopValid && near(f.hdop, 1.2, 1e-4));
    CHECK(f.sats == 12);
    CHECK(f.timeValid && f.dateValid);
    CHECK(f.year == 2026 && f.month == 10 && f.day == 21);
    CHECK(f.hour == 10 && f.minute == 35 && f.second == 7);
    CHECK(bleHandleFeedWrite(FULL, sizeof FULL, f) && f.seq == 2);  // seq s'incrémente
  }
  // Tout inconnu sauf la position
  {
    Fix f;
    CHECK(bleHandleFeedWrite(UNKNOWN, sizeof UNKNOWN, f));
    CHECK(near(f.lat, 48.8566) && near(f.lon, 2.3522));
    CHECK(!f.speedValid && !f.courseValid && !f.altValid && !f.hdopValid);
    CHECK(!f.timeValid && !f.dateValid && f.sats == 0);
  }
  // Ancienne appli : charge utile plus courte (lat+lon+vitesse seulement), octets en trop ignorés
  {
    Fix f;
    uint8_t shortMsg[2 + 10];
    memcpy(shortMsg, FULL, sizeof shortMsg);
    shortMsg[1] = 10;
    CHECK(bleHandleFeedWrite(shortMsg, sizeof shortMsg, f));
    CHECK(f.speedValid && near(f.kmh, 23.4, 1e-4) && !f.courseValid && !f.altValid && !f.timeValid);

    uint8_t longMsg[2 + 24] = {};
    memcpy(longMsg, FULL, sizeof FULL);
    longMsg[1] = 24;  // futurs champs ajoutés à la fin
    Fix g;
    CHECK(bleHandleFeedWrite(longMsg, sizeof longMsg, g) && g.sats == 12);
  }
  // Rejets
  {
    Fix f;
    CHECK(!bleHandleFeedWrite(FULL, 1, f));       // trop court
    CHECK(!bleHandleFeedWrite(FULL, 10, f));      // len annoncée (20) > reçu (8)
    const uint8_t unknownType[] = {0x7e, 0x02, 0xaa, 0xbb};
    CHECK(!bleHandleFeedWrite(unknownType, sizeof unknownType, f));
    const uint8_t tooFew[] = {0x01, 0x04, 1, 2, 3, 4};  // moins de lat+lon
    CHECK(!bleHandleFeedWrite(tooFew, sizeof tooFew, f));
    CHECK(!f.valid && f.seq == 0);
  }
  // Péremption (avec débordement du compteur millis)
  {
    Fix f;
    bleHandleFeedWrite(FULL, sizeof FULL, f);
    bleExpireFix(f, 1000 + 4999, 1000);
    CHECK(f.valid);
    bleExpireFix(f, 1000 + 5001, 1000);
    CHECK(!f.valid);
    Fix g;
    bleHandleFeedWrite(FULL, sizeof FULL, g);
    bleExpireFix(g, 100, 0xFFFFFF00u);  // wrap : 356 ms écoulées
    CHECK(g.valid);
  }
  // Dates : 1970-01-01, 2000-02-29, 2038-01-19 03:14:08 (au-delà de int32 signé)
  {
    uint16_t y; uint8_t m, d;
    bleCivilFromDays(0, y, m, d);
    CHECK(y == 1970 && m == 1 && d == 1);
    bleCivilFromDays(11016, y, m, d);  // 2000-02-29
    CHECK(y == 2000 && m == 2 && d == 29);
    bleCivilFromDays(2147483648u / 86400, y, m, d);
    CHECK(y == 2038 && m == 1 && d == 19);
  }
  // Device Info
  {
    uint8_t buf[8];
    const uint8_t types[] = {BLE_MSG_GPS_FIX};
    const size_t n = bleBuildDeviceInfo(buf, BLE_BOARD_LILYGO_T4, types, 1);
    CHECK(n == 5 && buf[0] == 1 && buf[1] == 0 && buf[2] == 2 && buf[3] == 1 && buf[4] == 1);
  }
  printf(fails ? "%d échec(s)\n" : "OK\n", fails);
  return fails ? 1 : 0;
}
