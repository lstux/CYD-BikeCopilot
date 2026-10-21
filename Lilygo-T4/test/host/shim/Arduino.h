#pragma once
// Mini-shim Arduino pour compiler main.cpp de la T4 sur PC (simulateur d'interface).
// Le temps (millis/delay) et l'état des broches sont pilotés par sim_main.cpp.
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846

// ESP.getFreeHeap() (diagnostic) : valeur fictive sur PC
struct ESPShim {
  uint32_t getFreeHeap() const { return 200000; }
};
static ESPShim ESP;
#endif
#define INPUT 0
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1

extern uint32_t simMillis;
extern int simPin[64];
static inline uint32_t millis() { return simMillis; }
static inline void delay(uint32_t ms) { simMillis += ms; }
static inline void pinMode(int, int) {}
static inline int digitalRead(int pin) { return simPin[pin]; }

struct SerialShim {
  void begin(int) {}
  void println(const char *s) { ::printf("[serial] %s\n", s); }
  void printf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list a;
    va_start(a, fmt);
    ::printf("[serial] ");
    ::vprintf(fmt, a);
    va_end(a);
  }
};
extern SerialShim Serial;
