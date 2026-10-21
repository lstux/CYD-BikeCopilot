// Test d'écran isolé (sans Bluetooth, sans logique de page) : rouge, vert, bleu, blanc, noir,
// puis le nom du pilote écrit à l'écran. Sert à savoir si l'écran répond, et avec quel pilote.
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

#ifdef ST7789_DRIVER
#define DRIVER_NAME "ST7789"
#else
#define DRIVER_NAME "ILI9341"
#endif

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  Serial.println("[screen] pilote " DRIVER_NAME);
  const uint16_t c[5] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
  for (int i = 0; i < 5; i++) {
    tft.fillScreen(c[i]);
    delay(700);
  }
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(3);
  tft.setCursor(10, 100);
  tft.print("Pilote: " DRIVER_NAME);
  Serial.println("[screen] fin du test de couleurs");
}

void loop() {
  delay(1000);
  Serial.println("[screen] ...");
}
