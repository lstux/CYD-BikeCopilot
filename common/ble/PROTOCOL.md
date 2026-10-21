# Protocole BLE BikeCopilot — version 1.0

Standard commun entre l'appli Android [BLE-Companion](../../BLE-Companion/README.md) et les cartes (T4, Waveshare C6, CYD…).
Les cartes ne font que **s'annoncer et recevoir** ; le téléphone est le *central* : il scanne, se connecte, et écrit.

Implémentation de référence côté carte : [`../src/ble_proto.h`](../src/ble_proto.h) (UUID, décodage → `Fix`).
Test : `g++ -std=c++17 -I common/src common/tools/ble_proto_test.cpp -o /tmp/ble_proto_test && /tmp/ble_proto_test`.

## Rôles

| Rôle | Qui | Ce qu'il fait |
|---|---|---|
| Central | le téléphone | scanne le service BikeCopilot, se connecte (plusieurs cartes possibles), lit *Device Info*, écrit dans *Feed* |
| Périphérique | la carte | annonce le service, expose 3 caractéristiques, traite ce qu'on écrit dans *Feed* |

## Service et caractéristiques

Toutes les valeurs sont dans la même famille d'UUID 128 bits : `0a2b00NN-ec15-422d-8e42-7dfd3f9bff50`.

| NN | Nom | UUID | Propriétés | Sens | Contenu |
|---|---|---|---|---|---|
| 01 | **Service BikeCopilot** | `0a2b0001-ec15-422d-8e42-7dfd3f9bff50` | — | — | à mettre dans l'annonce (advertising) pour que le téléphone filtre le scan |
| 02 | **Device Info** | `0a2b0002-ec15-422d-8e42-7dfd3f9bff50` | Read | carte → téléphone | version du protocole, type de carte, liste des messages acceptés |
| 03 | **Feed** | `0a2b0003-ec15-422d-8e42-7dfd3f9bff50` | Write (sans réponse) | téléphone → carte | messages (voir plus bas) |
| 04 | **Event** | `0a2b0004-ec15-422d-8e42-7dfd3f9bff50` | Notify | carte → téléphone | *réservé* : boutons, capteurs de la carte, accusés… Non utilisé en v1, mais la caractéristique doit exister pour que les cartes déjà flashées n'aient pas à changer d'UUID plus tard |

Le nom GAP de la carte (affiché dans l'appli) est libre, par exemple `BikeCopilot-T4`. Le téléphone **ne se fie pas au nom** : seul l'UUID du service compte.

## Règles générales

- **Little-endian** partout (natif ESP32 et Android).
- **MTU** : le téléphone demande 247 octets juste après la connexion. Une carte doit accepter au minimum des écritures de **22 octets** (MTU ≥ 25, car l'en-tête ATT prend 3 octets). En dessous, le téléphone affiche « carte incompatible ».
- **Une écriture = un message.** Pas de fragmentation en v1.
- **Compatibilité future : on ne fait qu'*ajouter* à la fin.** Un récepteur lit les champs qu'il connaît d'après la longueur annoncée et **ignore les octets en trop**. Si le message est plus court que prévu (ancienne appli), les champs manquants valent « inconnu ». Un champ existant ne change jamais de sens ni de taille ; si c'est indispensable, on change de numéro de type.
- **Pas de sécurité en v1** (pas d'appairage, aucune donnée secrète n'est échangée). Piste ultérieure : liaison (bonding) + refus d'une 2ᵉ connexion centrale.

## Format d'un message (écrit dans *Feed*)

| Octet | Contenu |
|---|---|
| 0 | `type` (voir registre) |
| 1 | `len` : nombre d'octets de charge utile qui suivent |
| 2… | charge utile (`len` octets) |

### Registre des types

| Type | Sens | Nom | État |
|---|---|---|---|
| `0x00` | — | réservé | — |
| `0x01` | téléphone → carte | **GPS_FIX** | défini (v1) |
| `0x02` | téléphone → carte | DEST (destination : latitude, longitude, nom) | prévu |
| `0x03` | téléphone → carte | TIME (heure, fuseau) | prévu |
| `0x04`–`0x3F` | téléphone → carte | libres (météo, notifications, cadence…) | libre |
| `0x40`–`0x7F` | carte → téléphone | libres (messages d'*Event*) | libre |
| `0x80`–`0xFF` | — | expérimental, propre à un projet (non standardisé) | libre |

Un type inconnu est **ignoré** sans erreur (et la connexion reste ouverte).

## `0x01` GPS_FIX — position du téléphone

Charge utile v1 : **20 octets** (message complet : 22 octets).

| Offset | Taille | Type | Champ | Unité / codage | « Inconnu » |
|---|---|---|---|---|---|
| 0 | 4 | int32 | `lat` | degrés × 10⁷ | — (message sans position = ne pas l'envoyer) |
| 4 | 4 | int32 | `lon` | degrés × 10⁷ | — |
| 8 | 2 | uint16 | `speed` | 0,1 km/h | `0xFFFF` |
| 10 | 2 | uint16 | `course` | 0,1 °, 0 = nord, sens horaire (0…3599) | `0xFFFF` |
| 12 | 2 | int16 | `alt` | mètres au-dessus du niveau de la mer | `-32768` (`0x8000`) |
| 14 | 1 | uint8 | `acc` | précision horizontale, mètres (rayon à 68 %) | `255` |
| 15 | 1 | uint8 | `sats` | satellites utilisés | `0` |
| 16 | 4 | uint32 | `utc` | secondes Unix UTC (GPS, pas l'horloge du téléphone si possible) | `0` |

### Rythme et péremption
- Le téléphone envoie **une position par seconde** tant qu'il en reçoit une nouvelle (jamais plus vite : la carte n'en a pas besoin).
- Côté carte, la `Fix` passe à `valid = false` si aucun message n'est arrivé depuis **5 s** (connexion perdue, tunnel…).
- Une position que le téléphone sait périmée n'est pas envoyée : le silence suffit.

### Correspondance avec la `Fix` de `common/src/fix.h`
| `Fix` | Source |
|---|---|
| `lat`, `lon` | `lat`/`lon` ÷ 10⁷ |
| `kmh`, `speedValid` | `speed` ÷ 10 ; valide si ≠ `0xFFFF` |
| `course`, `courseValid` | `course` ÷ 10 ; valide si ≠ `0xFFFF` |
| `alt`, `altValid` | `alt` ; valide si ≠ `-32768` |
| `sats` | `sats` |
| `hdop`, `hdopValid` | `acc` ÷ 5 (approximation : précision ≈ HDOP × 5 m) ; valide si `acc` ≠ 255 |
| `hour`…`day`, `timeValid`, `dateValid` | `utc` décomposé en UTC ; valide si ≠ 0 |
| `seq` | incrémenté à chaque message reçu |
| `valid` | `true` à la réception, `false` après 5 s de silence |

## `Device Info` (lecture)

| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | version majeure du protocole (`1`) |
| 1 | 1 | version mineure (`0`) |
| 2 | 1 | type de carte : `0` inconnu, `1` CYD, `2` Lilygo T4 v1.3, `3` Waveshare C6 1.47″, `255` autre |
| 3 | 1 | `n` : nombre de types de message acceptés dans *Feed* |
| 4… | `n` | liste des types acceptés (ex. `01` pour GPS_FIX) |

Le téléphone n'envoie à une carte que les types qu'elle déclare. Une version **majeure** différente de celle que connaît l'appli → l'appli ne se connecte pas et le dit.

## Vecteurs de test

Message GPS_FIX complet (22 octets) pour : lat 48,8566°, lon 2,3522°, 23,4 km/h, cap 187,5°, 35 m, précision 6 m, 12 satellites, 2026-10-21 10:35:07 UTC.

```
01 14 f0 ec 1e 1d d0 ea 66 01 ea 00 53 07 23 00 06 0c 5b 95 d8 6a
```

Même position, tout le reste inconnu :

```
01 14 f0 ec 1e 1d d0 ea 66 01 ff ff ff ff 00 80 ff 00 00 00 00 00
```

Ces deux vecteurs sont repris tels quels dans le test C++ et dans le test Kotlin de l'appli.
