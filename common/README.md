# common — code partagé entre les cartes

Code C++ (en-têtes) sans dépendance matérielle directe, utilisé par toutes les variantes (CYD, T4…).
Chaque projet PlatformIO l'ajoute à son chemin d'inclusion (`-I$PROJECT_DIR/../common/src`).

## `src/`
| Fichier | Rôle |
|---|---|
| `gfx.h`, `fonts_data.h` | couche de dessin anticrénelé au-dessus de TFT_eSPI, polices Inter en bitmaps |
| `theme.h` | couleurs |
| `hud.h`, `page_gauge.h`, `page_compass.h`, `page_map.h`, `page_trips.h` | pages : dessin pur à partir de données |
| `geo.h` | projection Web Mercator, distance entre deux points |
| `tiles.h` | cache de tuiles `.raw` sur SD (`<dossier>/<z>/<x>/<y>.raw`) |
| `track.h` | trace GPX/CSV sur SD |
| `trips.h`, `tripdata.h` | enregistrement et statistiques des sorties |

## `tools/`
- `make_fonts.py` : régénère `src/fonts_data.h` (`python3 common/tools/make_fonts.py`).

## Hors de `common/` (propre à chaque carte)
Configuration des broches (`config.h`), entrées (tactile ou boutons), source de position, luminosité, `main.cpp`.
