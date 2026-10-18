# CYD-BikeCopilot

Compteur / tableau de bord de vélo sur une carte **ESP32-2432S028 (« Cheap Yellow Display »)** avec un module GPS et une boussole QMC5883.

> État : prototype. Le câblage (GPS, I2C, boussole) est validé sur le matériel. La jauge et la boussole fonctionnent ; la page carte (SD, tuiles, trace GPX) est écrite mais pas encore testée sur le matériel.

## Fonctions

| Page | Contenu | État |
|---|---|---|
| 1. Compteur | jauge de vitesse à échelle adaptative, distance, temps roulant, moyenne, max, heure | codée |
| 2. Direction | boussole tournante (haut = sens de marche), flèche vers la destination, distance à vol d'oiseau | codée |
| 3. Carte | tuiles OSM depuis la carte SD, centrée sur la position, trace GPX, prochain virage, distance restante | codée, non testée |
| 4. Trajets | liste des trajets enregistrés, détail avec stats et courbes vitesse / altitude | codée, non testée |
| 5. Réglages | journal on/off, fuseau horaire, échelle mini de la jauge, fin de trajet, calibrations | codée, non testée |

### Tactile

- **Glisser** gauche / droite : page suivante / précédente.
- **Page carte** : toucher le tiers gauche = dézoomer, le tiers droit = zoomer.
- **Pages compteur et direction** : appui long (~1 s) = fenêtre « Remettre à zéro ? » (confirmation).
- **Page trajets** : toucher un trajet pour voir le détail (toucher l'en-tête ou glisser pour revenir) ; flèches ▲ ▼ pour défiler.
- **Page réglages** : toucher les boutons (journal, fuseau, échelle de la jauge, terminer le trajet, calibrations).
- **Calibration** : automatique au premier démarrage (toucher 3 croix : haut-gauche, haut-droite, bas-droite), mémorisée. Pour recommencer : appui de plus de 4 s sur BOOT.
- Le tactile (XPT2046, IO25/32/39/33/36) est lu en SPI logiciel, car l'écran et la SD occupent déjà les deux bus matériels.

Bouton BOOT : appui court = page suivante, appui long (> 1 s) = remise à zéro du trajet (sur la page carte : changement de zoom 15 / 16 / 17).

### Journal des trajets

Avec une carte SD, les trajets sont enregistrés automatiquement : le départ est détecté au mouvement (voir le filtrage de vitesse), un point est relevé toutes les 2 s, l'écriture sur la SD se fait toutes les 30 s. Un mouvement de moins de 30 s est jeté. Quand on s'arrête plus de 60 s l'enregistrement se met en pause, et après 5 min d'arrêt le trajet est terminé (ou via « Terminer » dans les réglages). Un trajet de moins de 100 m est supprimé. Un point rouge dans la barre du haut indique un trajet en cours.

```
/trips/AAAAMMJJ_HHMMSS.csv   t (epoch UTC), lat, lon, alt, km/h, satellites, hdop
/trips/index.csv             une ligne de statistiques par trajet terminé
```

Après une coupure de courant en plein trajet, le fichier est retrouvé et indexé au démarrage suivant. Pour exporter : `python3 tools/csv_to_gpx.py trips/xxx.csv` produit un GPX (Strava, Komoot…). Le dénivelé est calculé sur une altitude lissée avec un seuil de 3 m, car l'altitude GPS est bruitée.

### Filtrage de la vitesse

À l'arrêt, le GPS dérive et annonce parfois 2 à 4 km/h. La vitesse est donc mise à 0 tant qu'on n'a pas dépassé 5 km/h pendant 2 s avec un signal correct (≥ 4 satellites, HDOP ≤ 3) ; on repasse à l'arrêt sous 3 km/h. En mouvement, un lissage léger (~0,7 s) calme l'aiguille. La distance, le temps roulant et le max n'utilisent que cette vitesse filtrée. Réglages `SPEED_*` dans `config.h` ; l'environnement `dashboard-walk` utilise des seuils plus bas (2 / 1,2 km/h) pour la marche.

### Échelle de la jauge

L'échelle part de 15 km/h. Dès que la vitesse la dépasse, elle passe au palier de 5 au-dessus (20, 25, 30…). Elle ne redescend d'un cran qu'après 8 s nettement sous le palier inférieur (moins de 80 % de sa valeur), pour éviter les sauts. Réglages dans `src/dashboard/config.h` (`GAUGE_*`). Pour tester à pied : `pio run -e dashboard-walk -t upload` (la jauge démarre à 5 km/h).

## Page carte : préparer la carte SD

La carte microSD (FAT32) contient :

```
/tiles/<zoom>/<x>/<y>.raw   tuiles 256×256 en RGB565 (131072 octets), zooms 15, 16, 17
/route.gpx                  trace à suivre (optionnelle) ; son dernier point devient la destination
```

Le script `tools/make_tiles.py` (Python + Pillow) produit ce dossier :

```
pip install pillow
python3 tools/make_tiles.py --gpx route.gpx --zoom 15 16 17 --out sd/
# ou une zone : --bbox LAT1 LON1 LAT2 LON2
# ou convertir des tuiles PNG déjà téléchargées : --png-dir dossier_png
```

Puis copier le contenu de `sd/` à la racine de la carte. `--dry-run` compte les tuiles sans rien faire.
Le téléchargement est plafonné (`--max-tiles`) : la [politique des serveurs OSM](https://operations.osmfoundation.org/policies/tiles/) interdit le téléchargement massif ; pour une grande zone, utiliser un autre fournisseur ou `--png-dir`.
Sans `route.gpx`, la carte s'affiche quand même, centrée sur la position, avec la destination de `config.h`.

La SD est sur le bus VSPI (IO5/18/19/23), l'écran sur HSPI (`-DUSE_HSPI_PORT`).

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
