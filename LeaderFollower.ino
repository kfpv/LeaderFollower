#include <Arduino.h>

#include "interfaces.h"
#include "animations.h"
#include "protocol.h"
#include "node_config.h"
#include "anim_schema.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
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
  uint8_t animIndex{1}; // kept for SYNC compatibility (leader uses leaderAnimIndex)
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

  // Animation indices per role (CFG2-based)
  uint8_t leaderAnimIndex{1};
  uint8_t followerAnimIndex{1};
  float globalSpeed{1.0f};
  float globalMin{0.0f}; // 0..1
  float globalMax{0.1f}; // 0..1

  // Full dynamic parameter sets (mirror of Anim::ParamSet)
  Anim::ParamSet leaderParams; // defaults already set by struct definition
  Anim::ParamSet followerParams;

#if defined(ARDUINO_ARCH_ESP32)
  // Web UI (leader only)
  WebServer *server{nullptr};
  // Persistent storage for globals
  Preferences prefs;
  float lastSavedGMin{-9999.0f};
  float lastSavedGMax{9999.0f};
#endif
#ifdef ARDUINO
  SerialConsole console;
#endif

  void begin() {
  #ifdef ARDUINO
  Serial.begin(115200);
  #endif
    comm->begin();
    leds->begin();
    leds->setBrightness(brightness);

  // Initialize per-role animation indices
  leaderAnimIndex = animIndex;
  followerAnimIndex = animIndex;

#if defined(ARDUINO_ARCH_ESP32)
  // Init NVS and load persisted global min/max if present; apply to both ParamSets
  prefs.begin("vivid", false);
  bool haveMin = prefs.isKey("gmin");
  bool haveMax = prefs.isKey("gmax");
  if (haveMin && haveMax) {
    float gmin = prefs.getFloat("gmin", 0.0f);
    float gmax = prefs.getFloat("gmax", 1.0f);
    // Apply to param sets and legacy mirrors
    leaderParams.globalMin = gmin; leaderParams.globalMax = gmax;
    followerParams.globalMin = gmin; followerParams.globalMax = gmax;
    globalMin = gmin; globalMax = gmax;
    lastSavedGMin = gmin; lastSavedGMax = gmax;
  } else {
    // Initialize lastSaved with current defaults to avoid first-frame write
    lastSavedGMin = globalMin; lastSavedGMax = globalMax;
  }
  if (isLeader) {
    setupWiFiAndServer();
  }
#endif
  if (isLeader) initConsole();
  }

  void tick() {
    comm->loop();
#ifdef ARDUINO
  if (isLeader) console.loop();
#endif
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
  // SYNC no longer carries animation; animation changes arrive via CFG2 only
      } else if (isLeader && msg.type == Message::ACK) {
        // Handle ACK received by leader
        if (pendingAck && msg.frame == pendingAckFrame) {
          pendingAck = false;
          #ifdef ARDUINO
          Serial.print("ACK confirmed for frame "); Serial.println(msg.frame);
          #endif
        }
      } else if (!isLeader && msg.type == Message::CFG2) {
  // Followers apply received dynamic follower config
  followerAnimIndex = msg.cfg2_animIndex;
        // Update followerParams with received pairs
        for (uint8_t i=0;i<msg.cfg2_paramCount;i++) {
          Anim::setParamField(followerParams, msg.cfg2_paramIds[i], msg.cfg2_paramValues[i]);
        }
        for (uint8_t i=0;i<msg.cfg2_globalCount;i++) {
          Anim::setParamField(followerParams, msg.cfg2_globalIds[i], msg.cfg2_globalValues[i]);
        }
  // Update globals mirror
        globalSpeed = followerParams.globalSpeed;
        globalMin = followerParams.globalMin;
        globalMax = followerParams.globalMax;
#if defined(ARDUINO_ARCH_ESP32)
        // Persist globals if changed significantly
        auto fabsf_local = [](float x){ return x < 0 ? -x : x; };
        if (fabsf_local(globalMin - lastSavedGMin) > 0.001f || fabsf_local(globalMax - lastSavedGMax) > 0.001f) {
          prefs.putFloat("gmin", globalMin);
          prefs.putFloat("gmax", globalMax);
          lastSavedGMin = globalMin; lastSavedGMax = globalMax;
        }
#endif
        #ifdef ARDUINO
        Serial.println("Applied follower CFG2 from leader");
        #endif
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
            comm->sendSync(nowReq, pendingAckFrame);
            lastSyncSent = nowReq;
          } else {
            uint32_t currentFrameReq = nowReq / 33;
            #ifdef ARDUINO
            Serial.print("REQ: sending NEW frame "); Serial.println(currentFrameReq);
            #endif
            comm->sendSync(nowReq, currentFrameReq);
            lastSyncSent = nowReq;
            pendingAck = true;
            pendingAckFrame = currentFrameReq;
          }
  } else if (msg.type == Message::BRIGHTNESS) {
        brightness = msg.brightness; leds->setBrightness(brightness);
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
  comm->sendSync(now, currentFrame);
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
  comm->sendSync(now, pendingAckFrame);
          lastSyncSent = now;
        }
      }
    }
    static float buf[Anim::TOTAL_LEDS];
    // Use synced time for followers
    uint32_t baseNow = isLeader ? now : (uint32_t)((int32_t)now + timeOffsetMs);
  // Time in seconds (renderer applies globalSpeed from ParamSet internally)
  float t = (baseNow / 1000.0f);
  // Render using new schema ParamSet directly
  const Anim::ParamSet &ps = isLeader ? leaderParams : followerParams;
  uint8_t aidx = isLeader ? leaderAnimIndex : followerAnimIndex;
  Anim::applyAnim(aidx, t, Anim::TOTAL_LEDS, ps, buf);

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

  // Note: global min/max scaling is already handled inside Anim::applyAnim
    leds->setLEDs(buf, Anim::TOTAL_LEDS);

#if defined(ARDUINO_ARCH_ESP32)
    if (isLeader && server) server->handleClient();
#endif
  }

  // (legacy render wrapper removed; rendering uses ParamSet directly)

  // --- Serial console ---
  static void cbSendSync(void* u){ Node* self = reinterpret_cast<Node*>(u); 
    #ifdef ARDUINO
    uint32_t now = millis();
    #else
    uint32_t now = 0;
    #endif
    uint32_t frame = now/33; self->comm->sendSync(now, frame); }
  
  static void cbSetBrightness(void* u, float b){ Node* self = reinterpret_cast<Node*>(u); self->brightness = constrain(b,0.0f,1.0f); self->leds->setBrightness(self->brightness); }
  static void cbApplyFollowerCfg2(void* u, uint8_t animIndex, const uint8_t* ids, const float* vals, uint8_t count){
    Node* self = reinterpret_cast<Node*>(u);
    // Update followerParams
    self->followerAnimIndex = animIndex;
    for(uint8_t i=0;i<count;i++){ Anim::setParamField(self->followerParams, ids[i], vals[i]); }
    // update globals mirror
    self->globalSpeed = self->followerParams.globalSpeed; self->globalMin = self->followerParams.globalMin; self->globalMax = self->followerParams.globalMax;
  // Send all parameters via CFG2
  #if defined(ARDUINO_ARCH_ESP32)
  self->sendAllParams(1, animIndex, self->followerParams);
  #endif
  }
  #ifdef ARDUINO
  void initConsole(){
    ConsoleAdapter a; a.user=this; a.sendSync=&cbSendSync; a.setBrightness=&cbSetBrightness; a.applyFollowerCfg2=&cbApplyFollowerCfg2; console.begin(a);
  }
  #endif

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
  server->on("/api/cfg2", HTTP_POST, [this]() { handleCfg2(); });
    server->begin();
  }

void serveIndex() {
  String fullHtml;
  fullHtml.reserve(4096); // Pre-allocate to avoid reallocs
  fullHtml += FPSTR(INDEX_HTML_PREFIX);
  fullHtml += FPSTR(ANIM_SCHEMA_JSON);
  fullHtml += FPSTR(INDEX_HTML_SUFFIX);
  server->send(200, "text/html; charset=utf-8", fullHtml);
}


  void serveState() {  
    String j = "{";
    // Leader: dynamic only
    j += "\"leader\":{";
  j += "\"animIndex\":" + String(leaderAnimIndex) + ",";
    j += "\"params\":[";
    for (size_t i=0;i<sizeof(AnimSchema::PARAMS)/sizeof(AnimSchema::PARAMS[0]); ++i){
      AnimSchema::ParamDef pd; memcpy_P(&pd, &AnimSchema::PARAMS[i], sizeof(pd));
      if (i) j += ',';
      j += '{';
      j += "\"id\":" + String(pd.id) + ",\"value\":" + String(Anim::getParamField(leaderParams, pd.id), 5);
      j += '}';
    }
    j += "]},";
    // Follower: dynamic only
    j += "\"follower\":{";
  j += "\"animIndex\":" + String(followerAnimIndex) + ",";
    j += "\"params\":[";
    for (size_t i=0;i<sizeof(AnimSchema::PARAMS)/sizeof(AnimSchema::PARAMS[0]); ++i){
      AnimSchema::ParamDef pd; memcpy_P(&pd, &AnimSchema::PARAMS[i], sizeof(pd));
      if (i) j += ',';
      j += '{';
      j += "\"id\":" + String(pd.id) + ",\"value\":" + String(Anim::getParamField(followerParams, pd.id), 5);
      j += '}';
    }
    j += "]}";
    j += "}";
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
  uint8_t animIndex = (uint8_t)extractNum("\"animIndex\"", followerAnimIndex);
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
      // Update leader index and globals mirror; renderer uses leaderParams
      leaderAnimIndex = animIndex;
      globalSpeed = ps.globalSpeed; globalMin = ps.globalMin; globalMax = ps.globalMax;
#if defined(ARDUINO_ARCH_ESP32)
      // Persist globals if changed significantly
      auto fabsf_local = [](float x){ return x < 0 ? -x : x; };
      if (fabsf_local(globalMin - lastSavedGMin) > 0.001f || fabsf_local(globalMax - lastSavedGMax) > 0.001f) {
        prefs.putFloat("gmin", globalMin);
        prefs.putFloat("gmax", globalMax);
        lastSavedGMin = globalMin; lastSavedGMax = globalMax;
      }
#endif
    } else {
      followerAnimIndex = animIndex;
      // Update globals mirror; renderer uses followerParams
      globalSpeed = ps.globalSpeed; globalMin = ps.globalMin; globalMax = ps.globalMax;
      // Send out updated full param set for follower immediately
      sendAllParams(1, animIndex, followerParams);
#if defined(ARDUINO_ARCH_ESP32)
      // Persist globals if changed significantly (follower side could be remote UI too)
      auto fabsf_local = [](float x){ return x < 0 ? -x : x; };
      if (fabsf_local(globalMin - lastSavedGMin) > 0.001f || fabsf_local(globalMax - lastSavedGMax) > 0.001f) {
        prefs.putFloat("gmin", globalMin);
        prefs.putFloat("gmax", globalMax);
        lastSavedGMin = globalMin; lastSavedGMax = globalMax;
      }
#endif
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

    leaderAnimIndex = (uint8_t)getI("L_anim", leaderAnimIndex);

    followerAnimIndex = (uint8_t)getI("F_anim", followerAnimIndex);
    float F_speed = getF("F_speed", followerParams.speed);
    float F_phase = getF("F_phase", followerParams.phase);
    uint8_t F_width = (uint8_t)getI("F_width", (int)followerParams.width);
    bool F_branch = getB("F_branch", followerParams.branch);
    bool F_invert = getB("F_invert", followerParams.invert);

  // Update followerParams with legacy form subset (ParamSet is source of truth for renderer)
  Anim::setParamField(followerParams, AnimSchema::PID_SPEED, F_speed);
  Anim::setParamField(followerParams, AnimSchema::PID_PHASE, F_phase);
  Anim::setParamField(followerParams, (followerAnimIndex==4)? AnimSchema::PID_SINGLE_IDX : AnimSchema::PID_WIDTH, F_width);
  Anim::setParamField(followerParams, AnimSchema::PID_BRANCH, F_branch?1.0f:0.0f);
  Anim::setParamField(followerParams, AnimSchema::PID_INVERT, F_invert?1.0f:0.0f);
  Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_SPEED, globalSpeed);
  Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_MIN, globalMin);
  Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_MAX, globalMax);
  sendAllParams(1, followerAnimIndex, followerParams);

  // Keep legacy animIndex in sync for SYNC compatibility
  animIndex = leaderAnimIndex;

#if defined(ARDUINO_ARCH_ESP32)
  // Persist globals if changed significantly
  auto fabsf_local = [](float x){ return x < 0 ? -x : x; };
  if (fabsf_local(globalMin - lastSavedGMin) > 0.001f || fabsf_local(globalMax - lastSavedGMax) > 0.001f) {
    prefs.putFloat("gmin", globalMin);
    prefs.putFloat("gmax", globalMax);
    lastSavedGMin = globalMin; lastSavedGMax = globalMax;
  }
#endif

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

void loop(){ node.tick(); 
  #ifdef ARDUINO
  delay(10);
  #endif
}
