# LilyGO T4 V1.3 : matériel

Carte de référence du projet Lilygo-T4 Bike Copilot (GPS du téléphone via Wi-Fi/BLE).

**Sources** (par ordre de fiabilité) :

1. Schéma officiel `schematic/T4_v1.3.pdf` (4 pages, daté du 21/02/2019), dépôt [Xinyuan-LilyGO/LilyGo_Txx](https://github.com/Xinyuan-LilyGO/LilyGo_Txx) (le dépôt `TTGO-T4-TestCode` a le même contenu).
2. Code officiel : `LilyGo_Txx/T4_V13.h` (brochage) et `User_Setups/T4_V13.h` (config TFT_eSPI).
3. Fiche produit lilygo.cc et article CNX Software pour les caractéristiques générales.

Le brochage ci-dessous a été recoupé entre le schéma et `T4_V13.h` : les deux concordent. Le tableau du README du dépôt, lui, est **faux pour les boutons** (il y donne 14/5/27/25, qui sont des broches de la microSD, de l'écran et de l'audio). Ne pas s'y fier.

## Caractéristiques

| Élément | Valeur |
|---|---|
| Carte | LilyGO T4 V1.3 (écran 2,4" ; une variante 2,2" existe) |
| MCU | ESP32 double cœur Tensilica LX6, jusqu'à 240 MHz, 520 Ko de SRAM |
| Flash | 4 Mo SPI NOR |
| PSRAM | 8 Mo (ISSI PN18S32, 1,8 V), CS sur GPIO16, horloge sur GPIO17 |
| Wi-Fi / Bluetooth | 802.11 b/g/n ; Bluetooth 4.2 BR/EDR + BLE |
| Antenne | antenne 3D sur la carte, connecteur u.FL pour antenne externe |
| Écran | TFT ILI9341, 320x240, 65K couleurs, SPI 4 fils |
| Rétroéclairage | 4 LED (4x15 mA) pilotées par un transistor sur GPIO4 |
| Stockage | slot microSD (SPI) |
| Audio | amplificateur NS4150 (classe D) sur GPIO25, connecteur haut-parleur 2 broches |
| Batterie | LiPo via IP5306 (charge 2,1 A, sortie 5 V), 2 connecteurs PH 2 broches (P2, P3) |
| USB | micro-USB (alim, charge, programmation) |
| Pont USB-série | CP2104 sur le schéma V1.3 ; **CH9102F** sur la fiche actuelle de lilygo.cc (lot plus récent). Installer le driver qui correspond à ta carte. |
| Programmation | Arduino IDE, MicroPython |

Réglages Arduino : carte « ESP32 Dev Module », **PSRAM activée** (la PSRAM est présente sur cette carte, contrairement à la T-Display), flash 4 Mo.

## Configuration de l'écran (TFT_eSPI)

Fichier : `User_Setups/T4_V13.h` du dépôt LilyGo_Txx (TFT_eSPI fournit aussi `Setup22_TTGO_T4_v1.3.h`). Contenu utile :

```c
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_MISO 12
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   27
#define TFT_DC   32
#define TFT_RST   5
#define SPI_FREQUENCY 60000000
```

**Attention : sur notre carte, TFT_DC est sur GPIO26, pas 32.** Avec 32 l'écran reste blanc (rétroéclairage allumé, contrôleur jamais configuré). GPIO26 est la valeur de `Setup22_TTGO_T4.h` (anciennes T4) et celle utilisée avec succès dans le projet FarmerT4. Le firmware utilise donc `-DTFT_DC=26`.

Le rétroéclairage (`TFT_BL`) n'est pas dans ce fichier : il est défini dans le sketch (`T4_V13.h`) sur GPIO4.

## Brochage complet des GPIO

### Écran TFT (SPI)

| Signal | GPIO | Remarque |
|---|---|---|
| TFT_MOSI (SDI) | 23 | |
| TFT_SCLK (SCK) | 18 | |
| TFT_CS | 27 | |
| TFT_DC (RS) | 32 | exposé aussi sur le header (pin 8). **Notre carte : GPIO26** (voir ci-dessous) |
| TFT_RST | 5 | |
| TFT_MISO (SDO) | 12 | GPIO12 est une broche de strapping (tension flash) : ne pas la forcer à 1 au démarrage |
| TFT_BL | 4 | active haut, via transistor Q14 (MMBT2222) ; PWM possible |

### Carte microSD (SPI séparé, 4 broches)

| Signal | GPIO |
|---|---|
| SD_MISO | 2 |
| SD_MOSI (CMD) | 15 |
| SD_SCLK | 14 |
| SD_CS | 13 |

Le signal de détection de carte (CD) n'est relié à aucun GPIO, seulement à un point de test. Sur le bus SD, GPIO2 et GPIO15 sont des broches de strapping ; le schéma ajoute des pull-up 10 kΩ et des diodes de protection.

### Boutons (actifs au niveau bas)

Les trois boutons du bord ont chacun un pull-up externe de 4,7 kΩ vers 3,3 V ; aucun pull-up interne n'est nécessaire (et GPIO34 à 39 n'en ont de toute façon pas).

| Bouton | GPIO | Composant |
|---|---|---|
| BUTTON_1 | 37 | K4 |
| BUTTON_2 | 38 | K3 |
| BUTTON_3 | 39 (S_VN) | K2 |
| BUTTON_4 / BOOT | 0 | SW4 « boot » |

Le bouton RST (SW3) réinitialise l'ESP32 (EN). `BUTTONS_MAP = {37, 38, 39, 0}` dans le code officiel, avec la bibliothèque Button2.

### Audio

| Signal | GPIO | Rôle |
|---|---|---|
| SPEAKER_OUT | 25 | sortie audio (DAC1) vers le NS4150, couplage par condensateur 100 nF |
| SPEAKER_PWD | 19 | commande, via le transistor Q15, l'alimentation (VDD_3V3) de l'amplificateur |

Le NS4150 est activé en permanence dès qu'il est alimenté (broche CTRL tirée à 3,3 V) : couper l'ampli passe par GPIO19.

### Mesure batterie

`ADC_IN = 35` : GPIO35 lit **la moitié de VBAT** (pont diviseur 100 kΩ / 100 kΩ, R37/R54). La tension batterie vaut donc 2 x la tension lue. GPIO35 est sur ADC1, utilisable avec le Wi-Fi actif.

### Bus I2C et connecteur 5 broches (J52, pas 2,0 mm)

| Pin | Signal |
|---|---|
| 1 | GND |
| 2 | +5 V |
| 3 | GPIO21 (SDA), pull-up 4,7 kΩ vers 3,3 V déjà présent |
| 4 | GPIO22 (SCL), pull-up 4,7 kΩ vers 3,3 V déjà présent |
| 5 | 3,3 V (VCC_IO) |

Ne pas ajouter de pull-up supplémentaires si tu ne branches qu'un ou deux modules. Attention : GPIO21 et GPIO22 sont aussi reliés (résistances 0 Ω) aux broches LED2/SDA et LED1/SCL de l'IP5306, ce qui permet de dialoguer avec lui sur le même bus quand cette variante le supporte (adresse habituelle 0x75, à confirmer par un scan I2C). Un scan I2C est donc le premier test à faire avant de brancher un capteur.

### Autres signaux

| Signal | GPIO | Remarque |
|---|---|---|
| UART0 TX / RX | 1 / 3 | console et programmation (via le pont USB) |
| IP5306 LED3/INT | 34 | **non câblé** (R55 non monté) |
| LED verte G1 | 22 | LED non montée (R19 non monté) |
| LED d'alimentation G2 | - | fixe, alimentée en 3,3 V |

## Header 20 broches (JP1) : GPIO libres

Numérotation lue sur le schéma (SM_20PIN) ; à vérifier avec la sérigraphie de ta carte avant de câbler.

| Pin gauche | Signal | Pin droite | Signal |
|---|---|---|---|
| 1 | +5 V | 20 | VBAT |
| 2 | GND | 19 | VCC_IO (3,3 V) |
| 3 | +3,3 V | 18 | GPIO4 (rétroéclairage) |
| 4 | GPIO19 (alim ampli) | 17 | GND |
| 5 | GPIO3 (U0RXD) | 16 | GPIO0 (BOOT) |
| 6 | GPIO1 (U0TXD) | 15 | GND |
| 7 | **GPIO26** | 14 | **GPIO33** |
| 8 | GPIO32 (TFT_DC) | 13 | GPIO35 (mesure batterie) |
| 9 | GPIO34 (entrée seule) | 12 | RST/EN |
| 10 | GPIO39 / S_VN (bouton 3) | 11 | **GPIO36 / S_VP** (entrée seule) |

### Ce qui est réellement libre pour le projet

| GPIO | Usage possible | Contrainte |
|---|---|---|
| 26 | entrée/sortie numérique, ADC2 | ADC2 inutilisable avec le Wi-Fi |
| 33 | entrée/sortie numérique, ADC1 | aucune |
| 36 (S_VP) | entrée seule, ADC1 | pas de sortie, pas de pull-up interne |
| 34 | entrée seule, ADC1 | pas de sortie, pas de pull-up interne |
| 21 / 22 | I2C (boussole, capteurs) | bus partagé avec l'IP5306 |

À éviter : 16 et 17 (PSRAM), 6 à 11 (flash, non exposés), 12, 2, 15, 0, 5 (strapping), et toutes les broches utilisées par l'écran, la microSD et l'audio.

## Points ouverts

1. Vérifier sur ta carte physique la sérigraphie du header pour confirmer la numérotation des pins ci-dessus.
2. Vérifier quelle puce USB est montée (CP2104 ou CH9102F) selon le lot.
3. Scanner le bus I2C pour voir si l'IP5306 répond sur ta carte.
4. Les GPIO libres (26, 33, 36) sont peu nombreux : prévoir la place du GPS (UART) et d'un éventuel capteur de vitesse. Le GPS du téléphone, lui, passe par le Wi-Fi/BLE et ne consomme aucune broche.
