#pragma once
// Mini-shim Arduino pour le simulateur d'interface sur PC.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static inline uint32_t millis() { return 0; }
