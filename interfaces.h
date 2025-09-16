#pragma once
#include <Arduino.h>

struct Message {
  enum Type : uint8_t { REQ=0x01, SYNC=0x02, BRIGHTNESS=0x03, ACK=0x04, CFG2=0x06 };
  Type type;
  uint32_t time_ms{0};
  uint32_t frame{0};
  uint16_t anim_code{0};
  float brightness{1.0f};
  // Dynamic configuration payload (when type==CFG2)
  uint8_t cfg2_role{0};
  uint8_t cfg2_animIndex{0};
  uint8_t cfg2_paramCount{0};
  uint8_t cfg2_globalCount{0};
  // Fixed small-capacity arrays (tune as needed)
  uint8_t cfg2_paramIds[16]{};
  float   cfg2_paramValues[16]{};
  uint8_t cfg2_globalIds[8]{};
  float   cfg2_globalValues[8]{};
};

class CommunicationInterface {
 public:
  virtual ~CommunicationInterface() {}
  virtual void begin() = 0;
  virtual bool sendSync(uint32_t time_ms, uint32_t frame) = 0;
  virtual bool sendAck(uint32_t frame) = 0;
  virtual bool sendBrightness(float brightness) = 0;
  virtual bool sendReq() = 0;
  // Send dynamic configuration (CFG2). Caller provides per-animation param id/value pairs and global param pairs.
  virtual bool sendAnimCfg2(uint8_t role,
                            uint8_t animIndex,
                            const uint8_t *animParamIds, const float *animParamValues, uint8_t animParamCount,
                            const uint8_t *globalParamIds, const float *globalParamValues, uint8_t globalParamCount) = 0;
  virtual bool poll(Message &outMsg) = 0; // non-blocking; true when got a message
  virtual void loop() = 0;                // service IRQs if needed
};

class LEDInterface {
 public:
  virtual ~LEDInterface() {}
  virtual void begin() = 0;
  virtual void setBrightness(float b) = 0; // 0..1
  virtual void setLEDs(const float *values, size_t count) = 0; // 0..1 per LED
};

class TimeInterface {
 public:
  virtual ~TimeInterface() {}
  virtual uint32_t nowMs() const = 0; // monotonic ms
  virtual void sleepMs(uint32_t ms) = 0;
};

class ControlInterface {
 public:
  virtual ~ControlInterface() {}
  virtual void begin() {}
  virtual void loop() {}
};
