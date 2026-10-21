# common — code partagé entre les cartes

Code C++ (en-têtes) sans dépendance matérielle directe, utilisé par toutes les variantes (CYD, T4…).
Chaque projet PlatformIO l'ajoute à son chemin d'inclusion (`-I$PROJECT_DIR/../common/src`).

## `src/`
| Fichier | Rôle |
|---|---|
| `gfx.h`, `fonts_data.h` | couche de dessin anticrénelé au-dessus de TFT_eSPI, polices Inter en bitmaps |
| `theme.h` | couleurs |
| `hud.h`, `page_gauge.h`, `page_compass.h`, `page_map.h`, `page_trips.h` | pages : dessin pur à partir de données |
| `fix.h` | struct `Fix` : position, vitesse, cap, altitude, satellites, heure… indépendante de la source |
| `geo.h` | projection Web Mercator, distance et cap entre deux points |
| `tiles.h` | cache de tuiles `.raw` sur SD (`<dossier>/<z>/<x>/<y>.raw`) |
| `track.h` | trace GPX/CSV sur SD |
| `trips.h`, `tripdata.h` | enregistrement et statistiques des sorties |
| `settings.h` | réglages mémorisés en flash (`Preferences`) ; lit `TZ_OFFSET_H` et `GAUGE_MIN_MAX` dans le `config.h` de la carte |
| `mappage.h` | page carte : tuiles, trace, position, guidage, zoom/déplacement (`mapSetZoom`, `mapZoomStep`, `mapDrag`, `mapRecenter`…) |
| `histpage.h` | page Trajets : liste, détail, courbes, actions (`histOpen`, `histBack`, `histScroll`, `histAskDelete`, `histAnswerDelete`, `histSetDestFromTrip`) |

## Pages « à inclure »
`mappage.h` et `histpage.h` sont inclus par le `main.cpp` de la carte, qui doit avoir défini avant :
`tft`, `spr`, `fix`, `cfg`, `trip`, `compass` (si présente), `sdOk` (SD montée), `mapLight`, `destLat/destLon`,
`currentKmh()`, `pollGps()`, `chooseDest()`, `scr()`. Elles ne lisent aucune entrée : le tactile ou les boutons
appellent leurs fonctions d'action (voir `CYD/src/dashboard/histtouch.h` pour un exemple).

## `tools/`
- `make_fonts.py` : régénère `src/fonts_data.h` (`python3 common/tools/make_fonts.py`).

## Hors de `common/` (propre à chaque carte)
Configuration des broches (`config.h`), SD (`sdSPI`), entrées (tactile ou boutons : `touch.h`, `histtouch.h`), page Réglages (`settingspage.h`, propre aux capteurs de la carte), luminosité (`maptheme.h`), `main.cpp`, et l'adaptateur qui remplit la `Fix` :
- CYD : `CYD/src/dashboard/gpsfix.h` (TinyGPSPlus → `Fix`)
- T4 : à écrire (position reçue du téléphone → `Fix`, en incrémentant `seq` à chaque point)
