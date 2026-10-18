# CYD-BikeCopilot

Compteur / tableau de bord de vélo sur une carte **ESP32-2432S028 (« Cheap Yellow Display »)** avec un module GPS et une boussole QMC5883.

> État : prototype. Le code n'a pas encore été testé sur le matériel.

## Fonctions

| Page | Contenu | État |
|---|---|---|
| 1. Compteur | vitesse, distance, moyenne, temps roulant, vitesse max, heure | codée |
| 2. Direction | flèche vers une destination, distance à vol d'oiseau, cap GPS / boussole | codée |
| 3. Carte | tuiles OSM depuis la carte SD, trace GPX, prochain virage | à faire |

Bouton BOOT : appui court = page suivante, appui long (> 1 s) = remise à zéro du trajet.

## Matériel

- ESP32-2432S028 (ESP32-WROOM-32, écran ILI9341 320×240, lecteur microSD)
- Module GPS (type « Cyclone » AliExpress, trames NMEA à 38400 bauds)
- Boussole QMC5883P (`0x2C`, le QMC5883L en `0x0D` est aussi géré)

### Câblage

| Signal | GPIO | Remarque |
|---|---|---|
| GPS TX → carte | IO35 | entrée seule, connecteur P3 |
| QMC5883 SDA | IO27 | connecteur CN1 |
| QMC5883 SCL | IO22 | connecteur CN1 |
| 3V3 / GND | | communs |

IO21 est le rétroéclairage de l'écran, à ne pas réutiliser.

## Utilisation

Avec [PlatformIO](https://platformio.org/) :

```
pio run -e dashboard -t upload
pio device monitor
```

Réglages dans `src/dashboard/config.h` : destination (`DEST_LAT`, `DEST_LON`), fuseau horaire (`TZ_OFFSET_H`, 2 en été et 1 en hiver), orientation de la boussole.

### Tests de câblage

- `pio run -e i2cscan -t upload` : scanne le bus I2C (SDA=27 / SCL=22, puis l'inverse). Attendu : `0x0D` ou `0x2C`.
- `pio run -e nmea -t upload` : recopie les trames NMEA reçues sur IO35 (essaie 38400, 9600, 115200, 4800 bauds).

## Limites connues

- Boussole : calibration à chaud (tourner la carte sur elle-même), non sauvegardée, cap à plat sans compensation d'inclinaison.
- Initialisation du QMC5883P écrite d'après la datasheet : le module répond en `0x2C`, mais la lecture du cap reste à valider.
- IO35 est en entrée seule (réception uniquement) : pas de configuration du GPS possible (cadence, constellations) sans souder un fil sur une autre broche.

## Licence

GPL-3.0, voir [LICENSE](LICENSE).
