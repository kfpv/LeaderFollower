#pragma once
// Minimal Arduino compatibility shim for compiling animations.h with Emscripten.
// Keep animations.h unchanged; this header provides only what's needed.

#include <stdint.h>
#include <math.h>
#include <string.h>

template <typename T>
static inline T constrain(T x, T a, T b) {
  return x < a ? a : (x > b ? b : x);
}

// Arduino PROGMEM compatibility for Emscripten
#define PROGMEM
#define memcpy_P(dest, src, n) memcpy((dest), (src), (n))
