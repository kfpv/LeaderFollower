#include <Arduino.h>
#include "interfaces.h"
#include "animations.h"
#include "protocol.h"
#include "node_config.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include "web_ui.h"
#endif

extern CommunicationInterface* createCommunication();
extern LEDInterface* createLEDs();
extern TimeInterface* createTimeIf();

struct Node {
  bool isLeader;
  CommunicationInterface *comm;
  LEDInterface *leds;
  TimeInterface *timeif;
  float brightness{1.0f};
  uint8_t animIndex{1}; // kept for SYNC compatibility (leader uses leaderCfg.animIndex)
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

  // Animation configuration
  struct AnimCfg {
    uint8_t animIndex{1};
    float speed{3.0f};
    float phase{0.0f};
    uint8_t width{2};
    bool branchMode{true};
    bool invert{false};
  } leaderCfg, followerCfg;
  float globalSpeed{1.0f};
  float globalMin{0.0f}; // 0..1
  float globalMax{0.1f}; // 0..1

#if defined(ARDUINO_ARCH_ESP32)
  // Web UI (leader only)
  WebServer *server{nullptr};
#endif

  void begin() {
    Serial.begin(115200);
    comm->begin();
    leds->begin();
    leds->setBrightness(brightness);

  // Initialize default cfgs
  leaderCfg.animIndex = animIndex;
  followerCfg.animIndex = animIndex;

#if defined(ARDUINO_ARCH_ESP32)
  if (isLeader) {
    setupWiFiAndServer();
  }
#endif
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
          followerCfg.animIndex = animIndex;
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
      } else if (!isLeader && msg.type == Message::CFG) {
        // Followers apply received follower config
        followerCfg.animIndex = msg.cfg_animIndex;
        followerCfg.speed = msg.cfg_speed;
        followerCfg.phase = msg.cfg_phase;
        followerCfg.width = msg.cfg_width;
        followerCfg.branchMode = (msg.cfg_flags & 0x01) != 0;
        followerCfg.invert = (msg.cfg_flags & 0x02) != 0;
        globalSpeed = msg.cfg_globalSpeed;
  globalMin = msg.cfg_min;
  globalMax = msg.cfg_max;
        Serial.println("Applied follower CFG from leader");
      }
    }
    uint32_t now = millis();

    // Follower: only request sync until first SYNC is received
    if (!isLeader && lastSyncRecvMs == 0) {
      if (lastReqSentMs == 0 || now - lastReqSentMs > reqInterval) {
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
      comm->sendSync(now, currentFrame, Proto::encodeAnimCode(leaderCfg.animIndex));
          lastSyncSent = now;
          pendingAck = true;
          pendingAckFrame = currentFrame;
        }
      } else {
        // Pending ACK - on timeout, resend the SAME frame
        if (now - lastSyncSent > ackTimeout) {
          Serial.print("ACK timeout - resending SAME frame "); Serial.println(pendingAckFrame);
      comm->sendSync(now, pendingAckFrame, Proto::encodeAnimCode(leaderCfg.animIndex));
          lastSyncSent = now;
        }
      }
    }
    static float buf[Anim::TOTAL_LEDS];
    // Use synced time for followers
    uint32_t baseNow = isLeader ? now : (uint32_t)((int32_t)now + timeOffsetMs);
    float t = (baseNow / 1000.0f) * globalSpeed;
    // Render based on role-specific config
    const AnimCfg &cfg = isLeader ? leaderCfg : followerCfg;
    renderAnim(cfg, t, buf);

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

    // Apply global min/max scaling for all animations
    float minv = constrain(globalMin, 0.0f, 1.0f);
    float maxv = constrain(globalMax, minv, 1.0f);
    float range = maxv - minv;
    for (uint16_t i = 0; i < Anim::TOTAL_LEDS; ++i) {
      float v = constrain(buf[i], 0.0f, 1.0f);
      buf[i] = minv + v * range;
    }
    leds->setLEDs(buf, Anim::TOTAL_LEDS);

#if defined(ARDUINO_ARCH_ESP32)
    if (isLeader && server) server->handleClient();
#endif
  }

  void renderAnim(const AnimCfg &cfg, float t, float *out) {
    switch (cfg.animIndex) {
      case 0: // static
        Anim::staticOn(t, Anim::TOTAL_LEDS, constrain(cfg.speed, 0.0f, 1.0f), out);
        break;
      case 1: // wave
        Anim::wave(t, Anim::TOTAL_LEDS, cfg.speed, cfg.phase, cfg.branchMode, cfg.invert, out);
        break;
      case 2: // pulse
        Anim::pulse(t, Anim::TOTAL_LEDS, cfg.speed, cfg.phase, cfg.branchMode, out);
        break;
      case 3: // chase
        Anim::chase(t, Anim::TOTAL_LEDS, cfg.speed, cfg.width, cfg.branchMode, out);
        break;
      case 4: // single LED (index in width)
        Anim::single(t, Anim::TOTAL_LEDS, cfg.width, out);
        break;
      default:
        Anim::wave(t, Anim::TOTAL_LEDS, cfg.speed, cfg.phase, cfg.branchMode, cfg.invert, out);
        break;
    }
  }

#if defined(ARDUINO_ARCH_ESP32)
  // --- Web UI ---
  void setupWiFiAndServer() {
    static WebServer srv(80);
    server = &srv;
    const char *apSsid = "LeaderNode-AP";
    const char *apPass = "leds1234";
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(apSsid, apPass);
    Serial.print("AP start: "); Serial.println(ok ? "OK" : "FAIL");
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

    server->on("/", HTTP_GET, [this]() { serveIndex(); });
  server->on("/api/state", HTTP_GET, [this]() { serveState(); });
  server->on("/api/apply", HTTP_POST, [this]() { handleApply(); });
    server->begin();
  }

  void serveIndex() {
  server->send(200, "text/html; charset=utf-8", INDEX_HTML);
  }

  void serveState() {
    String j = "{";
  j += "\"globalSpeed\":" + String(globalSpeed, 3) + ",";
  j += "\"globalMin\":" + String(globalMin, 3) + ",";
  j += "\"globalMax\":" + String(globalMax, 3) + ",";
    j += "\"leader\":{";
    j += "\"animIndex\":" + String(leaderCfg.animIndex) + ",";
    j += "\"speed\":" + String(leaderCfg.speed,3) + ",";
    j += "\"phase\":" + String(leaderCfg.phase,3) + ",";
    j += "\"width\":" + String(leaderCfg.width) + ",";
    j += "\"branchMode\":" + String(leaderCfg.branchMode?"true":"false") + ",";
    j += "\"invert\":" + String(leaderCfg.invert?"true":"false") + "},";
    j += "\"follower\":{";
    j += "\"animIndex\":" + String(followerCfg.animIndex) + ",";
    j += "\"speed\":" + String(followerCfg.speed,3) + ",";
    j += "\"phase\":" + String(followerCfg.phase,3) + ",";
    j += "\"width\":" + String(followerCfg.width) + ",";
    j += "\"branchMode\":" + String(followerCfg.branchMode?"true":"false") + ",";
    j += "\"invert\":" + String(followerCfg.invert?"true":"false") + "}";
    j += "}";
    server->send(200, "application/json", j);
  }

  void handleApply() {
    auto getF = [this](const char* k, float defv){ return server->hasArg(k) ? server->arg(k).toFloat() : defv; };
    auto getI = [this](const char* k, int defv){ return server->hasArg(k) ? server->arg(k).toInt() : defv; };
    auto getB = [this](const char* k, bool defv){ return server->hasArg(k) ? (server->arg(k) == "1" || server->arg(k) == "on") : defv; };

  globalSpeed = getF("globalSpeed", globalSpeed);
  globalMin = getF("globalMin", globalMin);
  globalMax = getF("globalMax", globalMax);
  if (globalMax < globalMin) globalMax = globalMin;

    leaderCfg.animIndex = (uint8_t)getI("L_anim", leaderCfg.animIndex);
    leaderCfg.speed = getF("L_speed", leaderCfg.speed);
    leaderCfg.phase = getF("L_phase", leaderCfg.phase);
    leaderCfg.width = (uint8_t)getI("L_width", leaderCfg.width);
    leaderCfg.branchMode = getB("L_branch", leaderCfg.branchMode);
    leaderCfg.invert = getB("L_invert", leaderCfg.invert);

    followerCfg.animIndex = (uint8_t)getI("F_anim", followerCfg.animIndex);
    followerCfg.speed = getF("F_speed", followerCfg.speed);
    followerCfg.phase = getF("F_phase", followerCfg.phase);
    followerCfg.width = (uint8_t)getI("F_width", followerCfg.width);
    followerCfg.branchMode = getB("F_branch", followerCfg.branchMode);
    followerCfg.invert = getB("F_invert", followerCfg.invert);

    // Keep legacy animIndex in sync for SYNC packets
    animIndex = leaderCfg.animIndex;

    // Send follower config over radio
  comm->sendAnimCfg(/*role=*/1, followerCfg.animIndex, followerCfg.speed, followerCfg.phase,
            followerCfg.width, followerCfg.branchMode, followerCfg.invert,
            globalSpeed, globalMin, globalMax);

    server->send(200, "text/plain", "applied");
  }
#endif
} node;

void setup(){
  node.isLeader = IS_LEADER; // defined in node_config.h
  node.comm = createCommunication();
  node.leds = createLEDs();
  node.timeif = createTimeIf();
  node.begin();
}

void loop(){ node.tick(); delay(10); }
