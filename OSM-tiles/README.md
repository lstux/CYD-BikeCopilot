# OSM-tiles — tuiles de carte pour les variantes BikeCopilot

Outils partagés pour préparer les tuiles OpenStreetMap lues par les cartes (CYD aujourd'hui, d'autres ensuite). Les originales sont téléchargées une fois puis gardées, et on peut les convertir autant de fois qu'on veut (thème sombre, thème clair…) sans retélécharger.

Contenu : `make_tiles.py` (téléchargement et conversion), `tiles-manager.html` (choisir les zones, compter les tuiles, générer les commandes), `Angers-OSM.sh` (exemple : les zones d'Angers).

> Toutes les commandes se lancent **depuis la racine du dépôt**. `tiles_src/` (originales) et `sd/` (sortie à copier sur la carte SD) y sont créés par défaut ; les deux sont ignorés par git.

Le script `OSM-tiles/make_tiles.py` (Python + Pillow + numpy) produit les dossiers de tuiles :

```
pip install pillow numpy
python3 OSM-tiles/make_tiles.py --gpx route.gpx --zoom 15 16 17 --out sd/   # écrit sd/tiles_dark et sd/tiles_light ; --theme dark | light | osm, un ou plusieurs
# ou une zone : --bbox LAT1 LON1 LAT2 LON2
# ou convertir des tuiles PNG déjà téléchargées : --png-dir dossier_png
```

Puis copier le contenu de `sd/` à la racine de la carte. `--dry-run` compte les tuiles (et celles déjà en cache) sans rien faire.

**Thèmes.** `dark` : fond presque noir, routes claires et hiérarchisées par couleur (rues bleu clair, secondaires jaunes, principales orange, autoroutes roses), bâtiments, eau et parcs en retrait, texte retravaillé. `light` : fond clair, routes sombres, pour le plein soleil. `osm` : couleurs d'origine (dossier `tiles_osm`, non lu par le firmware). Un dossier par thème, les deux premiers par défaut : `--theme dark light` ne lit chaque original qu'une fois. La recoloration reconnaît les couleurs standard d'OpenStreetMap ; avec un autre fournisseur, les couleurs inconnues sont simplement atténuées.

**Tuiles originales.** Les PNG téléchargés sont gardés tels quels dans `tiles_src/<fond>/<z>/<x>/<y>.png` (`--cache`, `--style`) : une tuile n'est téléchargée qu'une fois. Pour changer de thème ou de traitement sans réseau :

```
python3 OSM-tiles/make_tiles.py --bbox LAT1 LON1 LAT2 LON2 --zoom 13 14 15 16 17 --out sd/ --offline --force --theme light
```

`--offline` convertit seulement ce qui est dans le cache, `--force` réécrit les `.raw` existants. Le plafond `--max-tiles` ne compte que les tuiles **à télécharger** ; la [politique des serveurs OSM](https://operations.osmfoundation.org/policies/tiles/) interdit le téléchargement massif : pour une grande zone, utiliser un autre fournisseur (`--style NOM --url 'https://…/{z}/{x}/{y}.png'`) ou `--png-dir`.

**Choisir les zones : `OSM-tiles/tiles-manager.html`.** Page autonome (Leaflet) : on dessine des rectangles ou on charge un GPX, on coche les zooms 13 à 17, et elle affiche le nombre de tuiles et la taille sur la SD, puis les commandes `make_tiles.py` à copier (téléchargement + conversion, et reconversion depuis le cache). Elle ne télécharge aucune tuile : c'est le script qui le fait. Le fond de la page vient d'OSM, qui demande un Referer ; si la carte reste grise, servez la page en local :

```
python3 -m http.server -d OSM-tiles 8000     # puis http://localhost:8000/tiles-manager.html
```

**Exemple.** `OSM-tiles/Angers-OSM.sh` regroupe les commandes générées par la page pour l'agglomération d'Angers. Il se lance de n'importe où ; les options données au script sont passées à chaque commande, par exemple `OSM-tiles/Angers-OSM.sh --dry-run` pour compter, ou `--theme dark` pour un seul thème.

**Format des fichiers.** `tiles_<thème>/<z>/<x>/<y>.raw` : 256×256 pixels, RGB565 big-endian (131072 octets), c'est l'ordre des octets d'un sprite TFT_eSPI en 16 bits. Les cartes lisent les lignes de la tuile directement dans la mémoire du sprite.

Licence : GPL-3.0, voir [LICENSE](../LICENSE). Les données de carte sont © les contributeurs d'[OpenStreetMap](https://www.openstreetmap.org/copyright).
