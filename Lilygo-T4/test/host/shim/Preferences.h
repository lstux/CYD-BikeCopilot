#pragma once
// Préférences factices : rien n'est mémorisé (identique à celui du simulateur CYD).
#include <Arduino.h>
class Preferences {
 public:
  bool begin(const char *, bool = false) { return true; }
  void end() {}
  bool getBool(const char *, bool d = false) { return d; }
  int getInt(const char *, int d = 0) { return d; }
  double getDouble(const char *, double d = 0) { return d; }
  size_t putBool(const char *, bool) { return 1; }
  size_t putInt(const char *, int) { return 1; }
  size_t putDouble(const char *, double) { return 1; }
};
