#include <Arduino.h>
#include "interfaces.h"
#include "animations.h"
#include "protocol.h"
#include "node_config.h"
#include "anim_schema.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include "web_ui.h"
#endif
#include "serial_console.h"

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

  // Full dynamic parameter sets (mirror of Anim::ParamSet)
  Anim::ParamSet leaderParams; // defaults already set by struct definition
  Anim::ParamSet followerParams;

#if defined(ARDUINO_ARCH_ESP32)
  // Web UI (leader only)
  WebServer *server{nullptr};
#endif
  SerialConsole console;

  void begin() {
  #ifdef ARDUINO
  Serial.begin(115200);
  #endif
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
  if (isLeader) initConsole();
  }

  void tick() {
    comm->loop();
  if (isLeader) console.loop();
    Message msg;
  while (comm->poll(msg)) {
      if (!isLeader && msg.type == Message::SYNC) {
        // Follower: ACK and apply time sync offset if needed
        comm->sendAck(msg.frame);
  #ifdef ARDUINO
  lastSyncRecvMs = millis();
    int32_t now32 = (int32_t)millis();
  #else
  lastSyncRecvMs = 0; int32_t now32 = 0;
  #endif
        int32_t newOffset = (int32_t)msg.time_ms - now32;
        int32_t diff = newOffset - timeOffsetMs;
        int32_t adiff = diff < 0 ? -diff : diff;
        const int32_t kThresholdMs = 100; // tighter threshold than 150ms
        if (adiff > kThresholdMs) {
          // For moderate diffs, slews half-way to avoid visible jumps
          if (adiff < 200) {
            timeOffsetMs += diff / 2; // gentle correction
            #ifdef ARDUINO
            Serial.print("[SYNC] Slew by ms="); Serial.println(diff / 2);
            #endif
          } else {
            timeOffsetMs = newOffset; // large jump -> snap
            #ifdef ARDUINO
            Serial.print("[SYNC] Snap offset to ms="); Serial.println(timeOffsetMs);
            #endif
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
          #ifdef ARDUINO
          Serial.print("ACK confirmed for frame "); Serial.println(msg.frame);
          #endif
        }
      } else if (isLeader && msg.type == Message::REQ) {
          // Leader: if pending, re-send the SAME frame; otherwise start a new in-flight SYNC
          #ifdef ARDUINO
          uint32_t nowReq = millis();
          #else
          uint32_t nowReq = 0;
          #endif
          if (pendingAck) {
            #ifdef ARDUINO
            Serial.print("REQ: resending SAME frame "); Serial.println(pendingAckFrame);
            #endif
            comm->sendSync(nowReq, pendingAckFrame, Proto::encodeAnimCode(animIndex));
            lastSyncSent = nowReq;
          } else {
            uint32_t currentFrameReq = nowReq / 33;
            #ifdef ARDUINO
            Serial.print("REQ: sending NEW frame "); Serial.println(currentFrameReq);
            #endif
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
    #ifdef ARDUINO
    Serial.println("Applied follower CFG from leader");
    #endif
      }
    }
  #ifdef ARDUINO
  uint32_t now = millis();
  #else
  uint32_t now = 0;
  #endif

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
          #ifdef ARDUINO
          Serial.print("SYNC: new frame "); Serial.println(currentFrame);
          #endif
      comm->sendSync(now, currentFrame, Proto::encodeAnimCode(leaderCfg.animIndex));
          lastSyncSent = now;
          pendingAck = true;
          pendingAckFrame = currentFrame;
        }
      } else {
        // Pending ACK - on timeout, resend the SAME frame
        if (now - lastSyncSent > ackTimeout) {
          #ifdef ARDUINO
          Serial.print("ACK timeout - resending SAME frame "); Serial.println(pendingAckFrame);
          #endif
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
  #ifdef ARDUINO
  Serial.print("FPS: "); Serial.println(fps);
  #endif

      // Map brightness to 5 levels: 0..4 -> characters from low to high
      const char mapChars[5] = { ' ', '.', ':', '*', '#' };
      // Print one line per branch to simulate in-terminal display
      for (uint8_t b = 0; b < Anim::BRANCHES; ++b) {
  #ifdef ARDUINO
  Serial.print("Branch "); Serial.print(b); Serial.print(": ");
  #endif
        for (uint8_t i = 0; i < Anim::LEDS_PER_BRANCH; ++i) {
          uint16_t idx = b * Anim::LEDS_PER_BRANCH + i;
          float v = buf[idx];
          // apply global brightness as well
          v = constrain(v * brightness, 0.0f, 1.0f);
          uint8_t level = (uint8_t)roundf(v * 4.0f); // 0..4
          #ifdef ARDUINO
          Serial.print(mapChars[level]);
          #endif
        }
  #ifdef ARDUINO
  Serial.println();
  #endif
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

  // --- Serial console ---
  static void cbSendSync(void* u){ Node* self = reinterpret_cast<Node*>(u); uint32_t now = millis(); uint32_t frame = now/33; self->comm->sendSync(now, frame, Proto::encodeAnimCode(self->leaderCfg.animIndex)); }
  
  static void cbSetBrightness(void* u, float b){ Node* self = reinterpret_cast<Node*>(u); self->brightness = constrain(b,0.0f,1.0f); self->leds->setBrightness(self->brightness); }
  static void cbApplyFollowerCfg2(void* u, uint8_t animIndex, const uint8_t* ids, const float* vals, uint8_t count){
    Node* self = reinterpret_cast<Node*>(u);
    // Update followerParams
    self->followerCfg.animIndex = animIndex; for(uint8_t i=0;i<count;i++){ Anim::setParamField(self->followerParams, ids[i], vals[i]); }
    // derive legacy subset
    self->followerCfg.speed = self->followerParams.speed; self->followerCfg.phase = self->followerParams.phase; self->followerCfg.width = (animIndex==4)? self->followerParams.singleIndex : self->followerParams.width; self->followerCfg.branchMode = self->followerParams.branch; self->followerCfg.invert = self->followerParams.invert;
    self->globalSpeed = self->followerParams.globalSpeed; self->globalMin = self->followerParams.globalMin; self->globalMax = self->followerParams.globalMax;
    // Send all parameters via CFG2
    self->sendAllParams(1, animIndex, self->followerParams);
  }
  void initConsole(){
    ConsoleAdapter a; a.user=this; a.sendSync=&cbSendSync; a.setBrightness=&cbSetBrightness; a.applyFollowerCfg2=&cbApplyFollowerCfg2; console.begin(a);
  }

#if defined(ARDUINO_ARCH_ESP32)
  // --- Web UI ---
  // Helper: build & send full parameter set for a role via CFG2
  void sendAllParams(uint8_t role, uint8_t animIndex, Anim::ParamSet &ps){
    // Gather ALL parameters from schema
    const size_t MAXP = 24; // safety
    uint8_t ids[MAXP]; float vals[MAXP]; uint8_t count=0;
    for (size_t i=0;i<sizeof(AnimSchema::PARAMS)/sizeof(AnimSchema::PARAMS[0]); ++i){
      AnimSchema::ParamDef pd; memcpy_P(&pd, &AnimSchema::PARAMS[i], sizeof(pd));
      if (count < MAXP){ ids[count] = pd.id; vals[count] = Anim::getParamField(ps, pd.id); count++; }
    }
    // No separate globals (all in main list)
    comm->sendAnimCfg2(role, animIndex, ids, vals, count, nullptr, nullptr, 0);
  }

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
  server->on("/api/apply", HTTP_POST, [this]() { handleApply(); }); // legacy form
  server->on("/api/schema", HTTP_GET, [this]() { serveSchema(); });
  server->on("/api/cfg2", HTTP_POST, [this]() { handleCfg2(); });
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

  void serveSchema() {
    using namespace AnimSchema;
    String j = "{";
    j += "\"params\":[";
    for (size_t i=0;i<sizeof(PARAMS)/sizeof(PARAMS[0]); ++i){
      ParamDef pd; memcpy_P(&pd, &PARAMS[i], sizeof(pd));
      if (i) j += ',';
      j += '{';
      j += "\"id\":" + String(pd.id);
      j += ",\"name\":\"" + String(pd.name) + "\"";
      j += ",\"type\":" + String((uint8_t)pd.type);
      j += ",\"min\":" + String(pd.minVal,3);
      j += ",\"max\":" + String(pd.maxVal,3);
      j += ",\"def\":" + String(pd.defVal,3);
      j += ",\"bits\":" + String(pd.bits);
      j += '}';
    }
    j += "],\"animations\":[";
    for (size_t i=0;i<sizeof(ANIMS)/sizeof(ANIMS[0]); ++i){
      AnimDef ad; memcpy_P(&ad, &ANIMS[i], sizeof(ad));
      if (i) j += ','; j += '{';
      j += "\"index\":" + String(ad.index);
      j += ",\"name\":\"" + String(ad.name) + "\"";
      j += ",\"params\":[";
      for (uint8_t k=0;k<ad.paramCount; ++k){
        uint8_t pid = pgm_read_byte(ad.paramIds + k);
        if (k) j += ','; j += String(pid);
      }
      j += "]}";
    }
    j += "]}";
    server->send(200, "application/json", j);
  }

  // Very light JSON parser for cfg2 {role,animIndex,params:[{id,value}],globals:[{id,value}]}
  void handleCfg2() {
    String body = server->arg("plain");
    auto extractNum=[&](const char *key, float defv)->float{
      int idx = body.indexOf(key);
      if (idx<0) return defv; idx = body.indexOf(':', idx); if (idx<0) return defv; idx++;
      while (idx<(int)body.length() && (body[idx]==' ')) idx++;
      int end=idx; while (end<(int)body.length() && ( (body[end]>='0'&&body[end]<='9') || body[end]=='-' || body[end]=='+' || body[end]=='.' || body[end]=='e' || body[end]=='E')) end++;
      return body.substring(idx,end).toFloat();
    };
    uint8_t role = (uint8_t)extractNum("\"role\"", 1);
    uint8_t animIndex = (uint8_t)extractNum("\"animIndex\"", followerCfg.animIndex);
    // Update target param set
    Anim::ParamSet &ps = (role==0)? leaderParams : followerParams;
    // Parse all occurrences of "id":X and following "value":Y (simple approximation)
    int pos=0; int safety=0;
    while (safety<64) {
      int idKey = body.indexOf("\"id\"", pos); if (idKey<0) break;
      int colon = body.indexOf(':', idKey); if (colon<0) break; int idStart=colon+1;
      while (idStart<(int)body.length() && body[idStart]==' ') idStart++;
      int idEnd=idStart; while (idEnd<(int)body.length() && isdigit(body[idEnd])) idEnd++;
      uint8_t pid = (uint8_t)body.substring(idStart,idEnd).toInt();
      int valKey = body.indexOf("\"value\"", idEnd); if (valKey<0) { pos = idEnd; safety++; continue; }
      colon = body.indexOf(':', valKey); if (colon<0) break; int vStart=colon+1; while (vStart<(int)body.length() && body[vStart]==' ') vStart++;
      int vEnd=vStart; while (vEnd<(int)body.length() && ( (body[vEnd]>='0'&&body[vEnd]<='9') || body[vEnd]=='-' || body[vEnd]=='+' || body[vEnd]=='.')) vEnd++;
      float v = body.substring(vStart,vEnd).toFloat();
      Anim::setParamField(ps, pid, v);
      pos = vEnd; safety++;
    }
    // Also parse globals array similarly (already covered by generic loop since arrays share structure)
    if (role==0){
      leaderCfg.animIndex = animIndex; // keep legacy sync
      // derive legacy fields
      leaderCfg.speed = ps.speed; leaderCfg.phase = ps.phase; leaderCfg.width = (animIndex==4)? ps.singleIndex : ps.width; leaderCfg.branchMode = ps.branch; leaderCfg.invert = ps.invert;
      globalSpeed = ps.globalSpeed; globalMin = ps.globalMin; globalMax = ps.globalMax;
    } else {
      followerCfg.animIndex = animIndex;
      followerCfg.speed = ps.speed; followerCfg.phase = ps.phase; followerCfg.width = (animIndex==4)? ps.singleIndex : ps.width; followerCfg.branchMode = ps.branch; followerCfg.invert = ps.invert;
      globalSpeed = ps.globalSpeed; globalMin = ps.globalMin; globalMax = ps.globalMax;
      // Send out updated full param set for follower immediately
      sendAllParams(1, animIndex, followerParams);
    }
    server->send(200, "application/json", "{\"ok\":true}");
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

  // Update followerParams with legacy form subset
  Anim::setParamField(followerParams, AnimSchema::PID_SPEED, followerCfg.speed);
  Anim::setParamField(followerParams, AnimSchema::PID_PHASE, followerCfg.phase);
  Anim::setParamField(followerParams, (followerCfg.animIndex==4)? AnimSchema::PID_SINGLE_IDX : AnimSchema::PID_WIDTH, followerCfg.width);
  Anim::setParamField(followerParams, AnimSchema::PID_BRANCH, followerCfg.branchMode?1.0f:0.0f);
  Anim::setParamField(followerParams, AnimSchema::PID_INVERT, followerCfg.invert?1.0f:0.0f);
  Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_SPEED, globalSpeed);
  Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_MIN, globalMin);
  Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_MAX, globalMax);
  sendAllParams(1, followerCfg.animIndex, followerParams);

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
