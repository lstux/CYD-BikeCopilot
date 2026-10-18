#pragma once
// Mini pilote QMC5883L (0x0D) / QMC5883P (0x2C), sans dépendance externe.
// Cap à plat uniquement (pas de compensation d'inclinaison), calibration min/max à chaud.
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

class Compass {
 public:
  bool begin() {
    if (probe(0x0D)) {
      addr_ = 0x0D;
      isP_ = false;
      write8(0x0B, 0x01);  // période SET/RESET
      write8(0x09, 0x1D);  // OSR512, ±8G, 200 Hz, continu
      return true;
    }
    if (probe(0x2C)) {
      addr_ = 0x2C;
      isP_ = true;
      write8(0x29, 0x06);  // réglage recommandé constructeur
      write8(0x0B, 0x08);  // ±8G, SET/RESET on
      write8(0x0A, 0xCF);  // continu, 200 Hz, OSR 8
      return true;
    }
    return false;
  }

  bool ok() const { return addr_ != 0; }
  const char *name() const { return addr_ == 0x0D ? "QMC5883L" : addr_ == 0x2C ? "QMC5883P" : "aucun"; }

  // Met à jour la lecture ; retourne true si le cap est valide
  bool update() {
    if (!addr_) return false;
    uint8_t buf[6];
    uint8_t reg = isP_ ? 0x01 : 0x00;
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr_, 6) != 6) return false;
    for (int i = 0; i < 6; i++) buf[i] = Wire.read();
    int16_t x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t y = (int16_t)(buf[2] | (buf[3] << 8));
    // buf[4..5] = Z, inutilisé

    if (x < minX_) minX_ = x;
    if (x > maxX_) maxX_ = x;
    if (y < minY_) minY_ = y;
    if (y > maxY_) maxY_ = y;

    float rx = x - (minX_ + maxX_) / 2.0f;
    float ry = y - (minY_ + maxY_) / 2.0f;
    float sx = (maxX_ - minX_) / 2.0f;
    float sy = (maxY_ - minY_) / 2.0f;
    if (sx < 50 || sy < 50) return false;  // pas assez de rotation pour calibrer
    rx /= sx;
    ry /= sy;

    float h = atan2f(ry, rx) * 180.0f / (float)M_PI;
    heading_ = h;
    valid_ = true;
    return true;
  }

  bool calibrated() const { return valid_; }
  // Cap en degrés 0..360 (0 = nord), après offset/inversion
  float heading(float offsetDeg, bool invert) const {
    float h = invert ? -heading_ : heading_;
    h += offsetDeg;
    while (h < 0) h += 360;
    while (h >= 360) h -= 360;
    return h;
  }
  void resetCalibration() {
    minX_ = minY_ = 32767;
    maxX_ = maxY_ = -32768;
    valid_ = false;
  }

 private:
  bool probe(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
  }
  void write8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  uint8_t addr_ = 0;
  bool isP_ = false;
  bool valid_ = false;
  float heading_ = 0;
  int16_t minX_ = 32767, maxX_ = -32768, minY_ = 32767, maxY_ = -32768;
};
