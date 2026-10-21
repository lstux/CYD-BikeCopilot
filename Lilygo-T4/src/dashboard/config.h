#pragma once
// Configuration de la LilyGO T4 V1.3 (voir ../../Hardware.md pour le brochage complet).

// --- Écran ILI9341 : broches et réglages TFT_eSPI dans platformio.ini ---
#define TFT_ROTATION   1      // 1 = paysage ; mettre 3 si l'image est à l'envers

// --- Boutons (actifs au niveau bas, pull-up externe 4,7 kΩ sauf BOOT) ---
#define BTN1_PIN       37
#define BTN2_PIN       38
#define BTN3_PIN       39     // GPIO39 = S_VN
#define BTN4_PIN       0      // BOOT

// Rôles (provisoires, à adapter à la place des boutons sur ta carte) :
//   BTN1 court : page précédente      BTN2 court : page suivante
//   BTN3 long  : remise à zéro du trajet (avec confirmation : BTN3 long = oui, autre bouton = non)
//   BTN4 très long (5 s) : oublier la configuration Wi-Fi et redémarrer

// --- Batterie : GPIO35 lit la moitié de VBAT (pont 100 k / 100 k) ---
#define BATT_ADC_PIN   35

// --- Carte SD (bus séparé de l'écran, pas encore utilisée) ---
#define SD_CS_PIN      13
#define SD_SCK_PIN     14
#define SD_MISO_PIN    2
#define SD_MOSI_PIN    15
#define GPX_PATH       "/route.gpx"  // trace à suivre (dernier point = destination)

// --- Page carte (common/src/mappage.h) : cap pris sur le téléphone au-dessus de ce seuil (pas de boussole) ---
#define COURSE_MIN_KMH    6.0
#define COMPASS_OFFSET_DEG 0.0
#define COMPASS_INVERT     0

// --- I2C (connecteur 5 broches) ---
#define I2C_SDA_PIN    21
#define I2C_SCL_PIN    22

// --- Heure locale (France : 1 en hiver, 2 en été) ---
#define TZ_OFFSET_H    2

// --- Trajet : filtrage de la vitesse (le GPS « dérive » à l'arrêt et annonce 2-4 km/h) ---
#ifndef SPEED_START_KMH
#define SPEED_START_KMH  5.0    // au dessus (pendant SPEED_START_MS) : on passe « en mouvement »
#endif
#ifndef SPEED_STOP_KMH
#define SPEED_STOP_KMH   3.0    // en dessous : retour « à l'arrêt » (vitesse affichée = 0)
#endif
#define SPEED_START_MS   2000   // durée au dessus du seuil de départ
#define SPEED_ACC_MAX_M  25.0   // pour démarrer : précision du téléphone suffisante (mètres)
#define SPEED_TAU_MS     700    // lissage de la vitesse en mouvement
#define MIN_STEP_M       2.0    // ignore les micro-déplacements (bruit GPS)

// --- Jauge de vitesse (échelle adaptative) ---
#ifndef GAUGE_MIN_MAX
#define GAUGE_MIN_MAX     15    // échelle de départ (km/h)
#endif
#define GAUGE_STEP        5     // pas entre deux paliers
#define GAUGE_DOWN_MS     8000  // durée sous le palier inférieur avant de redescendre d'un cran
#define GAUGE_DOWN_RATIO  0.8   // "nettement sous" = moins de 80 % du palier inférieur
