#!/bin/sh

# Zone Angers-Agglo (30 + 108 tuiles)
python3 tools/make_tiles.py --bbox 47.41705 -0.65248 47.54247 -0.41182 --zoom 13 --out 'sd/'
python3 tools/make_tiles.py --bbox 47.41705 -0.65248 47.54247 -0.41182 --zoom 14 --out 'sd/'

# Zone StBarth-Trélazé (36 + 132 tuiles)
python3 tools/make_tiles.py --bbox 47.43622 -0.51361 47.4758 -0.45387 --zoom 15 --out 'sd/'
python3 tools/make_tiles.py --bbox 47.43622 -0.51361 47.4758 -0.45387 --zoom 16 --out 'sd/'
# Zone Angers (70 + 252 tuiles)
python3 tools/make_tiles.py --bbox 47.44132 -0.5821 47.50259 -0.50915 --zoom 15 --out 'sd/'
python3 tools/make_tiles.py --bbox 47.44132 -0.5821 47.50259 -0.50915 --zoom 16 --out 'sd/'


# Zone StBarth (273 tuiles)
python3 tools/make_tiles.py --bbox 47.45189 -0.51619 47.47278 -0.45885 --zoom 17 --out 'sd/'
# Zone Angers1 (323 tuiles)
python3 tools/make_tiles.py --bbox 47.46686 -0.55155 47.49679 -0.50022 --zoom 17 --out 'sd/'
# Zone Angers2 (342 tuiles)
python3 tools/make_tiles.py --bbox 47.44934 -0.56854 47.4802 -0.51704 --zoom 17 --out 'sd/'
