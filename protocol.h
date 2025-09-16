#pragma once
#include <Arduino.h>

namespace Proto {
static constexpr uint8_t MSG_REQ = 0x01;
static constexpr uint8_t MSG_SYNC = 0x02;
static constexpr uint8_t MSG_BRIGHTNESS = 0x03;
static constexpr uint8_t MSG_ACK = 0x04;
// New dynamic configuration packet (variable length, see dyn_config.h)
static constexpr uint8_t MSG_CFG2 = 0x06;

// Flag bits for legacy compact config flags (retained for reference)
// bit0: branchMode (non-single animations)
// bit1: invert (non-single animations)
// bits2-7: single LED index (when animIndex == 4 single animation). 0..63 supported.
static constexpr uint8_t FLAG_BRANCH    = 0x01;
static constexpr uint8_t FLAG_INVERT    = 0x02;
static constexpr uint8_t FLAG_SINGLE_SHIFT = 2; // store index starting at bit2

// Layout: type(1) | time_ms(4) | frame(4)
// total 9 bytes
struct SyncPacket {
  uint8_t type{MSG_SYNC};
  uint32_t time_ms{0};
  uint32_t frame{0};
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
