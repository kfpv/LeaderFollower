#pragma once

// Conditional include for Arduino.h
#if defined(__EMSCRIPTEN__) || defined(_LP64)
  #include "web-sim2/Arduino.h"
#else
  // Actual Arduino compilation
  #include <Arduino.h>
#endif

#include <stdint.h>

// Data-driven parameter & animation schema (shared by encoder/decoder & web JSON)
namespace AnimSchema {

enum ParamType : uint8_t { PT_BOOL=0, PT_RANGE=1, PT_INT=2, PT_ENUM=3 };

struct ParamDef {
  uint8_t   id;       // unique ID
  ParamType type;     // kind
  const char *name;   // short token
  float     minVal;   // for numeric
  float     maxVal;
  float     defVal;   // default
  uint8_t   bits;     // on-wire bits (1..32 practical)
};

struct AnimDef {
  uint8_t index;            // animation index
  const char *name;         // label
  const uint8_t *paramIds;  // pointer into PROGMEM array of param IDs
  uint8_t paramCount;       // number of parameter IDs
};

// Parameter IDs (keep aligned with Anim::ParamId to allow decoupled include sets)
static constexpr uint8_t PID_SPEED        = 1;
static constexpr uint8_t PID_PHASE        = 2;
static constexpr uint8_t PID_WIDTH        = 3;
static constexpr uint8_t PID_BRANCH       = 4;
static constexpr uint8_t PID_INVERT       = 5;
static constexpr uint8_t PID_LEVEL        = 6;
static constexpr uint8_t PID_SINGLE_IDX   = 7;
static constexpr uint8_t PID_RANDOM_MODE  = 8;
static constexpr uint8_t PID_GLOBAL_SPEED = 20;
static constexpr uint8_t PID_GLOBAL_MIN   = 21;
static constexpr uint8_t PID_GLOBAL_MAX   = 22;

// Parameter table (tune bits for resolution vs. packet size)
static const ParamDef PARAMS[] PROGMEM = {
  { PID_SPEED,        PT_RANGE, "speed",       0.0f,   12.0f,  3.0f, 12 },
  { PID_PHASE,        PT_RANGE, "phase",      -6.283f, 6.283f, 0.0f, 12 },
  { PID_WIDTH,        PT_INT,   "width",       1.0f,    8.0f,  3.0f, 4  },
  { PID_BRANCH,       PT_BOOL,  "branch",      0.0f,    1.0f,  0.0f, 1  },
  { PID_INVERT,       PT_BOOL,  "invert",      0.0f,    1.0f,  0.0f, 1  },
  { PID_LEVEL,        PT_RANGE, "level",       0.0f,    1.0f,  0.5f, 8  },
  { PID_SINGLE_IDX,   PT_INT,   "singleIndex", 0.0f,   63.0f,  0.0f, 6  },
  { PID_RANDOM_MODE, PT_BOOL,  "random",      0.0f,    1.0f,  0.0f, 1  },
  { PID_GLOBAL_SPEED, PT_RANGE, "globalSpeed", 0.0f,    4.0f,  1.0f, 10 },
  { PID_GLOBAL_MIN,   PT_RANGE, "globalMin",   0.0f,    1.0f,  0.0f, 8  },
  { PID_GLOBAL_MAX,   PT_RANGE, "globalMax",   0.0f,    1.0f,  1.0f, 8  },
};

// Per-animation param lists (PROGMEM arrays)
static const uint8_t A_STATIC[] PROGMEM = { PID_LEVEL };
static const uint8_t A_WAVE[]   PROGMEM = { PID_SPEED, PID_PHASE, PID_BRANCH, PID_INVERT };
static const uint8_t A_PULSE[]  PROGMEM = { PID_SPEED, PID_PHASE, PID_BRANCH };
static const uint8_t A_CHASE[]  PROGMEM = { PID_SPEED, PID_WIDTH, PID_BRANCH };
static const uint8_t A_SINGLE[] PROGMEM = { PID_SINGLE_IDX };
static const uint8_t A_SPARKLE[] PROGMEM = { PID_SPEED, PID_RANDOM_MODE };

static const AnimDef ANIMS[] PROGMEM = {
  { 0, "Static", A_STATIC, sizeof(A_STATIC) },
  { 1, "Wave",   A_WAVE,   sizeof(A_WAVE)   },
  { 2, "Pulse",  A_PULSE,  sizeof(A_PULSE)  },
  { 3, "Chase",  A_CHASE,  sizeof(A_CHASE)  },
  { 4, "Single", A_SINGLE, sizeof(A_SINGLE) },
  { 5, "Sparkle", A_SPARKLE, sizeof(A_SPARKLE) }
};

inline const ParamDef* findParam(uint8_t id) {
  for (size_t i=0;i<sizeof(PARAMS)/sizeof(PARAMS[0]);++i) {
    ParamDef tmp; memcpy_P(&tmp, &PARAMS[i], sizeof(tmp));
    if (tmp.id == id) return &PARAMS[i];
  }
  return nullptr;
}

inline const AnimDef* findAnim(uint8_t index) {
  for (size_t i=0;i<sizeof(ANIMS)/sizeof(ANIMS[0]);++i) {
    AnimDef tmp; memcpy_P(&tmp, &ANIMS[i], sizeof(tmp));
    if (tmp.index == index) return &ANIMS[i];
  }
  return nullptr;
}

inline uint8_t valueBytes(const ParamDef &pd) {
  if (pd.type == PT_BOOL) return 1;
  if (pd.bits <= 8)  return 1;
  if (pd.bits <= 16) return 2;
  if (pd.bits <= 24) return 3;
  return 4;
}

inline uint32_t encodeValue(float v, const ParamDef &pd) {
  if (pd.type == PT_BOOL) return (v != 0.0f) ? 1u : 0u;
  float mn=pd.minVal, mx=pd.maxVal;
  if (v < mn) v = mn; else if (v > mx) v = mx;
  uint32_t maxq = (pd.bits>=31)?0xFFFFFFFFu:((1u<<pd.bits)-1u);
  if (maxq==0) return 0;
  float qf = (v - mn) * (float)maxq / (mx - mn);
  return (uint32_t)lroundf(qf);
}

inline float decodeValue(uint32_t q, const ParamDef &pd) {
  if (pd.type == PT_BOOL) return (q & 1u)?1.0f:0.0f;
  uint32_t maxq = (pd.bits>=31)?0xFFFFFFFFu:((1u<<pd.bits)-1u);
  if (maxq==0) return pd.minVal;
  return pd.minVal + (float)q * (pd.maxVal - pd.minVal) / (float)maxq;
}

} // namespace AnimSchema
