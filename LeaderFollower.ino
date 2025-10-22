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

  // --- Auto mode state ---
  bool autoOn{false};
  uint16_t autoIntervalSec{10};
  bool autoRandom{false};
  // IDs refer to favorites indices as stored
  // We keep selections as a small fixed array for embedded safety
  static const uint8_t kMaxAutoSel = 32;
  uint8_t autoSel[kMaxAutoSel];
  uint8_t autoSelCount{0};
  int8_t autoIdx{-1}; // index into autoSel (-1 = not started)
  uint32_t autoLastMs{0};

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
  // Load Auto config
  autoOn = prefs.getBool("auto_on", false);
  autoIntervalSec = prefs.getUShort("auto_iv", 60);
  autoRandom = prefs.getBool("auto_rand", false);
  autoSelCount = 0;
  String selStr = prefs.getString("auto_sel", "");
  int pos = 0;
  while (pos < (int)selStr.length() && autoSelCount < kMaxAutoSel) {
    int comma = selStr.indexOf(',', pos);
    int end = (comma >= 0) ? comma : selStr.length();
    int val = selStr.substring(pos, end).toInt();
    if (val >= 0 && val <= 255) {
      autoSel[autoSelCount++] = (uint8_t)val;
    }
    if (comma < 0) break; pos = comma + 1;
  }
  autoIdx = prefs.getChar("auto_idx", -1);
  autoLastMs = prefs.getULong("auto_last", 0);
  if (isLeader) {
    setupWiFiAndServer();
  }
#endif
  #ifdef ARDUINO
  if (isLeader) initConsole();
  #endif
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
      // Auto mode advancement
      tickAutoMode(now);
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

  void applyFavoriteToBoth(uint8_t favId){
    // Load favorite JSON from prefs and apply via cfg2-like path to both leader and follower
    String key = String("fav_") + String(favId);
    String cfg = prefs.getString(key.c_str(), "");
    if (cfg.length()==0) return;
    // Parse animIndex for leader and follower and param arrays; use a simple pattern-based parser
    auto findNum=[&](const String &s, const char* key, float defv)->float{
      int idx = s.indexOf(key); if (idx<0) return defv; idx = s.indexOf(':', idx); if (idx<0) return defv; idx++; while (idx<(int)s.length() && s[idx]==' ') idx++; int end=idx; while (end<(int)s.length() && ( (s[end]>='0'&&s[end]<='9') || s[end]=='-' || s[end]=='+' || s[end]=='.')) end++; return s.substring(idx,end).toFloat(); };
    uint8_t L_anim = (uint8_t)findNum(cfg, "\"leader\"\s*:\s*\{[\\s\\S]*?\"animIndex\"", leaderAnimIndex); // fallback approach below
    // Fallback simple: find first "leader" then its animIndex
    int lPos = cfg.indexOf("\"leader\"");
    if (lPos>=0){ int aPos = cfg.indexOf("\"animIndex\"", lPos); if (aPos>=0){ int c = cfg.indexOf(':', aPos); if (c>=0){ int st=c+1; while (st<(int)cfg.length() && cfg[st]==' ') st++; int en=st; while (en<(int)cfg.length() && isdigit(cfg[en])) en++; L_anim = (uint8_t)cfg.substring(st,en).toInt(); } } }
    int fPos = cfg.indexOf("\"follower\""); uint8_t F_anim = followerAnimIndex; if (fPos>=0){ int aPos = cfg.indexOf("\"animIndex\"", fPos); if (aPos>=0){ int c = cfg.indexOf(':', aPos); if (c>=0){ int st=c+1; while (st<(int)cfg.length() && cfg[st]==' ') st++; int en=st; while (en<(int)cfg.length() && isdigit(cfg[en])) en++; F_anim = (uint8_t)cfg.substring(st,en).toInt(); } } }
    // Fill by scanning all id/value pairs inside leader and follower blocks separately
    auto fillParams = [&](const char* blockName, Anim::ParamSet &ps){
      int bpos = cfg.indexOf(blockName); if (bpos<0) return; int brace = cfg.indexOf('{', bpos); if (brace<0) return; int endBlock = cfg.indexOf('}', brace); if (endBlock<0) endBlock = cfg.length();
      int pos=brace; int safety=0; while (pos<endBlock && safety<128){
        int idKey = cfg.indexOf("\"id\"", pos); if (idKey<0 || idKey>=endBlock) break; int colon = cfg.indexOf(':', idKey); if (colon<0) break; int idStart=colon+1; while (idStart<endBlock && cfg[idStart]==' ') idStart++; int idEnd=idStart; while (idEnd<endBlock && isdigit(cfg[idEnd])) idEnd++; uint8_t pid=(uint8_t)cfg.substring(idStart,idEnd).toInt();
        int vKey = cfg.indexOf("\"value\"", idEnd); if (vKey<0 || vKey>=endBlock) { pos = idEnd; safety++; continue; } colon = cfg.indexOf(':', vKey); if (colon<0) break; int vStart=colon+1; while (vStart<endBlock && cfg[vStart]==' ') vStart++; int vEnd=vStart; while (vEnd<endBlock && ( (cfg[vEnd]>='0'&&cfg[vEnd]<='9') || cfg[vEnd]=='-' || cfg[vEnd]=='+' || cfg[vEnd]=='.')) vEnd++; float v = cfg.substring(vStart,vEnd).toFloat();
        Anim::setParamField(ps, pid, v);
        pos = vEnd; safety++;
      }
    };
    fillParams("\"leader\"", leaderParams);
    fillParams("\"follower\"", followerParams);

    // Parse optional globals and apply ONLY globalSpeed (min/max intentionally ignored)
    auto findNumInRange = [&](int start, int end, const char* key, bool &found, float &out){
      found = false; out = 0.0f; if (start<0 || end<=start) return; int k = cfg.indexOf(key, start); if (k<0 || k>=end) return; int c = cfg.indexOf(':', k); if (c<0 || c>=end) return; int st=c+1; while (st<end && cfg[st]==' ') st++; int en=st; while (en<end && ((cfg[en]>='0'&&cfg[en]<='9') || cfg[en]=='-' || cfg[en]=='+' || cfg[en]=='.')) en++; out = cfg.substring(st,en).toFloat(); found = true; };
    int gpos = cfg.indexOf("\"globals\"");
    if (gpos>=0){
      int gl = cfg.indexOf('{', gpos);
      int gr = cfg.indexOf('}', gl);
      if (gl>=0 && gr>gl){
        bool hs=false; float vs=0;
        findNumInRange(gl, gr, "\"globalSpeed\"", hs, vs);
        if (hs){ Anim::setParamField(leaderParams, AnimSchema::PID_GLOBAL_SPEED, vs); Anim::setParamField(followerParams, AnimSchema::PID_GLOBAL_SPEED, vs); }
        // NOTE: we intentionally ignore globalMin/globalMax in favorites to avoid overriding user settings
      }
    }

    leaderAnimIndex = L_anim; followerAnimIndex = F_anim;
    // Update globals mirrors from leader params
    globalSpeed = leaderParams.globalSpeed; globalMin = leaderParams.globalMin; globalMax = leaderParams.globalMax;
#if defined(ARDUINO_ARCH_ESP32)
    // Persist global min/max if changed significantly
    auto fabsf_local = [](float x){ return x < 0 ? -x : x; };
    if (fabsf_local(globalMin - lastSavedGMin) > 0.001f || fabsf_local(globalMax - lastSavedGMax) > 0.001f) {
      prefs.putFloat("gmin", globalMin);
      prefs.putFloat("gmax", globalMax);
      lastSavedGMin = globalMin; lastSavedGMax = globalMax;
    }
#endif
    // Send follower full param set
    sendAllParams(1, followerAnimIndex, followerParams);
  }

  void tickAutoMode(uint32_t nowMs){
    if (!autoOn) return;
    if (autoSelCount == 0) { autoOn = false; return; }
    if (autoIdx < 0 || (nowMs - autoLastMs) >= (uint32_t)autoIntervalSec*1000u){
      // advance index
      if (autoRandom){
        // simple random choice different from previous when possible
        uint8_t newi = (uint8_t)random(0, autoSelCount);
        if (autoSelCount > 1 && autoIdx>=0 && newi == (uint8_t)autoIdx){ newi = (newi + 1) % autoSelCount; }
        autoIdx = (int8_t)newi;
      } else {
        autoIdx = (autoIdx < 0) ? 0 : ((autoIdx + 1) % autoSelCount);
      }
      autoLastMs = nowMs;
      // Apply selected favorite to both
      uint8_t favId = autoSel[(uint8_t)autoIdx];
      applyFavoriteToBoth(favId);
      // Persist progress
      prefs.putChar("auto_idx", autoIdx);
      prefs.putULong("auto_last", autoLastMs);
    }
  }

  // --- Favorites (leader only) ---
  String extractNameFromJson(const String &js){
    int idx = js.indexOf("\"name\"");
    if (idx<0) return String("favorite");
    idx = js.indexOf(':', idx); if (idx<0) return String("favorite"); idx++;
    while (idx < (int)js.length() && (js[idx]==' ')) idx++;
    if (idx < (int)js.length() && js[idx]=='\"'){
      int end = js.indexOf('"', idx+1);
      if (end>idx) return js.substring(idx+1, end);
    }
    // Fallback: number or bare
    int end = idx; while (end<(int)js.length() && js[end]!=',' && js[end] != '}' && js[end] != '\n') end++;
    return js.substring(idx,end);
  }

  void serveFavorites(){
    uint8_t count = prefs.getUChar("fav_count", 0);
    String j = "{\"items\":[";
    for(uint8_t i=0;i<count;i++){
      if (i) j += ',';
      String key = String("fav_") + String(i);
      String cfg = prefs.getString(key.c_str(), "{}");
      String nm = extractNameFromJson(cfg);
      j += "{\"id\":" + String(i) + ",\"name\":\"" + nm + "\",\"cfg\":" + cfg + "}";
    }
    j += "]}";
    server->send(200, "application/json", j);
  }

  void handleFavAdd(){
    String body = server->arg("plain");
    uint8_t count = prefs.getUChar("fav_count", 0);
    String key = String("fav_") + String(count);
    prefs.putString(key.c_str(), body);
    prefs.putUChar("fav_count", (uint8_t)(count+1));
    server->send(200, "application/json", "{\"ok\":true,\"id\":" + String(count) + "}");
  }

  void handleFavDelete(){
    // Accept id either as query arg or in JSON body {id:n}
    int id = -1;
    if (server->hasArg("id")) { id = server->arg("id").toInt(); }
    if (id < 0) {
      String body = server->arg("plain");
      int idx = body.indexOf("\"id\"");
      if (idx >= 0) { idx = body.indexOf(':', idx); if (idx>=0){ idx++; while (idx<(int)body.length() && body[idx]==' ') idx++; int end=idx; while (end<(int)body.length() && isdigit(body[end])) end++; id = body.substring(idx,end).toInt(); } }
    }
    uint8_t count = prefs.getUChar("fav_count", 0);
    if (id < 0 || id >= count) { server->send(400, "application/json", "{\"ok\":false,\"error\":\"bad id\"}"); return; }
    // Shift entries down from id+1 .. count-1
    for (int i=id; i<(int)count-1; ++i){
      String srcKey = String("fav_") + String(i+1);
      String dstKey = String("fav_") + String(i);
      String val = prefs.getString(srcKey.c_str(), "{}");
      prefs.putString(dstKey.c_str(), val);
    }
    // Remove last
    String lastKey = String("fav_") + String(count-1);
    #if ESP_IDF_VERSION_MAJOR >= 4 || defined(ESP_ARDUINO_VERSION)
    prefs.remove(lastKey.c_str());
    #else
    prefs.putString(lastKey.c_str(), "");
    #endif
    prefs.putUChar("fav_count", (uint8_t)(count-1));
    // Remap auto selections: drop deleted id and decrement higher ones
    if (autoSelCount>0){
      uint8_t out[kMaxAutoSel]; uint8_t outCount=0;
      for (uint8_t i=0;i<autoSelCount;i++){
        uint8_t v = autoSel[i];
        if (v == id) continue; // drop
        if (v > id) v = v - 1; // shift
        if (outCount < kMaxAutoSel) out[outCount++] = v;
      }
      for (uint8_t i=0;i<outCount;i++) autoSel[i]=out[i];
      autoSelCount = outCount;
      // Clamp autoIdx
      if (autoSelCount==0){ autoOn=false; autoIdx=-1; }
      else if (autoIdx >= (int8_t)autoSelCount) autoIdx = autoSelCount-1;
      // Persist updated selections
      String s=""; for(uint8_t i=0;i<autoSelCount;i++){ if(i) s+=","; s+=String(autoSel[i]); }
      prefs.putString("auto_sel", s);
      prefs.putChar("auto_idx", autoIdx);
    }
    server->send(200, "application/json", "{\"ok\":true}");
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
  server->on("/api/favorites", HTTP_GET, [this]() { serveFavorites(); });
  server->on("/api/favorites/add", HTTP_POST, [this]() { handleFavAdd(); });
  server->on("/api/favorites/delete", HTTP_POST, [this]() { handleFavDelete(); });
  // Auto endpoints
  server->on("/api/auto/config", HTTP_GET, [this]() { serveAutoConfig(); });
  server->on("/api/auto/settings", HTTP_POST, [this]() { handleAutoSettings(); });
  server->on("/api/auto/start", HTTP_POST, [this]() { handleAutoStart(); });
  server->on("/api/auto/stop", HTTP_POST, [this]() { handleAutoStop(); });
  // Globals-only update (does not stop Auto)
  server->on("/api/globals", HTTP_POST, [this]() { handleGlobals(); });
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

  // --- Auto endpoints ---
  void serveAutoConfig(){
    // derive current favorite name if any
    String curName = ""; int curId = -1; uint32_t remaining=0;
    #ifdef ARDUINO
    uint32_t now = millis();
    #else
    uint32_t now = 0;
    #endif
    if (autoOn && autoSelCount>0 && autoIdx>=0 && autoIdx < (int8_t)autoSelCount){
      curId = autoSel[(uint8_t)autoIdx];
      String key = String("fav_") + String(curId);
      String cfg = prefs.getString(key.c_str(), "");
      curName = extractNameFromJson(cfg);
      if (now >= autoLastMs) {
        uint32_t elapsed = now - autoLastMs;
        uint32_t dur = (uint32_t)autoIntervalSec * 1000u;
        if (elapsed < dur) remaining = (dur - elapsed)/1000u; else remaining = 0;
      }
    }
    // selections as JSON array
    String selJson="["; for(uint8_t i=0;i<autoSelCount;i++){ if(i) selJson += ","; selJson += String(autoSel[i]); } selJson += "]";
    String j = "{";
    j += "\"on\":" + String(autoOn?"true":"false") + ",";
    j += "\"interval\":" + String((int)(autoIntervalSec/60)) + ",";
    j += "\"random\":" + String(autoRandom?"true":"false") + ",";
    j += "\"selections\":" + selJson + ",";
    j += "\"current\":{\"name\":\"" + curName + "\",\"id\":" + String(curId) + ",\"remaining\":" + String((int)remaining) + "}";
    j += "}";
    server->send(200, "application/json", j);
  }

  void handleAutoSettings(){
    String body = server->arg("plain");
    // parse interval
    auto extractNum=[&](const char *key, int defv)->int{
      int idx = body.indexOf(key); if (idx<0) return defv; idx = body.indexOf(':', idx); if (idx<0) return defv; idx++; while (idx<(int)body.length() && body[idx]==' ') idx++; int end=idx; while (end<(int)body.length() && isdigit(body[end])) end++; return body.substring(idx,end).toInt(); };
    auto extractBool=[&](const char *key, bool defv)->bool{
      int idx = body.indexOf(key); if (idx<0) return defv; idx = body.indexOf(':', idx); if (idx<0) return defv; idx++; while (idx<(int)body.length() && (body[idx]==' ')) idx++; if (body.startsWith("true", idx)) return true; if (body.startsWith("false", idx)) return false; return defv; };
    // Treat incoming interval as minutes, store seconds
    {
      int ivMin = extractNum("\"interval\"", autoIntervalSec/60);
      if (ivMin < 1) ivMin = 1;
      autoIntervalSec = (uint16_t)(ivMin * 60);
    }
    autoRandom = extractBool("\"random\"", autoRandom);
    // parse selections array
    autoSelCount = 0;
    int sidx = body.indexOf("\"selections\"");
    if (sidx>=0){ int lb = body.indexOf('[', sidx); int rb = body.indexOf(']', lb); if (lb>=0 && rb>lb){ int pos=lb+1; while (pos<rb && autoSelCount<kMaxAutoSel){ while (pos<rb && (body[pos]==' '||body[pos]==',')) pos++; int start=pos; while (pos<rb && isdigit(body[pos])) pos++; if (pos>start){ int v = body.substring(start,pos).toInt(); if (v>=0 && v<=255) autoSel[autoSelCount++] = (uint8_t)v; } while (pos<rb && body[pos]!=',' ) pos++; } } }
    // persist
    prefs.putUShort("auto_iv", autoIntervalSec);
    prefs.putBool("auto_rand", autoRandom);
    String s=""; for(uint8_t i=0;i<autoSelCount;i++){ if(i) s+=","; s+=String(autoSel[i]); }
    prefs.putString("auto_sel", s);
    server->send(200, "application/json", "{\"ok\":true}");
  }

  void handleAutoStart(){
    if (autoSelCount==0){ server->send(400, "application/json", "{\"ok\":false,\"error\":\"no selections\"}"); return; }
    autoOn = true;
    // start from first or keep current
    if (autoIdx < 0 || autoIdx >= (int8_t)autoSelCount) autoIdx = 0;
    // Force apply immediately so UI shows current
    #ifdef ARDUINO
    uint32_t now = millis();
    #else
    uint32_t now = 0;
    #endif
    autoLastMs = now;
    applyFavoriteToBoth(autoSel[(uint8_t)autoIdx]);
    prefs.putBool("auto_on", true);
    prefs.putChar("auto_idx", autoIdx);
    prefs.putULong("auto_last", autoLastMs);
    server->send(200, "application/json", "{\"ok\":true}");
  }

  void handleAutoStop(){
    autoOn = false;
    prefs.putBool("auto_on", false);
    server->send(200, "application/json", "{\"ok\":true}");
  }



  void serveState() {  
    String j = "{";
    // Auto flags for UI convenience
    #ifdef ARDUINO
    uint32_t now = millis();
    #else
    uint32_t now = 0;
    #endif
    uint32_t remaining=0;
    if (autoOn && autoSelCount>0 && autoIdx>=0){
      if (now >= autoLastMs){
        uint32_t elapsed = now - autoLastMs;
        uint32_t dur = (uint32_t)autoIntervalSec * 1000u;
        if (elapsed < dur) remaining = (dur - elapsed)/1000u; else remaining = 0;
      }
    }
    j += "\"autoOn\":" + String(autoOn?"true":"false") + ",";
    j += "\"autoRemaining\":" + String((int)remaining) + ",";
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
    if (autoOn) {
      autoOn = false;
      prefs.putBool("auto_on", false);
      #ifdef ARDUINO
      Serial.println("Auto stopped due to manual cfg2");
      #endif
    }
    server->send(200, "application/json", "{\"ok\":true}");
  }

  void handleGlobals(){
    String body = server->arg("plain");
    // Parse all occurrences of id/value and apply only globalSpeed/globalMin/globalMax to BOTH roles
    auto applyIfGlobal = [&](uint8_t pid, float v){
      if (pid==AnimSchema::PID_GLOBAL_SPEED || pid==AnimSchema::PID_GLOBAL_MIN || pid==AnimSchema::PID_GLOBAL_MAX){
        Anim::setParamField(leaderParams, pid, v);
        Anim::setParamField(followerParams, pid, v);
      }
    };
    int pos = 0; int safety = 0;
    while (safety < 128) {
      int idKey = body.indexOf("\"id\"", pos); if (idKey < 0) break;
      int colon = body.indexOf(':', idKey); if (colon < 0) break; int idStart = colon + 1;
      while (idStart < (int)body.length() && body[idStart]==' ') idStart++;
      int idEnd = idStart; while (idEnd < (int)body.length() && isdigit(body[idEnd])) idEnd++;
      uint8_t pid = (uint8_t)body.substring(idStart, idEnd).toInt();
      int valKey = body.indexOf("\"value\"", idEnd); if (valKey < 0) { pos = idEnd; safety++; continue; }
      colon = body.indexOf(':', valKey); if (colon < 0) break; int vStart = colon + 1;
      while (vStart < (int)body.length() && body[vStart] == ' ') vStart++;
      int vEnd = vStart; while (vEnd < (int)body.length() && ( (body[vEnd]>='0'&&body[vEnd]<='9') || body[vEnd]=='-' || body[vEnd]=='+' || body[vEnd]=='.')) vEnd++;
      float v = body.substring(vStart, vEnd).toFloat();
      applyIfGlobal(pid, v);
      pos = vEnd; safety++;
    }
    // Ensure min/max are sane (max >= min) on both sets
    if (leaderParams.globalMax < leaderParams.globalMin){
      leaderParams.globalMax = leaderParams.globalMin;
    }
    if (followerParams.globalMax < followerParams.globalMin){
      followerParams.globalMax = followerParams.globalMin;
    }
    // Update mirrors from leader params
    globalSpeed = leaderParams.globalSpeed;
    globalMin = leaderParams.globalMin;
    globalMax = leaderParams.globalMax;
#if defined(ARDUINO_ARCH_ESP32)
    // Persist global min/max if changed significantly
    auto fabsf_local = [](float x){ return x < 0 ? -x : x; };
    if (fabsf_local(globalMin - lastSavedGMin) > 0.001f || fabsf_local(globalMax - lastSavedGMax) > 0.001f) {
      prefs.putFloat("gmin", globalMin);
      prefs.putFloat("gmax", globalMax);
      lastSavedGMin = globalMin; lastSavedGMax = globalMax;
    }
#endif
    // Send follower full param set to apply globals live; do NOT touch Auto state
    sendAllParams(1, followerAnimIndex, followerParams);
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

    if (autoOn) {
      autoOn = false;
      prefs.putBool("auto_on", false);
      #ifdef ARDUINO
      Serial.println("Auto stopped due to legacy apply");
      #endif
    }
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
