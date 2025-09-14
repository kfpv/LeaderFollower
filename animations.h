#pragma once

// Conditional include for Arduino.h
#if defined(__EMSCRIPTEN__) || defined(_LP64)
  #include "web-sim2/Arduino.h"
#else
  // Actual Arduino compilation
  #include <Arduino.h>
#endif

namespace Anim {
static constexpr uint8_t BRANCHES = 4;
static constexpr uint8_t LEDS_PER_BRANCH = 7;
static constexpr uint8_t TOTAL_LEDS = BRANCHES * LEDS_PER_BRANCH;

// Legacy core primitives (kept so existing code keeps working)
inline void staticOn(float t, uint16_t n, float level, float *out) {
  (void)t;
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

inline void single(float /*t*/, uint16_t n, uint16_t index, float *out) {
  for (uint16_t i = 0; i < n; ++i) out[i] = 0.0f;
  if (index < n) out[index] = 1.0f;
}

// Dynamic parameter keys (align with anim_schema.h IDs). Re-declare minimal constants for decoupling.
enum ParamId : uint8_t {
  PID_SPEED = 1,
  PID_PHASE = 2,
  PID_WIDTH = 3,
  PID_BRANCH = 4,
  PID_INVERT = 5,
  PID_LEVEL = 6,
  PID_SINGLE_IDX = 7,
  PID_RANDOM_MODE = 8,
  PID_GLOBAL_SPEED = 20,
  PID_GLOBAL_MIN = 21,
  PID_GLOBAL_MAX = 22
};

// Simple POD holder for decoded values (sparse).
struct ParamSet {
  float speed = 3.0f;
  float phase = 0.0f;
  float level = 0.5f;
  uint8_t width = 3;
  uint8_t singleIndex = 0;
  bool branch = false;
  bool invert = false;
  bool randomMode = false;
  float globalSpeed = 1.0f;
  float globalMin = 0.0f;
  float globalMax = 1.0f;
};

inline void applyAnim(uint8_t animIndex, float t, uint16_t n, const ParamSet &ps, float *out) {
  switch (animIndex) {
    case 0: // Static
      staticOn(t, n, ps.level, out);
      break;
    case 1: // Wave
      wave(t, n, ps.speed * ps.globalSpeed, ps.phase, ps.branch, ps.invert, out);
      break;
    case 2: // Pulse (reuse pulse primitive with branch)
      pulse(t, n, ps.speed * ps.globalSpeed, ps.phase, ps.branch, out);
      break;
    case 3: // Chase
      chase(t, n, ps.speed * ps.globalSpeed, ps.width == 0 ? 1 : ps.width, ps.branch, out);
      break;
    case 4: // Single
      single(t, n, ps.singleIndex, out);
      break;
    case 5: // Sparkle
      sparkle(t, n, ps.speed * ps.globalSpeed, ps.randomMode, out);
      break;
    default:
      // Fallback: clear
      for (uint16_t i = 0; i < n; ++i) out[i] = 0.0f;
      break;
  }

  // Apply global min/max scaling if needed
  if (ps.globalMin != 0.0f || ps.globalMax != 1.0f) {
    float gmin = ps.globalMin;
    float gscale = (ps.globalMax > ps.globalMin) ? (ps.globalMax - ps.globalMin) : 0.0f;
    for (uint16_t i = 0; i < n; ++i) {
      float v = out[i];
      v = gmin + v * gscale;
      if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
      out[i] = v;
    }
  }
}
}
