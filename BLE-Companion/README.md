# BLE-Companion — pont Bluetooth LE entre le téléphone et les cartes

Appli Android qui envoie la **position GPS du téléphone** aux cartes BikeCopilot (T4, Waveshare C6…) par Bluetooth LE.
C'est l'équivalent « Bluetooth » de SensorServer (qui passe par le Wi-Fi). Pensée pour s'étendre à d'autres usages : le protocole est un standard commun défini dans [`../common/ble/PROTOCOL.md`](../common/ble/PROTOCOL.md).

> **État : première version écrite sans pouvoir la compiler** (le SDK Android n'est pas accessible là où elle a été écrite). Le protocole, lui, est testé (voir plus bas). Attends-toi à corriger quelques erreurs de compilation à la première ouverture dans Android Studio.

## Ce que fait l'appli

1. Tu appuies sur **Démarrer** ; Android demande les permissions (Bluetooth, position).
2. Un service de fond lit le GPS (1 point/s), cherche les cartes qui annoncent le service BikeCopilot, s'y connecte, puis leur écrit chaque position.
3. L'écran montre le GPS courant et l'état de chaque carte (● prête, ◌ connexion, ○ perdue, ✕ incompatible).

Rien n'est lu depuis les cartes pour l'instant (sauf *Device Info* à la connexion). Le sens carte → téléphone est réservé dans le protocole (caractéristique *Event*).

## Comment ça s'organise (le B.A.-BA d'un projet Android)

| Fichier | Rôle |
|---|---|
| `settings.gradle.kts`, `build.gradle.kts`, `app/build.gradle.kts` | **Gradle**, l'outil de compilation : quels modules, quelles versions, quelles bibliothèques. `minSdk 26` = Android 8 minimum. |
| `app/src/main/AndroidManifest.xml` | La carte d'identité de l'appli : écrans, services, **permissions** qu'elle peut demander. |
| `app/src/main/java/…/MainActivity.kt` | L'**écran** (une *Activity*). Elle ne travaille pas : elle démarre/arrête le service et affiche son état. |
| `app/src/main/res/layout/activity_main.xml` | L'aspect de l'écran, en XML (bouton + trois zones de texte). |
| `app/src/main/java/…/BridgeService.kt` | Le **pont** : un *service de premier plan* (il survit écran éteint, avec une notification permanente). Gère GPS, scan, connexions GATT, envois. |
| `app/src/main/java/…/proto/BikeProto.kt` | Le **protocole** : UUID, encodage du message GPS, lecture de *Device Info*. Pur Kotlin, sans Android : testable sur PC. |
| `app/src/main/java/…/Bridge.kt` | Le pont entre service et écran : un `StateFlow` (variable observable) que l'écran écoute. |
| `app/src/main/java/…/Permissions.kt` | Quelles permissions demander selon la version d'Android. |
| `app/src/test/…/BikeProtoTest.kt` | Test du protocole (mêmes vecteurs que le test C++ de `common/`). |

Quelques notions qui reviennent :
- **Activity / Service** : l'Activity vit tant que l'écran est affiché ; le Service continue en arrière-plan. D'où le découpage.
- **Callbacks** : le Bluetooth est asynchrone. On demande (« connecte-toi »), et Android appelle plus tard une méthode `on…` (`onConnectionStateChange`, `onServicesDiscovered`…). `BridgeService.Link` suit la séquence connexion → MTU → services → *Device Info* → prêt.
- **GATT** : le BLE organise les données en *services* contenant des *caractéristiques* (des « cases » lisibles/écrivables, repérées par un UUID). Ici : un service BikeCopilot avec *Device Info* (lecture) et *Feed* (écriture).
- **Permissions** : sans elles, ou avec le Bluetooth/GPS coupé, rien ne démarre ; c'est la source d'erreurs n° 1 en BLE.

## Compiler et installer

1. Installer [Android Studio](https://developer.android.com/studio).
2. *File → Open…* et choisir ce dossier `BLE-Companion` (pas la racine du dépôt). Android Studio télécharge Gradle et le SDK, puis « synchronise ». Si des versions sont proposées à la mise à jour (plugin Android, Kotlin…), accepter.
3. Sur le téléphone : activer le *mode développeur* et le *débogage USB* (Paramètres → À propos → toucher 7 fois le numéro de build), brancher en USB.
4. Bouton ▶ (*Run*). L'appli s'installe et se lance.

Pour lancer seulement le test du protocole : `./gradlew test` (ou clic droit sur `BikeProtoTest` → *Run*). Le fichier `gradlew` n'est pas versionné ; Android Studio le génère, sinon `gradle wrapper` dans ce dossier.

## Tester sans carte

L'appli gratuite **nRF Connect for Mobile** (sur un second téléphone) sait jouer le rôle d'une carte : créer un serveur GATT avec le service `0a2b0001-ec15-422d-8e42-7dfd3f9bff50`, ses caractéristiques *Device Info* (valeur `01 00 02 01 01`) et *Feed* (écriture sans réponse), et l'annoncer. Les messages reçus dans *Feed* se lisent en hexadécimal et se comparent aux vecteurs de [`PROTOCOL.md`](../common/ble/PROTOCOL.md).

## Limites connues de cette version

- Une seule connexion en cours d'établissement à la fois ; pas de nouvelle tentative si une connexion se bloque (Android la fait tomber de lui-même au bout d'une trentaine de secondes, puis le scan reprend).
- Si le Bluetooth ou le GPS est coupé pendant le fonctionnement, rien n'est signalé : arrêter puis redémarrer.
- Textes de l'interface en dur (français), pas d'icône d'appli, pas de thème personnalisé.
- Les écritures sont « sans réponse » : on ne sait pas si la carte a bien reçu chaque position.
