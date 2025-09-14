#pragma once

// Conditional include for Arduino.h
#if defined(__EMSCRIPTEN__) || defined(_LP64)
  #include "web-sim2/Arduino.h"
#else
  // Actual Arduino compilation
  #include <Arduino.h>
#endif

#include "anim_schema.h" // brings in Anim::ParamSet & helpers

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

using ParamSet = Anim::ParamSet; // alias for local convenience

// Sparkle animation (time-based, approximates step-wise Python version with internal state)
inline void sparkle(float t, uint16_t n, float speed, bool randomMode, const ParamSet &ps, float *out) {
  // Cap n to our compile-time maximum to keep static storage bounded
  if (n > TOTAL_LEDS) n = TOTAL_LEDS;

  // Time-based refactor: rise is linear (units brightness / sec), fade is exponential (decay rate / sec)
  struct SparkState { float brightness; uint8_t state; float rise; float decay; bool active; };
  enum : uint8_t { GROWING = 0, FADING = 1 };
  static SparkState sparks[TOTAL_LEDS];
  static bool initialized = false;
  static float lastTime = 0.0f;
  static float lastTargetChangeTime = 0.0f;
  static uint32_t rng = 0x12345678u;

  // Dynamic target spawn modulation
  static float spawnRate = 10.0f;       // current spawn attempts per second (smoothed)
  static float spawnRateTarget = 10.0f; // target spawn attempts per second

  // Constants (chosen to approximate previous behavior at speed=1)
  // Defaults used when ParamSet does not override
  const uint8_t minSparkles = 8;
  const uint8_t maxSparkles = 12;
  const float changeInterval = 2.0f;   // how often to retarget spawn tempo
  const float smoothing = 0.8f;         // smoothing factor (used with dt scaling)
  const float baseRise = 2.5f;          // ~0.4s to peak at speed=1 (1 / 2.5)
  const float baseDecay = 1.3f;         // exp(-1.3) ≈ 0.27 (matches old 0.95^25)
  const float minDecay = 0.4f;          // safety lower bound (too low -> very long trails)
  const float maxDecay = 8.0f;          // safety upper bound (avoid instant vanish)
  const float spawnRateMin = 6.0f;      // lower bound spawn attempts / sec
  const float spawnRateMax = 18.0f;     // upper bound spawn attempts / sec
  const float minTempoVal = 0.015f;     // reuses old minDelay notion for modulation mapping
  const float maxTempoVal = 0.070f;     // reuses old maxDelay notion

  // Pseudo-random number generator (LCG) producing [0,1)
  auto frand = [&]() -> float {
    rng = 1664525u * rng + 1013904223u;
    return (rng & 0xFFFFFFu) / 16777216.0f; // 24-bit fraction
  };

  if (!initialized) {
    for (uint16_t i = 0; i < TOTAL_LEDS; ++i) {
      sparks[i].brightness = 0.0f;
      sparks[i].state = GROWING;
      sparks[i].rise = baseRise;
      sparks[i].decay = baseDecay;
      sparks[i].active = false;
    }
    // Seed initial random sparkles so they are de-synchronized from the first frame
    uint8_t seedCount = minSparkles;
    for (uint8_t s = 0; s < seedCount; ++s) {
      uint16_t idx = (uint16_t)(frand() * TOTAL_LEDS);
      SparkState &sp = sparks[idx];
      sp.active = true;
      sp.state = (frand() < 0.5f) ? GROWING : FADING;
      sp.brightness = frand();
      // Randomize rise/decay a bit for variety
      float riseVar = 0.6f + frand() * 0.8f;   // 0.6 - 1.4
      float decayVar = 0.6f + frand() * 0.8f;  // 0.6 - 1.4
      sp.rise = baseRise * riseVar;
      sp.decay = baseDecay * decayVar;
    }
    initialized = true;
    lastTime = t;
    lastTargetChangeTime = t;
  }

  // Compute dt
  float dt = t - lastTime;
  if (dt < 0.0f) dt = 0.0f; // guard against time reset
  // Clamp overly large dt (e.g., frame hitch) to keep behavior stable
  if (dt > 0.25f) dt = 0.25f;
  lastTime = t;

  // Speed scaling (clamp to avoid extreme runaway)
  float speedScale = (speed <= 0.0f) ? 0.000001f : speed;
  if (speedScale > 10.0f) speedScale = 10.0f;

  // Periodically retarget spawn tempo (map random tempo value -> spawnRateTarget)
  if (t - lastTargetChangeTime >= changeInterval) {
    float r = frand();
    float expFactor = r * r; // bias toward center
    float tempoVal = minTempoVal + expFactor * (maxTempoVal - minTempoVal);
    // Invert mapping: shorter tempoVal -> higher spawnRateTarget
    float norm = (maxTempoVal - tempoVal) / (maxTempoVal - minTempoVal); // 0..1
    spawnRateTarget = spawnRateMin + norm * (spawnRateMax - spawnRateMin);
    lastTargetChangeTime = t;
  }
  // Smooth spawnRate toward target (dt-based blending)
  float blend = 1.0f - expf(-smoothing * dt * 5.0f); // quicker convergence independent of frame rate
  spawnRate += (spawnRateTarget - spawnRate) * blend;

  // Update existing sparkles & count actives
  uint8_t activeCount = 0;
  for (uint16_t i = 0; i < n; ++i) {
    SparkState &sp = sparks[i];
    if (!sp.active) continue;
    ++activeCount;

    // Per-spark jitter factor (small) to further desync behavior over time
    float jitter = 0.9f + frand() * 0.2f; // 0.9 - 1.1

    if (sp.state == GROWING) {
      float riseRate = sp.rise * speedScale * jitter; // linear rise
      sp.brightness += riseRate * dt;
      if (sp.brightness >= 1.0f) {
        sp.brightness = 1.0f;
        sp.state = FADING;
      }
    } else { // FADING
      float decayRate = sp.decay * speedScale * jitter;
      if (decayRate < minDecay) decayRate = minDecay;
      if (decayRate > maxDecay) decayRate = maxDecay;
      // Exponential decay
      sp.brightness *= expf(-decayRate * dt);
      if (sp.brightness < 0.02f) {
        sp.active = false;
        --activeCount;
        continue;
      }
    }
  }

  // Decide current target sparkle count (allow ParamSet overrides)
  uint8_t curMin = (ps.minSparkles > 0) ? ps.minSparkles : minSparkles;
  uint8_t curMax = (ps.maxSparkles > 0) ? ps.maxSparkles : maxSparkles;
  uint8_t targetSparkles = curMin;
  if (curMax > curMin) {
    targetSparkles = (uint8_t)(curMin + (uint8_t)(frand() * (float)(curMax - curMin + 1)));
    if (targetSparkles > curMax) targetSparkles = curMax;
  }
  if (targetSparkles > n) targetSparkles = n;

  // Spawn probability for this frame; accelerate with speedScale
  float attemptsPerSec = spawnRate * speedScale;
  if (attemptsPerSec < 0.1f) attemptsPerSec = 0.1f;
  float spawnProb = 1.0f - expf(-attemptsPerSec * dt); // probability at least one attempt

  // Try spawning until we reach target (stochastic each frame)
  // Cap loop iterations to prevent pathological long loops when n is small.
  uint8_t guard = 0;
  while (activeCount < targetSparkles && guard < 20) {
    ++guard;
    if (frand() > spawnProb) break; // no spawn this frame
    uint16_t idx = (uint16_t)(frand() * n);
    if (idx >= n) continue;
    SparkState &sp = sparks[idx];
    if (sp.active) continue;
    sp.active = true;
    sp.state = GROWING;
    // Base parameters
    float rise = baseRise;
    float decay = baseDecay;
    if (randomMode) {
      rise *= (0.6f + frand() * 0.8f);   // 0.6 - 1.4
      decay *= (0.6f + frand() * 0.8f);  // 0.6 - 1.4
    }
    sp.rise = rise;
    sp.decay = decay;
    sp.brightness = 0.05f + frand() * 0.20f; // small random initial brightness
    ++activeCount;
  }

  // Write out buffer
  for (uint16_t i = 0; i < n; ++i) {
    float v = sparks[i].active ? sparks[i].brightness : 0.0f;
    if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
    out[i] = v;
  }
  // Clear & reset any beyond n
  for (uint16_t i = n; i < TOTAL_LEDS; ++i) {
    if (sparks[i].active) {
      sparks[i].active = false;
      sparks[i].brightness = 0.0f;
    }
  }
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
 ,PID_CAL_MIN = 23
 ,PID_CAL_MAX = 24
 ,PID_DELTA   = 25
 ,PID_SPARKLE_MIN = 26
 ,PID_SPARKLE_MAX = 27
};

// (ParamSet already defined above for sparkle)

// --- Lightweight value noise for Perlin pattern ---
inline uint32_t vh(uint32_t x, uint32_t y, uint32_t z){ uint32_t h = x*374761393u + y*668265263u + z*362437u; h = (h ^ (h>>13))*1274126177u; return h ^ (h>>16); }
inline float vhash(uint32_t x, uint32_t y, uint32_t z){ return (vh(x,y,z) & 0xFFFFFFu)/16777216.0f; }
inline float vfade(float t){ return t*t*(3.0f-2.0f*t); }
inline float vlerp(float a,float b,float t){ return a + (b-a)*t; }
inline float valueNoise(float x,float y,float z){
  int X=(int)floorf(x), Y=(int)floorf(y), Z=(int)floorf(z);
  float fx=x-X, fy=y-Y, fz=z-Z;
  float u=vfade(fx), v=vfade(fy), w=vfade(fz);
  float n000=vhash(X,Y,Z), n100=vhash(X+1,Y,Z); float n010=vhash(X,Y+1,Z), n110=vhash(X+1,Y+1,Z);
  float n001=vhash(X,Y,Z+1), n101=vhash(X+1,Y,Z+1); float n011=vhash(X,Y+1,Z+1), n111=vhash(X+1,Y+1,Z+1);
  float nx00=vlerp(n000,n100,u), nx10=vlerp(n010,n110,u); float nx01=vlerp(n001,n101,u), nx11=vlerp(n011,n111,u);
  float nxy0=vlerp(nx00,nx10,v), nxy1=vlerp(nx01,nx11,v); return vlerp(nxy0,nxy1,w);
}
inline float ridgeNoise(float x,float y,float z,int octaves,float lacu,float gain,float offset){
  float sum=0.0f,f=1.0f,amp=0.5f; for(int i=0;i<octaves;i++){ float v=valueNoise(x*f,y*f,z*f); v=v*2.0f-1.0f; v=offset - fabsf(v); v=v*v; sum += v*amp; f*=lacu; amp*=gain; } return sum;
}

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
      sparkle(t, n, ps.speed * ps.globalSpeed, ps.randomMode, ps, out);
      break;
    case 6: { // Perlin (ridge) pattern on virtual 3D rotation collapsed to 2D; branch layout uses delta spacing
      // Map width param (1..8) -> offset for ridge (1 -> 1.5 baseline). We'll treat 'width' directly: offset = 1.5 + (width-1)*0.25
      float offset = 1.5f/3.0f*float(ps.width); // simple mapping; user asked 1 -> 1.5
      int octaves = 1; // fixed as requested
      float lacu = 1.3f; float gain=0.75f;
      // Effective time speed: user speed * 0.002 base scaling (speed=1 => 0.002)
      float ts = t * (ps.speed * 0.02f) * ps.globalSpeed;
      // Precompute ridge normalization (max occurs when each octave hits offset^2 * amp)
      float ampSum = 0.5f * (1.0f - powf(gain, (float)octaves)) / (1.0f - gain);
      float maxRidge = offset*offset * ampSum;
      float invMax = (maxRidge>1e-6f)?(1.0f/maxRidge):1.0f;
      // Branch geometry: if branch flag set, we compute per branch LED coords along axes separated by delta spacing
      if (ps.branch){
        for (uint8_t b=0;b<BRANCHES;b++){
          for (uint8_t i=0;i<LEDS_PER_BRANCH;i++){
            uint16_t idx = b*LEDS_PER_BRANCH + i; if (idx>=n) continue;
            float dist = ps.delta * i + ps.delta * 0.5f; // start at half delta similar to UI first=0.5 maybe adjust
            float x=0,y=0,z=0;
            switch(b){
              case 0: x = dist; break; // Right
              case 1: x = -dist; break; // Left
              case 2: y = dist; break; // Up
              case 3: y = -dist; break; // Down
            }
            // sample ridge (spatialFreq ~3 like JS) compress coordinates
            float p = ridgeNoise(x*3.0f + 0.5f, y*3.0f + 0.5f, z*3.0f + ts*3.0f, octaves, lacu, gain, offset);
            p *= invMax; p = p*p;
            float v = 0.0f;
            if (p <= 0.1f) v=0.0f; else if (p >= 1.0f) v=1.0f; else v=(p-0.1f)/(1.0f-0.1f);
            v = v*v;
            // Apply calibration clamp & remap
            if (ps.calMax > ps.calMin){
              if (v <= ps.calMin) v=0.0f; else if (v >= ps.calMax) v=1.0f; else v = (v - ps.calMin)/(ps.calMax - ps.calMin);
            }
            out[idx] = v;
          }
        }
      } else {
        // Linear layout across n
        for (uint16_t i=0;i<n;i++){
          float x = (float)i * ps.delta; float y=0,z=0;
          float p = ridgeNoise(x*3.0f + 0.5f, y*3.0f + 0.5f, z*3.0f + ts*3.0f, octaves, lacu, gain, offset);
          p*=invMax; p=p*p; float v=0; if (p<=0.1f) v=0; else if (p>=1.0f) v=1.0f; else v=(p-0.1f)/(0.9f); v=v*v;
          if (ps.calMax > ps.calMin){
            if (v <= ps.calMin) v=0.0f; else if (v >= ps.calMax) v=1.0f; else v = (v - ps.calMin)/(ps.calMax - ps.calMin);
          }
          out[i]=v;
        }
      }
      break; }
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
