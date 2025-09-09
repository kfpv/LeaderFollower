#pragma once
#include <Arduino.h>

struct Message {
  enum Type : uint8_t { REQ=0x01, SYNC=0x02, BRIGHTNESS=0x03, ACK=0x04, CFG=0x05 };
  Type type;
  uint32_t time_ms{0};
  uint32_t frame{0};
  uint16_t anim_code{0};
  float brightness{1.0f};
  // Animation config payload (when type==CFG)
  uint8_t cfg_role{0}; // 0=leader,1=follower
  uint8_t cfg_animIndex{0};
  uint8_t cfg_width{0};
  uint8_t cfg_flags{0}; // bit0: branchMode, bit1: invert
  float cfg_speed{0.0f};
  float cfg_phase{0.0f};
  float cfg_globalSpeed{1.0f};
  float cfg_min{0.0f};
  float cfg_max{1.0f};
};

class CommunicationInterface {
 public:
  virtual ~CommunicationInterface() {}
  virtual void begin() = 0;
  virtual bool sendSync(uint32_t time_ms, uint32_t frame, uint16_t animCode) = 0;
  virtual bool sendAck(uint32_t frame) = 0;
  virtual bool sendBrightness(float brightness) = 0;
  virtual bool sendReq() = 0;
  // Send animation configuration (to followers)
  virtual bool sendAnimCfg(uint8_t role,
                           uint8_t animIndex,
                           float speed,
                           float phase,
                           uint8_t width,
                           bool branchMode,
                           bool invert,
                           float globalSpeed,
                           float minScale,
                           float maxScale) = 0;
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
