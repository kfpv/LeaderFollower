// Tiny C++ shim to expose functions from animations.h to JS without modifying the header.
// We must compile with Emscripten.

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// Include the exact existing header without changes.
#include "../animations.h"

// Internal buffer to avoid exposing raw heap details to JS
static float g_out[Anim::TOTAL_LEDS];

extern "C" {
// Returns constants so JS can size buffers and draw the layout.
uint8_t anim_branches() { return Anim::BRANCHES; }
uint8_t anim_leds_per_branch() { return Anim::LEDS_PER_BRANCH; }
uint16_t anim_total_leds() { return Anim::TOTAL_LEDS; }

// Fill provided buffer with values [0..1] for selected animation.
// out_ptr: pointer to float[TOTAL_LEDS]
// anim_id: 0=staticOn, 1=wave, 2=pulse, 3=chase
// t: time seconds
// a,b,c,d: parameters depending on anim
void anim_eval(int anim_id, float t, float a, float b, float c, float d, float* out_ptr) {
  using namespace Anim;
  switch (anim_id) {
    case 0: // staticOn(t, n, level)
      staticOn(t, TOTAL_LEDS, a, out_ptr);
      break;
    case 1: // wave(t, n, speed, phase, branchMode)
      wave(t, TOTAL_LEDS, a /*speed*/, b /*phase*/, (bool)c /*branchMode*/, out_ptr);
      break;
    case 2: // pulse(t, n, speed, phase, branchMode)
      pulse(t, TOTAL_LEDS, a /*speed*/, b /*phase*/, (bool)c /*branchMode*/, out_ptr);
      break;
    case 3: // chase(t, n, speed, width, branchMode)
      // d unused
      chase(t, TOTAL_LEDS, a /*speed*/, (uint8_t)b /*width*/, (bool)c /*branchMode*/, out_ptr);
      break;
    default:
      // zero
      for (uint16_t i=0;i<TOTAL_LEDS;++i) out_ptr[i] = 0.0f;
      break;
  }
}

// Variant that writes into an internal buffer; then JS can read per-index values.
void anim_eval2(int anim_id, float t, float a, float b, float c, float d) {
  anim_eval(anim_id, t, a, b, c, d, g_out);
}

float anim_value_at(uint16_t idx) {
  if (idx < Anim::TOTAL_LEDS) return g_out[idx];
  return 0.0f;
}
}
