// Dynamic C++ shim to expose functions from animations.h and anim_schema.h to JS.
// Uses the new applyAnim function with dynamic parameter sets.
// We must compile with Emscripten.

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Include the exact existing headers without changes.
// We need to include our Arduino.h shim first so it's found
#include "Arduino.h"
#include "../animations.h"
#include "../anim_schema.h"

// Internal buffer to avoid exposing raw heap details to JS
static float g_out[Anim::TOTAL_LEDS];

// Global parameter set that can be configured from JS
static Anim::ParamSet g_params;

extern "C" {
// Returns constants so JS can size buffers and draw the layout.
uint8_t anim_branches() { return Anim::BRANCHES; }
uint8_t anim_leds_per_branch() { return Anim::LEDS_PER_BRANCH; }
uint16_t anim_total_leds() { return Anim::TOTAL_LEDS; }

// Get animation count
uint8_t anim_count() { 
  return sizeof(AnimSchema::ANIMS) / sizeof(AnimSchema::ANIMS[0]); 
}

// Get animation name by index (returns pointer to static string)
const char* anim_name(uint8_t index) {
  const AnimSchema::AnimDef* anim = AnimSchema::findAnim(index);
  return anim ? anim->name : "";
}

// Get parameter count for an animation
uint8_t anim_param_count(uint8_t anim_index) {
  const AnimSchema::AnimDef* anim = AnimSchema::findAnim(anim_index);
  return anim ? anim->paramCount : 0;
}

// Get parameter ID for animation at param_index
uint8_t anim_param_id(uint8_t anim_index, uint8_t param_index) {
  const AnimSchema::AnimDef* anim = AnimSchema::findAnim(anim_index);
  if (!anim || param_index >= anim->paramCount) return 0;
  
  uint8_t paramId;
  memcpy_P(&paramId, &anim->paramIds[param_index], sizeof(paramId));
  return paramId;
}

// Get parameter info by ID
void param_info(uint8_t param_id, char* name_out, float* min_out, float* max_out, float* def_out, uint8_t* type_out) {
  const AnimSchema::ParamDef* param = AnimSchema::findParam(param_id);
  if (!param) {
    if (name_out) name_out[0] = '\0';
    if (min_out) *min_out = 0.0f;
    if (max_out) *max_out = 1.0f;
    if (def_out) *def_out = 0.0f;
    if (type_out) *type_out = 0;
    return;
  }
  
  AnimSchema::ParamDef tmp;
  memcpy_P(&tmp, param, sizeof(tmp));
  
  if (name_out) strcpy(name_out, tmp.name);
  if (min_out) *min_out = tmp.minVal;
  if (max_out) *max_out = tmp.maxVal;
  if (def_out) *def_out = tmp.defVal;
  if (type_out) *type_out = (uint8_t)tmp.type;
}

// Set a parameter value in the global parameter set
void param_set(uint8_t param_id, float value) {
  const AnimSchema::ParamDef* param = AnimSchema::findParam(param_id);
  if (!param) return;
  
  AnimSchema::ParamDef tmp;
  memcpy_P(&tmp, param, sizeof(tmp));
  
  // Clamp value to parameter range
  if (value < tmp.minVal) value = tmp.minVal;
  if (value > tmp.maxVal) value = tmp.maxVal;
  
  switch (param_id) {
    case AnimSchema::PID_SPEED:
      g_params.speed = value;
      break;
    case AnimSchema::PID_PHASE:
      g_params.phase = value;
      break;
    case AnimSchema::PID_WIDTH:
      g_params.width = (uint8_t)value;
      break;
    case AnimSchema::PID_BRANCH:
      g_params.branch = value != 0.0f;
      break;
    case AnimSchema::PID_INVERT:
      g_params.invert = value != 0.0f;
      break;
    case AnimSchema::PID_LEVEL:
      g_params.level = value;
      break;
    case AnimSchema::PID_SINGLE_IDX:
      g_params.singleIndex = (uint8_t)value;
      break;
    case AnimSchema::PID_GLOBAL_SPEED:
      g_params.globalSpeed = value;
      break;
    case AnimSchema::PID_GLOBAL_MIN:
      g_params.globalMin = value;
      break;
    case AnimSchema::PID_GLOBAL_MAX:
      g_params.globalMax = value;
      break;
  }
}

// Get a parameter value from the global parameter set
float param_get(uint8_t param_id) {
  switch (param_id) {
    case AnimSchema::PID_SPEED: return g_params.speed;
    case AnimSchema::PID_PHASE: return g_params.phase;
    case AnimSchema::PID_WIDTH: return (float)g_params.width;
    case AnimSchema::PID_BRANCH: return g_params.branch ? 1.0f : 0.0f;
    case AnimSchema::PID_INVERT: return g_params.invert ? 1.0f : 0.0f;
    case AnimSchema::PID_LEVEL: return g_params.level;
    case AnimSchema::PID_SINGLE_IDX: return (float)g_params.singleIndex;
    case AnimSchema::PID_GLOBAL_SPEED: return g_params.globalSpeed;
    case AnimSchema::PID_GLOBAL_MIN: return g_params.globalMin;
    case AnimSchema::PID_GLOBAL_MAX: return g_params.globalMax;
    default: return 0.0f;
  }
}

// Evaluate animation using the global parameter set
void anim_eval_global(uint8_t anim_id, float t, float* out_ptr) {
  Anim::applyAnim(anim_id, t, Anim::TOTAL_LEDS, g_params, out_ptr);
}

// Evaluate and store into an internal buffer accessible via getters from JS.
void anim_eval_store_global(uint8_t anim_id, float t) {
  anim_eval_global(anim_id, t, g_out);
}

// JS-facing alternate name
void anim_eval2_global(uint8_t anim_id, float t) {
  anim_eval_store_global(anim_id, t);
}

// Return one value from internal buffer (0..TOTAL_LEDS-1).
float anim_get_value(uint16_t i) {
  if (i >= Anim::TOTAL_LEDS) return 0.0f;
  return g_out[i];
}

float anim_value_at(uint16_t i) { return anim_get_value(i); }

// Legacy compatibility functions - map old interface to new dynamic system
void anim_eval(int anim_id, float t, float a, float b, float c, float d, float* out_ptr) {
  // Create a temporary parameter set for legacy interface
  Anim::ParamSet params = g_params; // Start with global values
  
  // Override with legacy parameters based on animation type
  switch (anim_id) {
    case 0: // staticOn
      params.level = a;
      break;
    case 1: // wave
      params.speed = a;
      params.phase = b;
      params.branch = c != 0.0f;
      params.invert = d != 0.0f;
      break;
    case 2: // pulse
      params.speed = a;
      params.phase = b;
      params.branch = c != 0.0f;
      break;
    case 3: // chase
      params.speed = a;
      params.width = (uint8_t)b;
      params.branch = c != 0.0f;
      break;
    case 4: // single
      params.singleIndex = (uint8_t)a;
      break;
  }
  
  Anim::applyAnim(anim_id, t, Anim::TOTAL_LEDS, params, out_ptr);
}

void anim_eval_store(int anim_id, float t, float a, float b, float c, float d) {
  anim_eval(anim_id, t, a, b, c, d, g_out);
}

void anim_eval2(int anim_id, float t, float a, float b, float c, float d) {
  anim_eval_store(anim_id, t, a, b, c, d);
}
}