#pragma once
// Minimal Arduino compatibility shim for compiling animations.h with Emscripten.
// Keep animations.h unchanged; this header provides only what's needed.

#include <stdint.h>
#include <math.h>

template <typename T>
static inline T constrain(T x, T a, T b) {
  return x < a ? a : (x > b ? b : x);
}
