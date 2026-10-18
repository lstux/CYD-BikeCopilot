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
// Filtrage de la vitesse (le GPS « dérive » à l'arrêt et annonce 2-4 km/h)
#ifndef SPEED_START_KMH
#define SPEED_START_KMH  5.0    // au dessus (pendant SPEED_START_MS) : on passe « en mouvement »
#endif
#ifndef SPEED_STOP_KMH
#define SPEED_STOP_KMH   3.0    // en dessous : retour « à l'arrêt » (vitesse affichée = 0)
#endif
#define SPEED_START_MS   2000   // durée au dessus du seuil de départ
#define SPEED_HDOP_MAX   3.0    // pour démarrer : précision GPS suffisante (HDOP)
#define SPEED_MIN_SATS   4      // pour démarrer : nombre de satellites minimum
#define SPEED_TAU_MS     700    // lissage de la vitesse en mouvement
#define MIN_STEP_M       2.0    // ignore les micro-déplacements (bruit GPS)
#define COURSE_MIN_KMH   6.0    // au dessus : cap GPS plutôt que boussole

// --- Boussole ---
#define COMPASS_OFFSET_DEG  180.0 // module monté à l'envers : décalage mesuré de 180°
#define COMPASS_INVERT      0   // 1 si le sens de rotation est inversé

// --- Jauge de vitesse (échelle adaptative) ---
#ifndef GAUGE_MIN_MAX
#define GAUGE_MIN_MAX     15    // échelle de départ (km/h)
#endif
#define GAUGE_STEP        5     // pas entre deux paliers
#define GAUGE_DOWN_MS     8000  // durée sous le palier inférieur avant de redescendre d'un cran
#define GAUGE_DOWN_RATIO  0.8   // "nettement sous" = moins de 80 % du palier inférieur

// --- Carte SD et carte routière ---
#define SD_CS_PIN     5
#define SD_SCK_PIN    18
#define SD_MISO_PIN   19
#define SD_MOSI_PIN   23
#define GPX_PATH      "/route.gpx"   // trace à suivre (dernier point = destination)

// Écran tactile XPT2046 (SPI logiciel)
#define TOUCH_CLK_PIN   25
#define TOUCH_MOSI_PIN  32
#define TOUCH_MISO_PIN  39
#define TOUCH_CS_PIN    33
#define TOUCH_IRQ_PIN   36
