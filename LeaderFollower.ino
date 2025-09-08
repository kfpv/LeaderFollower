#include <Arduino.h>
#include "interfaces.h"
#include "animations.h"
#include "protocol.h"

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
      } else if (msg.type == Message::BRIGHTNESS) {
        brightness = msg.brightness; leds->setBrightness(brightness);
      }
    }
    uint32_t now = millis();
    if (isLeader && now - lastSyncSent > 100) {
      comm->sendSync(now, now/33, Proto::encodeAnimCode(animIndex));
      lastSyncSent = now;
    }
    static float buf[Anim::TOTAL_LEDS];
    float t = now/1000.0f;
    Anim::wave(t, Anim::TOTAL_LEDS, 3.0f, 0.0f, true, buf);
    leds->setLEDs(buf, Anim::TOTAL_LEDS);
  }
} node;

void setup(){
  node.isLeader = true; // set false on follower
  node.comm = createCommunication();
  node.leds = createLEDs();
  node.timeif = createTimeIf();
  node.begin();
}

void loop(){ node.tick(); delay(10); }
