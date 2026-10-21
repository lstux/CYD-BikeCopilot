// Tests sur PC de la logique pure de la T4 (aucune dépendance Arduino) :
// phonefix.h (téléphone -> Fix), speed.h (vitesse, trajet, échelle de la jauge), buttons.h.
// Lancer : Lilygo-T4/test/host/run.sh
#include <math.h>
#include <stdio.h>
#include "phonefix.h"
#include "speed.h"
#include "buttons.h"
#include "blesample.h"

static int fails = 0, checks = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    checks++;                                                             \
    if (!(cond)) {                                                        \
      fails++;                                                            \
      printf("ECHEC ligne %d : %s\n", __LINE__, #cond);                   \
    }                                                                     \
  } while (0)
#define NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))

static PhoneSample sample(double lat, double lon, uint64_t timeMs, float speedMs = 5.0f, float acc = 6.0f) {
  PhoneSample s;
  s.have = true;
  s.lat = lat;
  s.lon = lon;
  s.timeMs = timeMs;
  s.speedValid = true;
  s.speedMs = speedMs;
  s.bearingValid = true;
  s.bearing = 90;
  s.altValid = true;
  s.alt = 120;
  s.accValid = true;
  s.acc = acc;
  return s;
}

static void testPhoneFix() {
  Fix f;
  PhoneTracker t;
  PhoneSample none;
  phoneApply(f, t, none, 1000);
  CHECK(!f.valid);
  CHECK(f.seq == 0);

  // premier point : 2025-10-09 08:53:20 UTC
  PhoneSample s = sample(47.4784, -0.5632, 1760000000000ULL);
  phoneApply(f, t, s, 1000);
  CHECK(f.valid);
  CHECK(f.seq == 1);
  NEAR(f.lat, 47.4784, 1e-9);
  NEAR(f.kmh, 18.0, 1e-3);  // 5 m/s
  CHECK(f.speedValid && f.courseValid && f.altValid);
  NEAR(f.course, 90, 1e-6);
  NEAR(f.alt, 120, 1e-6);
  CHECK(f.timeValid && f.dateValid);
  CHECK(f.year == 2025 && f.month == 10 && f.day == 9);
  CHECK(f.hour == 8 && f.minute == 53 && f.second == 20);
  CHECK(f.sats == 0 && !f.hdopValid);
  CHECK(t.accValid);
  NEAR(t.acc, 6.0, 1e-6);

  // même point renvoyé : seq inchangé, encore valide à 3 s, périmé à 5 s
  phoneApply(f, t, s, 2000);
  CHECK(f.seq == 1 && f.valid);
  phoneApply(f, t, s, 3999);
  CHECK(f.valid);
  phoneApply(f, t, s, 6000);
  CHECK(!f.valid);
  CHECK(f.seq == 1);

  // nouveau point : valide à nouveau, seq + 1
  PhoneSample s2 = sample(47.4785, -0.5632, 1760000001000ULL);
  phoneApply(f, t, s2, 6100);
  CHECK(f.valid && f.seq == 2);

  // vitesse non fournie : kmh = 0 et speedValid = false
  PhoneSample s3 = sample(47.4786, -0.5632, 1760000002000ULL);
  s3.speedValid = false;
  phoneApply(f, t, s3, 6200);
  CHECK(!f.speedValid && f.kmh == 0);

  // précision absente
  PhoneSample s4 = sample(47.4787, -0.5632, 1760000003000ULL);
  s4.accValid = false;
  phoneApply(f, t, s4, 6300);
  CHECK(!t.accValid);

  // sans horodatage : un nouveau point se reconnaît à ses coordonnées
  Fix g;
  PhoneTracker u;
  PhoneSample a = sample(10, 20, 0);
  phoneApply(g, u, a, 100);
  CHECK(g.seq == 1 && !g.timeValid);
  phoneApply(g, u, a, 200);
  CHECK(g.seq == 1);
  PhoneSample b = sample(10.0001, 20, 0);
  phoneApply(g, u, b, 300);
  CHECK(g.seq == 2);

  // horodatage aberrant (avant 2020) : pas d'heure affichée
  Fix h;
  PhoneTracker v;
  PhoneSample c = sample(1, 1, 12345);
  phoneApply(h, v, c, 100);
  CHECK(h.valid && !h.timeValid);

  // le compteur de millis() qui boucle ne casse pas la validité
  Fix k;
  PhoneTracker w;
  PhoneSample d = sample(2, 2, 1760000000000ULL);
  phoneApply(k, w, d, 0xFFFFFF00u);
  phoneApply(k, w, d, 0xFFFFFF00u + 1000u);  // = 744 après débordement
  CHECK(k.valid);
}

static void testSpeed() {
  Fix f;
  f.valid = true;
  f.speedValid = true;
  SpeedFilter sp;

  // dérive à l'arrêt : 2 km/h ne démarre rien
  f.kmh = 2;
  for (uint32_t t = 100; t < 10000; t += 100) sp.update(t, f, true);
  CHECK(!sp.moving && sp.kmh == 0);

  // 8 km/h pendant 2 s : passe en mouvement
  f.kmh = 8;
  uint32_t t = 10000;
  for (; t < 11500; t += 100) sp.update(t, f, true);
  CHECK(!sp.moving);  // pas encore 2 s
  for (; t < 12500; t += 100) sp.update(t, f, true);
  CHECK(sp.moving);
  CHECK(sp.kmh > 7.5f && sp.kmh <= 8.01f);

  // accélération : lissée, converge vers la valeur brute
  f.kmh = 20;
  for (int i = 0; i < 60; i++, t += 100) sp.update(t, f, true);
  NEAR(sp.kmh, 20, 0.5);

  // sous le seuil d'arrêt : retombe à 0
  f.kmh = 1;
  sp.update(t, f, true);
  CHECK(!sp.moving && sp.kmh == 0);

  // précision insuffisante : pas de départ
  SpeedFilter sp2;
  f.kmh = 12;
  for (uint32_t u = 1000; u < 6000; u += 100) sp2.update(u, f, false);
  CHECK(!sp2.moving);

  // position périmée : pas de départ
  SpeedFilter sp3;
  f.valid = false;
  for (uint32_t u = 1000; u < 6000; u += 100) sp3.update(u, f, true);
  CHECK(!sp3.moving);
}

static void testTrip() {
  Fix f;
  f.valid = true;
  f.speedValid = true;
  f.kmh = 18;
  f.altValid = true;
  SpeedFilter sp;
  sp.moving = true;
  sp.kmh = 18;
  TripStats tr;

  // ~1 km vers le nord, un point par seconde à 5 m/s (soit 200 points), altitude +10 m
  double lat = 47.0;
  uint32_t now = 1000;
  for (int i = 0; i < 200; i++) {
    lat += 5.0 / 111195.0;  // 5 m
    f.lat = lat;
    f.lon = -0.5;
    f.alt = 100 + i * 0.05f;
    f.seq++;
    now += 1000;
    tr.update(now, f, sp);
  }
  NEAR(tr.distM, 995, 20);  // 199 intervalles de 5 m
  CHECK(tr.movingMs == 199000);
  NEAR(tr.avgKmh(), 18, 0.3);
  NEAR(tr.maxKmh, 18, 0.01);
  CHECK(tr.climbM > 5 && tr.climbM < 11);

  // un même point (seq identique) n'est pas compté deux fois
  double d0 = tr.distM;
  tr.update(now + 1000, f, sp);
  NEAR(tr.distM, d0, 1e-9);

  // à l'arrêt : le temps et la distance n'avancent pas
  sp.moving = false;
  f.seq++;
  f.lat += 0.001;
  tr.update(now + 2000, f, sp);
  NEAR(tr.distM, d0, 1e-9);

  tr.reset();
  CHECK(tr.distM == 0 && tr.movingMs == 0 && !tr.hasLast);
}

static void testGauge() {
  GaugeScale g;
  CHECK(g.maxKmh == 15);
  g.update(10, 1000, 15);
  CHECK(g.maxKmh == 15);
  g.update(22, 2000, 15);  // dépassement : palier juste au-dessus
  CHECK(g.maxKmh == 25);
  g.update(31, 2100, 15);
  CHECK(g.maxKmh == 35);
  // sous 80 % du palier inférieur (30 km/h -> 24) pendant 8 s : descend d'un cran
  uint32_t t = 3000;
  g.update(10, t, 15);
  CHECK(g.maxKmh == 35);
  g.update(10, t + 7000, 15);
  CHECK(g.maxKmh == 35);
  g.update(10, t + 8100, 15);
  CHECK(g.maxKmh == 30);
  // jamais sous l'échelle mini
  for (int i = 0; i < 50; i++) g.update(0, t + 9000 + i * 9000, 15);
  CHECK(g.maxKmh == 15);
  // échelle mini réglée à 5 (mode « à pied »)
  GaugeScale w;
  w.maxKmh = 5;
  w.update(3, 100, 5);
  CHECK(w.maxKmh == 5);
  w.update(6, 200, 5);
  CHECK(w.maxKmh == 10);
}

static void testButtons() {
  Button b;
  // appui court : un seul BE_SHORT au relâchement
  CHECK(b.update(false, 0) == BE_NONE);
  CHECK(b.update(true, 100) == BE_NONE);
  CHECK(b.update(true, 300) == BE_NONE);
  CHECK(b.update(false, 400) == BE_SHORT);
  CHECK(b.update(false, 500) == BE_NONE);

  // rebond (moins de 30 ms) : ignoré
  CHECK(b.update(true, 1000) == BE_NONE);
  CHECK(b.update(false, 1020) == BE_NONE);

  // appui long : BE_LONG une fois pendant l'appui, rien au relâchement
  CHECK(b.update(true, 2000) == BE_NONE);
  CHECK(b.update(true, 2900) == BE_NONE);
  CHECK(b.update(true, 3000) == BE_LONG);
  CHECK(b.update(true, 3500) == BE_NONE);
  CHECK(b.update(false, 3600) == BE_NONE);

  // appui très long : BE_LONG à 1 s puis BE_VLONG à 5 s
  CHECK(b.update(true, 10000) == BE_NONE);
  CHECK(b.update(true, 11000) == BE_LONG);
  CHECK(b.update(true, 14900) == BE_NONE);
  CHECK(b.update(true, 15000) == BE_VLONG);
  CHECK(b.update(true, 16000) == BE_NONE);
  CHECK(b.update(false, 16100) == BE_NONE);
}

// Message GPS_FIX du protocole BLE (vecteur de common/ble/PROTOCOL.md) -> PhoneSample -> Fix
static const uint8_t BLE_FULL[] = {0x01, 0x14, 0xf0, 0xec, 0x1e, 0x1d, 0xd0, 0xea, 0x66, 0x01, 0xea,
                                   0x00, 0x53, 0x07, 0x23, 0x00, 0x06, 0x0c, 0x5b, 0x95, 0xd8, 0x6a};
static const uint8_t BLE_NOTIME[] = {0x01, 0x14, 0xf0, 0xec, 0x1e, 0x1d, 0xd0, 0xea, 0x66, 0x01, 0xff,
                                     0xff, 0xff, 0xff, 0x00, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};

static void testBle() {
  PhoneSample s;
  CHECK(bleFeedToSample(BLE_FULL, sizeof BLE_FULL, 5000, s));
  CHECK(s.have);
  NEAR(s.lat, 48.8566, 1e-6);
  NEAR(s.lon, 2.3522, 1e-6);
  CHECK(s.speedValid);
  NEAR(s.speedMs, 23.4 / 3.6, 1e-4);  // km/h -> m/s
  CHECK(s.bearingValid);
  NEAR(s.bearing, 187.5, 1e-4);
  CHECK(s.altValid);
  NEAR(s.alt, 35, 1e-6);
  CHECK(s.accValid);
  NEAR(s.acc, 6, 1e-6);
  CHECK(s.timeMs == 1792578907000ULL);

  // jusqu'à la Fix : heure UTC 2026-10-21 10:35:07, vitesse en km/h
  Fix f;
  PhoneTracker t;
  phoneApply(f, t, s, 5000);
  CHECK(f.valid && f.seq == 1);
  NEAR(f.kmh, 23.4, 1e-3);
  CHECK(f.timeValid && f.dateValid);
  CHECK(f.year == 2026 && f.month == 10 && f.day == 21 && f.hour == 10 && f.minute == 35 && f.second == 7);

  // même message reçu deux fois (même seconde UTC) : pas un nouveau point
  phoneApply(f, t, s, 6000);
  CHECK(f.seq == 1);
  // message suivant (seconde suivante) : nouveau point
  PhoneSample s2 = s;
  s2.timeMs += 1000;
  phoneApply(f, t, s2, 6100);
  CHECK(f.seq == 2);
  // silence de plus de 5 s : la Fix n'est plus valide
  phoneApply(f, t, s2, 6100 + PHONE_FIX_STALE_MS + 1);
  CHECK(!f.valid);

  // sans heure UTC : chaque message est un nouveau point (identifié par millis), l'heure reste inconnue
  PhoneSample a, b;
  Fix g;
  PhoneTracker tg;
  CHECK(bleFeedToSample(BLE_NOTIME, sizeof BLE_NOTIME, 10000, a));
  CHECK(bleFeedToSample(BLE_NOTIME, sizeof BLE_NOTIME, 11000, b));
  CHECK(!a.speedValid && !a.bearingValid && !a.altValid && !a.accValid);
  phoneApply(g, tg, a, 10000);
  phoneApply(g, tg, b, 11000);
  CHECK(g.valid && g.seq == 2 && !g.timeValid);

  // messages à ignorer : trop courts, type inconnu
  PhoneSample none;
  CHECK(!bleFeedToSample(BLE_FULL, 1, 0, none));
  const uint8_t other[] = {0x7e, 0x02, 0xaa, 0xbb};
  CHECK(!bleFeedToSample(other, sizeof other, 0, none));
  CHECK(!none.have);
}

int main() {
  testPhoneFix();
  testBle();
  testSpeed();
  testTrip();
  testGauge();
  testButtons();
  printf("%d verifications, %d echec(s)\n", checks, fails);
  return fails ? 1 : 0;
}
