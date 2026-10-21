# BikeCopilot — LilyGO TTGO T4 v1.3 (ESP32)

À faire. Version ESP32 d'origine (pas la S3).

Matériel : ESP32, écran ILI9341 320×240 (comme la CYD), 3 boutons utilisateur (pas de tactile), lecteur microSD, 8 Mo de PSRAM.
Position : fournie par le téléphone (Wi-Fi / BLE), pas de GPS embarqué. Côté BLE : appli [BLE-Companion](../BLE-Companion/README.md), protocole dans [`common/ble/PROTOCOL.md`](../common/ble/PROTOCOL.md) (firmware à écrire : périphérique BLE + `bleHandleFeedWrite()`).

Le code d'affichage vient de [`../common`](../common/README.md).
