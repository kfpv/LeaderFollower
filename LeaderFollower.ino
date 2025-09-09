#include <Arduino.h>
#include "interfaces.h"
#include "animations.h"
#include "protocol.h"
#include "node_config.h"

extern CommunicationInterface* createCommunication();
extern LEDInterface* createLEDs();
extern TimeInterface* createTimeIf();

struct Node {
  bool isLeader;
  CommunicationInterface *comm;
  LEDInterface *leds;
  TimeInterface *timeif;
  float brightness{1.0f};
  uint8_t animIndex{1};
  uint32_t lastSyncSent{0};
  uint32_t lastPrintMs{0};
  uint32_t framesSincePrint{0};
  // time sync offset (leader_time_ms - local_now_ms). Used by followers.
  int32_t timeOffsetMs{0};
  // Follower sync request bookkeeping
  uint32_t lastSyncRecvMs{0};
  uint32_t lastReqSentMs{0};
  uint32_t reqInterval{2000}; // follower sends REQ every 2s until synced
  
  // ACK tracking (like Python protocol)
  bool pendingAck{false};
  uint32_t pendingAckFrame{0};
  uint32_t ackTimeout{2000}; // 2 seconds like Python
  uint32_t syncInterval{60000}; // 1 minute between regular syncs

  void begin() {
    Serial.begin(115200);
    comm->begin();
    leds->begin();
    leds->setBrightness(brightness);
  }

  void tick() {
    comm->loop();
    Message msg;
  while (comm->poll(msg)) {
      if (!isLeader && msg.type == Message::SYNC) {
        // Follower: ACK and apply time sync offset if needed
        comm->sendAck(msg.frame);
    lastSyncRecvMs = millis();
        int32_t now32 = (int32_t)millis();
        int32_t newOffset = (int32_t)msg.time_ms - now32;
        int32_t diff = newOffset - timeOffsetMs;
        int32_t adiff = diff < 0 ? -diff : diff;
        const int32_t kThresholdMs = 100; // tighter threshold than 150ms
        if (adiff > kThresholdMs) {
          // For moderate diffs, slews half-way to avoid visible jumps
          if (adiff < 200) {
            timeOffsetMs += diff / 2; // gentle correction
            Serial.print("[SYNC] Slew by ms="); Serial.println(diff / 2);
          } else {
            timeOffsetMs = newOffset; // large jump -> snap
            Serial.print("[SYNC] Snap offset to ms="); Serial.println(timeOffsetMs);
          }
        } else {
          // Already close enough
          // Serial.print("[SYNC] Small diff ms="); Serial.println(adiff);
        }
        // Optionally adopt animation index from leader
        if (msg.anim_code != 0) {
          animIndex = (uint8_t)msg.anim_code;
        }
      } else if (isLeader && msg.type == Message::ACK) {
        // Handle ACK received by leader
        if (pendingAck && msg.frame == pendingAckFrame) {
          pendingAck = false;
          Serial.print("ACK confirmed for frame "); Serial.println(msg.frame);
        }
      } else if (isLeader && msg.type == Message::REQ) {
          // Leader: if pending, re-send the SAME frame; otherwise start a new in-flight SYNC
          uint32_t nowReq = millis();
          if (pendingAck) {
            Serial.print("REQ: resending SAME frame "); Serial.println(pendingAckFrame);
            comm->sendSync(nowReq, pendingAckFrame, Proto::encodeAnimCode(animIndex));
            lastSyncSent = nowReq;
          } else {
            uint32_t currentFrameReq = nowReq / 33;
            Serial.print("REQ: sending NEW frame "); Serial.println(currentFrameReq);
            comm->sendSync(nowReq, currentFrameReq, Proto::encodeAnimCode(animIndex));
            lastSyncSent = nowReq;
            pendingAck = true;
            pendingAckFrame = currentFrameReq;
          }
      } else if (msg.type == Message::BRIGHTNESS) {
        brightness = msg.brightness; leds->setBrightness(brightness);
      }
    }
    uint32_t now = millis();

    // Follower: proactively request sync on boot and when out of sync
    if (!isLeader) {
      bool needReq = (lastSyncRecvMs == 0) || (now - lastSyncRecvMs > reqInterval);
      if (needReq && (lastReqSentMs == 0 || now - lastReqSentMs > reqInterval)) {
        comm->sendReq();
        lastReqSentMs = now;
      }
    }
    
    // Leader sync logic with ACK tracking (single in-flight frame)
    if (isLeader) {
      if (!pendingAck) {
        // No pending ACK - send sync every minute or if this is first sync
        if (lastSyncSent == 0 || (now - lastSyncSent > syncInterval)) {
          uint32_t currentFrame = now / 33;
          Serial.print("SYNC: new frame "); Serial.println(currentFrame);
          comm->sendSync(now, currentFrame, Proto::encodeAnimCode(animIndex));
          lastSyncSent = now;
          pendingAck = true;
          pendingAckFrame = currentFrame;
        }
      } else {
        // Pending ACK - on timeout, resend the SAME frame
        if (now - lastSyncSent > ackTimeout) {
          Serial.print("ACK timeout - resending SAME frame "); Serial.println(pendingAckFrame);
          comm->sendSync(now, pendingAckFrame, Proto::encodeAnimCode(animIndex));
          lastSyncSent = now;
        }
      }
    }
  static float buf[Anim::TOTAL_LEDS];
  // Use synced time for followers
  uint32_t baseNow = isLeader ? now : (uint32_t)((int32_t)now + timeOffsetMs);
  float t = baseNow/1000.0f;
    Anim::wave(t, Anim::TOTAL_LEDS, 3.0f, 0.0f, true, buf);

    // FPS and LED values printing every 500 ms
    framesSincePrint++;
    if (now - lastPrintMs >= 500) {
      float fps = framesSincePrint * 1000.0f / float(now - lastPrintMs);
      Serial.print("FPS: "); Serial.println(fps);

      // Map brightness to 5 levels: 0..4 -> characters from low to high
      const char mapChars[5] = { ' ', '.', ':', '*', '#' };
      // Print one line per branch to simulate in-terminal display
      for (uint8_t b = 0; b < Anim::BRANCHES; ++b) {
        Serial.print("Branch "); Serial.print(b); Serial.print(": ");
        for (uint8_t i = 0; i < Anim::LEDS_PER_BRANCH; ++i) {
          uint16_t idx = b * Anim::LEDS_PER_BRANCH + i;
          float v = buf[idx];
          // apply global brightness as well
          v = constrain(v * brightness, 0.0f, 1.0f);
          uint8_t level = (uint8_t)roundf(v * 4.0f); // 0..4
          Serial.print(mapChars[level]);
        }
        Serial.println();
      }

      lastPrintMs = now;
      framesSincePrint = 0;
    }

    leds->setLEDs(buf, Anim::TOTAL_LEDS);
  }
} node;

void setup(){
  node.isLeader = IS_LEADER; // defined in node_config.h
  node.comm = createCommunication();
  node.leds = createLEDs();
  node.timeif = createTimeIf();
  node.begin();
}

void loop(){ node.tick(); delay(10); }
