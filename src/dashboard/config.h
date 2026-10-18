#pragma once

// --- Câblage ---
#define GPS_RX_PIN   35
#define GPS_BAUD     38400
#define I2C_SDA_PIN  27
#define I2C_SCL_PIN  22
#define BUTTON_PIN   0      // bouton BOOT : appui court = page suivante, appui long = reset trajet

// --- Heure locale (France : 1 en hiver, 2 en été) ---
#define TZ_OFFSET_H  2

// --- Destination (à remplacer, plus tard saisie via GPX / écran) ---
#define DEST_LAT     48.858370
#define DEST_LON     2.294481

// --- Trajet ---
#define MOVING_MIN_KMH   3.0    // en dessous : considéré à l'arrêt
#define MIN_STEP_M       2.0    // ignore les micro-déplacements (bruit GPS)
#define COURSE_MIN_KMH   6.0    // au dessus : cap GPS plutôt que boussole

// --- Boussole ---
#define COMPASS_OFFSET_DEG  0.0 // à ajuster selon l'orientation du module sur la carte
#define COMPASS_INVERT      0   // 1 si le sens de rotation est inversé
