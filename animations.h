#pragma once
#include <Arduino.h>

namespace Anim {
static constexpr uint8_t BRANCHES = 4;
static constexpr uint8_t LEDS_PER_BRANCH = 7;
static constexpr uint8_t TOTAL_LEDS = BRANCHES * LEDS_PER_BRANCH;

inline void staticOn(float t, uint16_t n, float level, float *out) {
  level = constrain(level, 0.0f, 1.0f);
  for (uint16_t i = 0; i < n; ++i) out[i] = level;
}

inline void wave(float t, uint16_t n, float speed, float phase, bool branchMode, bool invert, float *out) {
  const float twoPi = 6.28318530718f;
  if (branchMode) {
    for (uint8_t b = 0; b < BRANCHES; ++b) {
      float bp = phase + b * 0.78539816339f; // pi/4
      for (uint8_t i = 0; i < LEDS_PER_BRANCH; ++i) {
        uint16_t idx = b * LEDS_PER_BRANCH + i;
        if (idx >= n) break;
        uint8_t ii = invert ? (uint8_t)(LEDS_PER_BRANCH - 1 - i) : i;
        float angle = (float)ii / (float)LEDS_PER_BRANCH * twoPi + t * speed + bp;
        float v = 0.5f + 0.5f * sinf(angle);
        out[idx] = v;
      }
    }
  } else {
    for (uint16_t i = 0; i < n; ++i) {
      uint16_t ii = invert ? (uint16_t)(n - 1 - i) : i;
      float angle = (float)ii / (float)n * twoPi + t * speed + phase;
      out[i] = 0.5f + 0.5f * sinf(angle);
    }
  }
}

inline void pulse(float t, uint16_t n, float speed, float phase, bool branchMode, float *out) {
  if (branchMode) {
    for (uint8_t b = 0; b < BRANCHES; ++b) {
      float bp = phase + b * 1.57079632679f; // pi/2
      float v = 0.5f + 0.5f * sinf(t * speed + bp);
      for (uint8_t i = 0; i < LEDS_PER_BRANCH; ++i) {
        uint16_t idx = b * LEDS_PER_BRANCH + i;
        if (idx >= n) break;
        out[idx] = v;
      }
    }
  } else {
    float v = 0.5f + 0.5f * sinf(t * speed + phase);
    for (uint16_t i = 0; i < n; ++i) out[i] = v;
  }
}

inline void chase(float t, uint16_t n, float speed, uint8_t width, bool branchMode, float *out) {
  for (uint16_t i = 0; i < n; ++i) out[i] = 0.0f;
  if (branchMode) {
    for (uint8_t b = 0; b < BRANCHES; ++b) {
      uint16_t pos = (uint16_t)(fmodf(t * speed + b * (LEDS_PER_BRANCH / 2.0f), (float)LEDS_PER_BRANCH));
      for (uint8_t w = 0; w < width; ++w) {
        uint16_t idx = b * LEDS_PER_BRANCH + (pos + w) % LEDS_PER_BRANCH;
        if (idx < n) out[idx] = 1.0f;
      }
    }
  } else {
    uint16_t pos = (uint16_t)fmodf(t * speed, (float)n);
    for (uint8_t w = 0; w < width; ++w) out[(pos + w) % n] = 1.0f;
  }
}

// Turn on a single LED (index 0..n-1), others off
inline void single(float /*t*/, uint16_t n, uint16_t index, float *out) {
  for (uint16_t i = 0; i < n; ++i) out[i] = 0.0f;
  if (index < n) out[index] = 1.0f;
}
}
