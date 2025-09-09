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
        comm->sendAck(msg.frame);
      } else if (isLeader && msg.type == Message::ACK) {
        // Handle ACK received by leader
        if (pendingAck && msg.frame == pendingAckFrame) {
          pendingAck = false;
          Serial.print("ACK confirmed for frame "); Serial.println(msg.frame);
        }
      } else if (msg.type == Message::BRIGHTNESS) {
        brightness = msg.brightness; leds->setBrightness(brightness);
      }
    }
    uint32_t now = millis();
    
    // Leader sync logic with ACK tracking (like Python protocol)
    if (isLeader) {
      bool shouldSend = false;
      uint32_t currentFrame = now/33;
      
      if (!pendingAck) {
        // No pending ACK - send sync every minute or if this is first sync
        if (lastSyncSent == 0 || (now - lastSyncSent > syncInterval)) {
          shouldSend = true;
        }
      } else {
        // Pending ACK - check for timeout and resend every 2s
        if (now - lastSyncSent > ackTimeout) {
          shouldSend = true;
          Serial.println("ACK timeout - resending SYNC");
        }
      }
      
      if (shouldSend) {
        comm->sendSync(now, currentFrame, Proto::encodeAnimCode(animIndex));
        lastSyncSent = now;
        pendingAck = true;
        pendingAckFrame = currentFrame;
      }
    }
    static float buf[Anim::TOTAL_LEDS];
    float t = now/1000.0f;
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
