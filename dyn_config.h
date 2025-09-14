#pragma once
#include <Arduino.h>
#include "protocol.h"
#include "anim_schema.h"

// Dynamic configuration packet helpers (encoder/decoder)
namespace DynCfg {
using AnimSchema::ParamDef;
using AnimSchema::encodeValue; using AnimSchema::decodeValue; using AnimSchema::valueBytes;

struct ParamValue { uint8_t id; float value; };

// Encode dynamic config (role-specific)
// Layout: [type=MSG_CFG2][role][animIndex][paramCount][paramId valueBytes...][optional 0xFF gCount gParams...]
inline uint8_t encodeCfg2(uint8_t role,
                          uint8_t animIndex,
                          const ParamValue *params, uint8_t paramCount,
                          const ParamValue *globals, uint8_t globalCount,
                          uint8_t *out, uint8_t outMax) {
  using namespace AnimSchema;
  uint8_t *p = out; uint8_t *end = out + outMax;
  auto need=[&](size_t n){ return (size_t)(end-p) >= n; };
  if (!need(4)) return 0;
  *p++ = Proto::MSG_CFG2; *p++ = role; *p++ = animIndex; uint8_t *countField = p++; uint8_t written=0;
  for (uint8_t i=0;i<paramCount;i++) {
    const ParamValue &pv = params[i];
    const ParamDef *pdPGM = AnimSchema::findParam(pv.id); if (!pdPGM) continue;
    ParamDef pd; memcpy_P(&pd, pdPGM, sizeof(pd));
    uint8_t vb = valueBytes(pd); if (!need(1+vb)) break; *p++ = pd.id;
    uint32_t q = encodeValue(pv.value, pd); for (uint8_t b=0;b<vb;b++) *p++ = (uint8_t)((q>>(8*b)) & 0xFF);
    written++;
  }
  *countField = written;
  if (globalCount && need(2)) {
    *p++ = 0xFF; uint8_t *gCountField = p++; uint8_t gw=0;
    for (uint8_t i=0;i<globalCount;i++) {
      const ParamValue &pv = globals[i];
      const ParamDef *pdPGM = AnimSchema::findParam(pv.id); if (!pdPGM) continue;
      ParamDef pd; memcpy_P(&pd, pdPGM, sizeof(pd)); uint8_t vb = valueBytes(pd);
      if (!need(1+vb)) break; *p++ = pd.id;
      uint32_t q = encodeValue(pv.value, pd); for (uint8_t b=0;b<vb;b++) *p++ = (uint8_t)((q>>(8*b)) & 0xFF);
      gw++;
    }
    *gCountField = gw;
  }
  return (uint8_t)(p - out);
}

inline bool decodeCfg2(const uint8_t *data, uint8_t len,
                       uint8_t &role, uint8_t &animIndex,
                       ParamValue *outParams, uint8_t &outCount,
                       ParamValue *outGlobals, uint8_t &outGCount) {
  using namespace AnimSchema;
  if (len < 4 || data[0] != Proto::MSG_CFG2) return false;
  role=data[1]; animIndex=data[2]; uint8_t pc=data[3];
  const uint8_t *p=data+4; const uint8_t *end=data+len; uint8_t w=0, gw=0;
  while (p<end && w<pc) {
    uint8_t pid=*p++; const ParamDef *pdPGM = AnimSchema::findParam(pid); if (!pdPGM) return false;
    ParamDef pd; memcpy_P(&pd, pdPGM, sizeof(pd)); uint8_t vb=valueBytes(pd); if ((size_t)(end-p)<vb) return false;
    uint32_t q=0; for (uint8_t b=0;b<vb;b++) q |= (uint32_t)p[b] << (8*b); p += vb;
    outParams[w++] = { pid, decodeValue(q,pd) };
  }
  outCount = w;
  if (p<end && *p==0xFF) { p++; if (p>=end) return false; uint8_t gc=*p++; while (p<end && gw<gc) {
      uint8_t pid=*p++; const ParamDef *pdPGM = AnimSchema::findParam(pid); if (!pdPGM) return false;
      ParamDef pd; memcpy_P(&pd, pdPGM, sizeof(pd)); uint8_t vb=valueBytes(pd); if ((size_t)(end-p)<vb) return false;
      uint32_t q=0; for (uint8_t b=0;b<vb;b++) q |= (uint32_t)p[b] << (8*b); p += vb;
      outGlobals[gw++] = { pid, decodeValue(q,pd) }; }
    outGCount = gw; }
  else outGCount=0;
  return true;
}

} // namespace DynCfg
