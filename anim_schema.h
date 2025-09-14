// Unified parameter schema using an X-macro list to eliminate duplication.
#pragma once

#if defined(__EMSCRIPTEN__) || defined(_LP64)
  #include "web-sim2/Arduino.h"
#else
  #include <Arduino.h>
#endif

#include <stdint.h>

namespace AnimSchema {

enum ParamType : uint8_t { PT_BOOL=0, PT_RANGE=1, PT_INT=2, PT_ENUM=3 };

struct ParamDef { uint8_t id; ParamType type; const char *name; float minVal; float maxVal; float defVal; uint8_t bits; };
struct AnimDef { uint8_t index; const char *name; const uint8_t *paramIds; uint8_t paramCount; };

// X-macro: NAME, ID, TYPE, UI_NAME, CTYPE, FIELD, MIN, MAX, DEF, BITS
// ADD PARAMS HERE
#define PARAM_LIST(X) \
  X(SPEED,        1,  PT_RANGE, "speed",       float,   speed,        0.0f,   12.0f,  3.0f, 12) \
  X(PHASE,        2,  PT_RANGE, "phase",       float,   phase,       -6.283f, 6.283f, 0.0f, 12) \
  X(WIDTH,        3,  PT_INT,   "width",       uint8_t, width,        1.0f,    8.0f,  3.0f, 4 ) \
  X(BRANCH,       4,  PT_BOOL,  "branch",      bool,    branch,       0.0f,    1.0f,  0.0f, 1 ) \
  X(INVERT,       5,  PT_BOOL,  "invert",      bool,    invert,       0.0f,    1.0f,  0.0f, 1 ) \
  X(LEVEL,        6,  PT_RANGE, "level",       float,   level,        0.0f,    1.0f,  0.5f, 8 ) \
  X(SINGLE_IDX,   7,  PT_INT,   "singleIndex", uint8_t, singleIndex,  0.0f,   63.0f,  0.0f, 6 ) \
  X(RANDOM_MODE,  8,  PT_BOOL,  "random",      bool,    randomMode,   0.0f,    1.0f,  0.0f, 1 ) \
  X(GLOBAL_SPEED, 20, PT_RANGE, "globalSpeed", float,   globalSpeed,  0.0f,    4.0f,  1.0f, 10) \
  X(GLOBAL_MIN,   21, PT_RANGE, "globalMin",   float,   globalMin,    0.0f,    1.0f,  0.0f, 8 ) \
  X(GLOBAL_MAX,   22, PT_RANGE, "globalMax",   float,   globalMax,    0.0f,    1.0f,  1.0f, 8 ) \
  X(CAL_MIN,      23, PT_RANGE, "calMin",      float,   calMin,       0.0f,    1.0f,  0.0f, 8 ) \
  X(CAL_MAX,      24, PT_RANGE, "calMax",      float,   calMax,       0.0f,    1.0f,  0.84f,10) \
  X(DELTA,        25, PT_RANGE, "delta",       float,   delta,        0.0f,    5.0f,  2.0f, 10) \
  X(SPARKLE_MIN,  26, PT_INT,   "sparkMin",    uint8_t, minSparkles,  0.0f,   63.0f,  8.0f, 6 ) \
  X(SPARKLE_MAX,  27, PT_INT,   "sparkMax",    uint8_t, maxSparkles,  0.0f,   63.0f, 12.0f, 6 )

// Generate PIDs
#define DECL_PID(NAME, ID, TYPE, UI, CTYPE, FIELD, MIN, MAX, DEF, BITS) static constexpr uint8_t PID_##NAME = ID;
PARAM_LIST(DECL_PID)
#undef DECL_PID

// ParamDef table
#define PARAM_DEF_ROW(NAME, ID, TYPE, UI, CTYPE, FIELD, MIN, MAX, DEF, BITS) { PID_##NAME, TYPE, UI, MIN, MAX, DEF, BITS },
static const ParamDef PARAMS[] PROGMEM = { PARAM_LIST(PARAM_DEF_ROW) };
#undef PARAM_DEF_ROW

// Animation param lists ADD ANIMATIONS HERE
static const uint8_t A_STATIC[]  PROGMEM = { PID_LEVEL };
static const uint8_t A_WAVE[]    PROGMEM = { PID_SPEED, PID_PHASE, PID_BRANCH, PID_INVERT };
static const uint8_t A_PULSE[]   PROGMEM = { PID_SPEED, PID_PHASE, PID_BRANCH };
static const uint8_t A_CHASE[]   PROGMEM = { PID_SPEED, PID_WIDTH, PID_BRANCH };
static const uint8_t A_SINGLE[]  PROGMEM = { PID_SINGLE_IDX };
static const uint8_t A_SPARKLE[] PROGMEM = { PID_SPEED, PID_RANDOM_MODE, PID_SPARKLE_MIN, PID_SPARKLE_MAX };
static const uint8_t A_PERLIN[]  PROGMEM = { PID_SPEED, PID_WIDTH, PID_CAL_MIN, PID_CAL_MAX, PID_DELTA };

static const AnimDef ANIMS[] PROGMEM = {
  { 0, "Static",  A_STATIC,  sizeof(A_STATIC) },
  { 1, "Wave",    A_WAVE,    sizeof(A_WAVE) },
  { 2, "Pulse",   A_PULSE,   sizeof(A_PULSE) },
  { 3, "Chase",   A_CHASE,   sizeof(A_CHASE) },
  { 4, "Single",  A_SINGLE,  sizeof(A_SINGLE) },
  { 5, "Sparkle", A_SPARKLE, sizeof(A_SPARKLE) },
  { 6, "Perlin",  A_PERLIN,  sizeof(A_PERLIN) }
};

inline const ParamDef* findParam(uint8_t id){
  for(size_t i=0;i<sizeof(PARAMS)/sizeof(PARAMS[0]);++i){ ParamDef tmp; memcpy_P(&tmp,&PARAMS[i],sizeof(tmp)); if(tmp.id==id) return &PARAMS[i]; }
  return nullptr;
}
inline const AnimDef* findAnim(uint8_t index){
  for(size_t i=0;i<sizeof(ANIMS)/sizeof(ANIMS[0]);++i){ AnimDef tmp; memcpy_P(&tmp,&ANIMS[i],sizeof(tmp)); if(tmp.index==index) return &ANIMS[i]; }
  return nullptr;
}
inline uint8_t valueBytes(const ParamDef &pd){ if(pd.type==PT_BOOL) return 1; if(pd.bits<=8) return 1; if(pd.bits<=16) return 2; if(pd.bits<=24) return 3; return 4; }
inline uint32_t encodeValue(float v,const ParamDef &pd){ if(pd.type==PT_BOOL) return (v!=0.0f)?1u:0u; float mn=pd.minVal,mx=pd.maxVal; if(v<mn) v=mn; else if(v>mx) v=mx; uint32_t maxq=(pd.bits>=31)?0xFFFFFFFFu:((1u<<pd.bits)-1u); if(maxq==0) return 0; float qf=(v-mn)*(float)maxq/(mx-mn); return (uint32_t)lroundf(qf); }
inline float decodeValue(uint32_t q,const ParamDef &pd){ if(pd.type==PT_BOOL) return (q&1u)?1.0f:0.0f; uint32_t maxq=(pd.bits>=31)?0xFFFFFFFFu:((1u<<pd.bits)-1u); if(maxq==0) return pd.minVal; return pd.minVal + (float)q*(pd.maxVal-pd.minVal)/(float)maxq; }

} // namespace AnimSchema

namespace Anim {
using namespace AnimSchema; // allow PT_RANGE/PT_BOOL tokens from PARAM_LIST macros
struct ParamSet {
#define PARAM_FIELD(NAME, ID, TYPE, UI, CTYPE, FIELD, MIN, MAX, DEF, BITS) CTYPE FIELD = (CTYPE)(DEF);
  PARAM_LIST(PARAM_FIELD)
#undef PARAM_FIELD
};

inline bool setParamField(ParamSet &ps, uint8_t id, float v){
  switch(id){
#define PARAM_SET_CASE(NAME, ID, TYPE, UI, CTYPE, FIELD, MIN, MAX, DEF, BITS) \
    case AnimSchema::PID_##NAME: ps.FIELD = (CTYPE)((TYPE)==AnimSchema::PT_BOOL? (v!=0.0f): v); return true;
    PARAM_LIST(PARAM_SET_CASE)
#undef PARAM_SET_CASE
    default: return false;
  }
}
inline float getParamField(const ParamSet &ps, uint8_t id){
  switch(id){
#define PARAM_GET_CASE(NAME, ID, TYPE, UI, CTYPE, FIELD, MIN, MAX, DEF, BITS) \
    case AnimSchema::PID_##NAME: return (TYPE)==AnimSchema::PT_BOOL ? (ps.FIELD?1.0f:0.0f) : (float)ps.FIELD;
    PARAM_LIST(PARAM_GET_CASE)
#undef PARAM_GET_CASE
    default: return 0.0f;
  }
}
} // namespace Anim

#undef PARAM_LIST
