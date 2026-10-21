# LilyGO T4 V1.3 : matériel

Carte de référence pour le projet Lilygo-T4 Bike Copilot (GPS du téléphone via Wi-Fi/BLE).

Sources : fiche produit LilyGO (lilygo.cc), article CNX Software (2022), README du dépôt `Xinyuan-LilyGO/LilyGo_Txx`, et dépôt `Xinyuan-LilyGO/TTGO-T4-TestCode`. Les points marqués **À VÉRIFIER** n'ont pas pu être confirmés sur le schéma.

## Caractéristiques

| Élément | Valeur |
|---|---|
| Carte | LilyGO T4 V1.3 (écran 2,4" ; une variante 2,2" existe) |
| MCU | ESP32 double cœur Tensilica LX6, jusqu'à 240 MHz |
| SRAM interne | 520 Ko |
| PSRAM | 8 Mo (ISSI PN18S32) |
| Flash | 4 Mo SPI NOR |
| Wi-Fi | 802.11 b/g/n (Wi-Fi 4) |
| Bluetooth | 4.2 BR/EDR et BLE |
| Antenne | antenne 3D intégrée, connecteur u.FL pour antenne externe |
| Écran | TFT ILI9341, 320x240, 65K couleurs, interface SPI |
| Rétroéclairage | piloté par GPIO 4 |
| Stockage | slot microSD |
| Puce USB/série | CH9102F (QFN24) selon la fiche LilyGO ; certains articles citent un CP2104 |
| USB | micro-USB (alimentation, charge, programmation, debug) |
| Alimentation | tension de fonctionnement 3,3 V |
| Batterie | LiPo, chargeur IP5306 (2,1 A) |
| Connecteur batterie | JST GH 1,25 mm |
| Connecteur I2C | 5 broches |
| Header GPIO | 20 broches |
| Boutons | Boot, RST, bouton de détection de tension batterie |
| Programmation | Arduino IDE, MicroPython |

## Configuration de l'écran (TFT_eSPI)

Fichier à utiliser : `User_Setups/Setup22_TTGO_T4_v1.3.h` (dépôt LilyGo_Txx). Dans `User_Setup_Select.h` de TFT_eSPI, désactiver `<User_Setup.h>` et activer ce fichier.

Dans le sketch, selon le dépôt : `#define T4_ V13` (version en vente).

## Brochage GPIO

### Écran TFT (ILI9341, SPI)

| Signal | GPIO |
|---|---|
| TFT_MISO | 12 |
| TFT_MOSI | 23 |
| TFT_SCLK | 18 |
| TFT_CS | 27 |
| TFT_DC | 32 |
| TFT_RST | 5 |
| TFT_BL (rétroéclairage) | 4 |

### Carte microSD (SPI)

| Signal | GPIO |
|---|---|
| SD_MISO | 2 |
| SD_MOSI | 15 |
| SD_SCK | 14 |
| SD_CS | 13 |

### Boutons

| Bouton | GPIO | Remarque |
|---|---|---|
| Button 0 | 14 | **À VÉRIFIER** : partage la même broche que SD_SCK |
| Button 1 | 5 | **À VÉRIFIER** : partage la même broche que TFT_RST |
| Button 2 | 27 | **À VÉRIFIER** : partage la même broche que TFT_CS |
| Button 3 | 25 | **À VÉRIFIER** : partage la même broche que Speaker out |
| BOOT | non listé | à confirmer sur le schéma (GPIO 0 sur la plupart des ESP32) |

Les conflits ci-dessus viennent du tableau du dépôt TestCode. Ils ne sont pas plausibles en matériel réel, donc ces lignes sont probablement fausses ou issues d'une autre révision. Ne pas câbler de capteur sur ces broches tant que le schéma n'est pas vérifié.

### Audio

| Signal | GPIO |
|---|---|
| Speaker out | 25 |
| Speaker PWD (activation ampli) | 19 |

### Bus I2C (connecteur 5 broches)

| Signal | GPIO |
|---|---|
| I2C SDA | 21 |
| I2C SCL | 22 |

### Batterie

La tension batterie est mesurée sur une entrée ADC, sans étiquette précise dans les sources. **À VÉRIFIER** : le dépôt T10 utilise GPIO 35 pour `ADC_IN`, mais rien ne confirme que ce soit la même broche sur le T4 V1.3.

### Tactile

Aucune broche tactile n'est listée pour le T4 V1.3.

## Points à vérifier avant câblage

1. Le schéma officiel (dépôt `TTGO-T4-TestCode` ou `LilyGo_Txx/schematic`) pour confirmer les GPIO des boutons, de la batterie et du BOOT.
2. La puce USB : CH9102F ou CP2104 selon le lot, impact sur le driver à installer.
3. Les GPIO utilisés par l'écran et la microSD ne doivent pas être réutilisés pour le capteur de vitesse ou le GPS.
4. GPIO 34 à 39 sont en entrée seule sur l'ESP32 : ne pas les utiliser pour des sorties.
