# BikeCopilot — LilyGO TTGO T4 v1.3 (ESP32)

Version ESP32 d'origine (pas la S3).

Matériel : ESP32, écran ILI9341 320×240 (comme la CYD), 4 boutons (3 utilisateur + BOOT, pas de tactile), lecteur microSD, 8 Mo de PSRAM. Brochage et caractéristiques : [`Hardware.md`](Hardware.md).
Position : fournie par le téléphone (Wi-Fi, appli Android **SensorServer**), pas de GPS embarqué.

Le code d'affichage vient de [`../common`](../common/README.md).

> **État : première version, jamais compilée ni testée sur la carte.** Le code a été écrit sans chaîne PlatformIO/ESP32 (voir « Vérifications faites » plus bas). Prévoir quelques corrections au premier `pio run`.

## Ce qui est fait

| Page | Contenu | État |
|---|---|---|
| 1. Compteur | jauge de vitesse à échelle adaptative (même code que la CYD), distance, temps roulant, dénivelé, moyenne, max, altitude | codée |
| 2. Liaison | état du Wi-Fi, de SensorServer et de la dernière position reçue | codée |

Pas encore : boussole (la T4 n'a pas de capteur, le cap vient du téléphone), carte, trajets, réglages, carte SD.

## Utilisation

1. Téléphone : installer SensorServer (F-Droid), autoriser la localisation, démarrer le serveur (port 8080 par défaut).
2. Premier démarrage de la carte : aucun Wi-Fi mémorisé, elle crée son réseau **BikeCopilot-XXXX** (mot de passe `bikecopilot`, affiché sur la page Liaison). Connecter le téléphone dessus, ouvrir `http://192.168.4.1/`, saisir le Wi-Fi (ou renseigner le partage de connexion du téléphone) et le port de SensorServer. La carte redémarre et retrouve SensorServer toute seule (dernier hôte, passerelle, mDNS, balayage).
3. Les réseaux sont mémorisés ; le portail reste accessible à l'IP de la carte.

### Boutons (provisoire, à adapter à leur position réelle)

| Bouton | GPIO | Action |
|---|---|---|
| BTN1 | 37 | appui court : page précédente |
| BTN2 | 38 | appui court : page suivante |
| BTN3 | 39 | appui long (page Compteur) : remise à zéro du trajet, confirmée par un second appui long ; un autre bouton annule |
| BTN4 (BOOT) | 0 | appui de 5 s : oublier la configuration Wi-Fi et redémarrer |

## Compilation

```
cd Lilygo-T4
pio run -t upload
pio device monitor
```

Environnements : `dashboard` (par défaut) et `dashboard-walk` (jauge démarrant à 5 km/h, pour tester à pied).
Plateforme `espressif32@6.9.0` (Arduino-ESP32 2.0.x, comme la CYD : TFT_eSPI n'est pas fiable avec le core 3.x), partition `huge_app`.

À régler au premier essai selon l'écran :
- `TFT_ROTATION` dans `src/dashboard/config.h` (1 ou 3 si l'image est à l'envers) ;
- `SPI_FREQUENCY` dans `platformio.ini` (40 MHz ici, LilyGO propose 60 MHz) ;
- `TZ_OFFSET_H` (heure locale) dans `config.h` : pas encore de page Réglages.

La PSRAM n'est pas activée pour l'instant (aucun besoin, sprites plus rapides en RAM interne) ; elle servira au cache de tuiles de la page carte.

## Organisation

```
src/dashboard/
  main.cpp        pages, boutons, boucle (cœur 1)
  config.h        broches, seuils de vitesse et de jauge
  phonefix.h      position du téléphone -> Fix (équivalent de gpsfix.h de la CYD)
  speed.h         vitesse filtrée, statistiques du trajet, échelle de la jauge
  buttons.h       antirebond, appui court / long / très long
  status.h        barre d'état (précision du point au lieu du nombre de satellites)
  phonelink.*     tâche Wi-Fi + WebSocket SensorServer (cœur 0)
  phone/          portail captif, mémorisation des réseaux, recherche de SensorServer
test/host/        tests et simulateur sur PC (voir ci-dessous)
```

`phone/` et `phonelink.cpp` viennent du firmware `phone-bridge` du dépôt [Waveshare-C6-1.47](https://github.com/lstux/Waveshare-C6-1.47), adaptés : variables renommées pour ne pas entrer en conflit avec les `cfg` du code commun, `MDNS.IP()` pour le core 2.0.x, tâche FreeRTOS. Les corrections faites d'un côté sont à reporter de l'autre tant que le code n'est pas partagé.

`speed.h` reprend la logique du `main.cpp` de la CYD (vitesse, trajet, jauge), sans satellites ni HDOP : le téléphone ne fournit qu'une précision en mètres. Cette logique pourrait migrer dans `common/` pour la CYD aussi.

## Vérifications faites (sur PC, sans la carte)

```
Lilygo-T4/test/host/run.sh       # tests de la logique pure (conversion Fix, vitesse, trajet, jauge, boutons)
Lilygo-T4/test/host/run_sim.sh   # compile le vrai main.cpp avec de fausses bibliothèques et sort des captures d'écran
```

Ces tests ne couvrent ni le Wi-Fi, ni le WebSocket, ni le portail, ni TFT_eSPI réel, ni les broches : tout cela reste à valider sur la carte.
