#pragma once
#include <Arduino.h>

namespace Proto {
static constexpr uint8_t MSG_REQ = 0x01;
static constexpr uint8_t MSG_SYNC = 0x02;
static constexpr uint8_t MSG_BRIGHTNESS = 0x03;
static constexpr uint8_t MSG_ACK = 0x04;

// Layout: type(1) | time_ms(4) | frame(4) | anim_code(2) | pad(1)=0
// total 12 bytes
struct SyncPacket {
  uint8_t type{MSG_SYNC};
  uint32_t time_ms{0};
  uint32_t frame{0};
  uint16_t anim_code{0};
  uint8_t pad{0};
} __attribute__((packed));

struct AckPacket {
  uint8_t type{MSG_ACK};
  uint32_t frame{0};
} __attribute__((packed));

struct BrightnessPacket {
  uint8_t type{MSG_BRIGHTNESS};
  uint8_t percent{100};
} __attribute__((packed));

struct ReqPacket {
  uint8_t type{MSG_REQ};
} __attribute__((packed));

inline uint16_t encodeAnimCode(uint8_t animIndex) { return animIndex; }
}
