#pragma once
#include <Arduino.h>

namespace Proto {
static constexpr uint8_t MSG_REQ = 0x01;
static constexpr uint8_t MSG_SYNC = 0x02;
static constexpr uint8_t MSG_BRIGHTNESS = 0x03;
static constexpr uint8_t MSG_ACK = 0x04;
static constexpr uint8_t MSG_CFG = 0x05;
// New dynamic configuration packet (variable length, see dyn_config.h)
static constexpr uint8_t MSG_CFG2 = 0x06;

// Flag bits inside CfgPacket.flags
// bit0: branchMode (non-single animations)
// bit1: invert (non-single animations)
// bits2-7: single LED index (when animIndex == 4 single animation). 0..63 supported.
static constexpr uint8_t FLAG_BRANCH    = 0x01;
static constexpr uint8_t FLAG_INVERT    = 0x02;
static constexpr uint8_t FLAG_SINGLE_SHIFT = 2; // store index starting at bit2

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

// Animation configuration packet (compact, packed)
// Layout: type(1) | role(1) | animIndex(1) | width(1) | flags(1)
//         | speed(float) | phase(float) | globalSpeed(float)
struct CfgPacket {
  uint8_t type{MSG_CFG};
  uint8_t role{0}; // 0=leader,1=follower
  uint8_t animIndex{0};
  uint8_t width{0};
  uint8_t flags{0}; // see flag layout above
  float speed{0.0f};
  float phase{0.0f};
  float globalSpeed{1.0f};
  float minScale{0.0f};
  float maxScale{1.0f};
} __attribute__((packed));

inline uint16_t encodeAnimCode(uint8_t animIndex) { return animIndex; }
}
