# BikeCopilot

Compteur / tableau de bord de vélo sur petites cartes ESP32 à écran : vitesse, cap, carte avec trace et destination, journal des trajets. Un seul dépôt pour plusieurs cartes, avec ce qui peut être partagé (les tuiles de carte, pour commencer).

| Dossier | Contenu | État |
|---|---|---|
| [CYD](CYD/README.md) | ESP32-2432S028 « Cheap Yellow Display » : GPS en direct, boussole, carte SD | prototype fonctionnel |
| [Lilygo-T4](Lilygo-T4/README.md) | variante LilyGO T4, position GPS reçue du téléphone (Wi-Fi ou BLE) | écrite (compile en CI), à tester sur la carte |
| [BLE-Companion](BLE-Companion/README.md) | appli Android : envoie la position GPS du téléphone aux cartes en Bluetooth LE (protocole dans [common/ble](common/ble/PROTOCOL.md)) | écrite, non testée |
| [OSM-tiles](OSM-tiles/README.md) | outils partagés : téléchargement, conversion (thèmes sombre / clair) et choix des zones de tuiles OSM | utilisable |

## Conventions

- Les commandes `python3 …` se lancent **depuis la racine du dépôt** (`python3 OSM-tiles/make_tiles.py …`). Avec PlatformIO, on indique le dossier de la variante : `pio run -d CYD -e dashboard -t upload`.
- Les tuiles sont communes à toutes les cartes : originales dans `tiles_src/`, sortie à copier sur la carte SD dans `sd/` (ces deux dossiers ne sont pas versionnés).

## Licence

GPL-3.0, voir [LICENSE](LICENSE).
