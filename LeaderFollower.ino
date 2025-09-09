#include <Arduino.h>
#include "interfaces.h"
#include "animations.h"
#include "protocol.h"
#include "node_config.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
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
  float globalMax{1.0f}; // 0..1

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
  static const char INDEX[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Leader Controls</title>
  <style>
    :root{--bg:#0f1116;--panel:#151826;--text:#e6e6ef;--muted:#9aa4b2;--accent1:#7c3aed;--accent2:#06b6d4;--accent1-20:#7c3aed22;--accent2-20:#06b6d422}
    *{box-sizing:border-box}body{margin:0;font:14px/1.4 system-ui,Segoe UI,Roboto,Helvetica,Arial;background:var(--bg);color:var(--text)}
    header{padding:16px 20px;border-bottom:1px solid #23283b;background:linear-gradient(180deg,#121525,#0f1116)}
    h1{margin:0;font-size:18px;font-weight:600}
    .container{max-width:920px;margin:0 auto;padding:20px;display:grid;gap:16px}
    .card{background:var(--panel);border:1px solid #23283b;border-radius:12px;padding:16px;box-shadow:0 10px 30px rgba(0,0,0,.25)}
    .row{display:grid;grid-template-columns:160px 1fr 96px;gap:10px;align-items:center;margin:10px 0}
    .row label{color:var(--muted)}
    input[type="range"]{width:100%}
    input[type="number"]{width:100%;background:#0c0f18;border:1px solid #23283b;border-radius:8px;color:var(--text);padding:8px}
    select{width:100%;background:#0c0f18;border:1px solid #23283b;border-radius:8px;color:var(--text);padding:8px}
  .twocol{display:grid;grid-template-columns:1fr 1fr;gap:16px}
    .btn{appearance:none;border:0;border-radius:10px;padding:10px 14px;color:white;cursor:pointer;font-weight:600;background:linear-gradient(135deg,var(--accent1),var(--accent2));box-shadow:0 8px 20px var(--accent1-20),0 4px 12px var(--accent2-20)}
    .btn:active{transform:translateY(1px)}
    .pill{padding:4px 8px;border-radius:999px;font-size:12px;border:1px solid #23283b;color:var(--muted)}
    .switch{display:flex;gap:12px;align-items:center}
  </style>
</head>
<body>
  <header><h1>Leader Controls</h1></header>
  <div class="container">
    <div class="card">
  <div class="row"><label>Global speed</label><input id="globalSpeed" type="range" min="0" max="4" step="0.01"><input id="globalSpeed_n" type="number" min="0" max="4" step="0.01"></div>
  <div class="row"><label>Global min</label><input id="globalMin" type="range" min="0" max="1" step="0.01"><input id="globalMin_n" type="number" min="0" max="1" step="0.01"></div>
  <div class="row"><label>Global max</label><input id="globalMax" type="range" min="0" max="1" step="0.01"><input id="globalMax_n" type="number" min="0" max="1" step="0.01"></div>
    </div>
    <div class="twocol">
      <div class="card">
        <div class="pill">Leader animation</div>
        <div class="row"><label>Type</label>
          <select id="L_anim"><option value="0">Static</option><option value="1">Wave</option><option value="2">Pulse</option><option value="3">Chase</option></select>
          <span></span>
        </div>
        <div class="row"><label>Speed</label><input id="L_speed" type="range" min="0" max="12" step="0.01"><input id="L_speed_n" type="number" min="0" max="12" step="0.01"></div>
        <div class="row"><label>Phase</label><input id="L_phase" type="range" min="-6.283" max="6.283" step="0.001"><input id="L_phase_n" type="number" min="-6.283" max="6.283" step="0.001"></div>
        <div class="row"><label>Width</label><input id="L_width" type="range" min="1" max="8" step="1"><input id="L_width_n" type="number" min="1" max="8" step="1"></div>
        <div class="row"><label>Branch mode</label><div class="switch"><input id="L_branch" type="checkbox"><span>per-branch</span></div><span></span></div>
        <div class="row"><label>Invert</label><div class="switch"><input id="L_invert" type="checkbox"><span>reverse</span></div><span></span></div>
      </div>
      <div class="card">
        <div class="pill">Follower animation</div>
        <div class="row"><label>Type</label>
          <select id="F_anim"><option value="0">Static</option><option value="1">Wave</option><option value="2">Pulse</option><option value="3">Chase</option></select>
          <span></span>
        </div>
        <div class="row"><label>Speed</label><input id="F_speed" type="range" min="0" max="12" step="0.01"><input id="F_speed_n" type="number" min="0" max="12" step="0.01"></div>
        <div class="row"><label>Phase</label><input id="F_phase" type="range" min="-6.283" max="6.283" step="0.001"><input id="F_phase_n" type="number" min="-6.283" max="6.283" step="0.001"></div>
        <div class="row"><label>Width</label><input id="F_width" type="range" min="1" max="8" step="1"><input id="F_width_n" type="number" min="1" max="8" step="1"></div>
        <div class="row"><label>Branch mode</label><div class="switch"><input id="F_branch" type="checkbox"><span>per-branch</span></div><span></span></div>
        <div class="row"><label>Invert</label><div class="switch"><input id="F_invert" type="checkbox"><span>reverse</span></div><span></span></div>
      </div>
    </div>
    <div><button class="btn" id="apply">Write settings</button> <span id="status" class="pill">&nbsp;</span></div>
  </div>
  <script>
    const link = (a,b)=>{a.addEventListener('input',()=>b.value=a.value);b.addEventListener('input',()=>a.value=b.value)};
    const ids = id=>document.getElementById(id);
    [
      ['globalSpeed','globalSpeed_n'],['globalMin','globalMin_n'],['globalMax','globalMax_n'],
      ['L_speed','L_speed_n'],['L_phase','L_phase_n'],['L_width','L_width_n'],
      ['F_speed','F_speed_n'],['F_phase','F_phase_n'],['F_width','F_width_n'],
    ].forEach(([a,b])=>link(ids(a),ids(b)));

    async function load(){
      try{
        const r = await fetch('/api/state');
        const s = await r.json();
        ids('globalSpeed').value = ids('globalSpeed_n').value = s.globalSpeed;
  ids('L_anim').value = s.leader.animIndex;
        ids('L_speed').value = ids('L_speed_n').value = s.leader.speed;
        ids('L_phase').value = ids('L_phase_n').value = s.leader.phase;
        ids('L_width').value = ids('L_width_n').value = s.leader.width;
        ids('L_branch').checked = s.leader.branchMode;
        ids('L_invert').checked = s.leader.invert;
        ids('F_anim').value = s.follower.animIndex;
        ids('F_speed').value = ids('F_speed_n').value = s.follower.speed;
        ids('F_phase').value = ids('F_phase_n').value = s.follower.phase;
        ids('F_width').value = ids('F_width_n').value = s.follower.width;
        ids('F_branch').checked = s.follower.branchMode;
  ids('F_invert').checked = s.follower.invert;
  ids('globalMin').value = ids('globalMin_n').value = s.globalMin;
  ids('globalMax').value = ids('globalMax_n').value = s.globalMax;
      }catch(e){console.error(e);}
    }

    async function apply(){
      const form = new URLSearchParams();
      form.set('globalSpeed', ids('globalSpeed_n').value);
  form.set('L_anim', ids('L_anim').value);
      form.set('L_speed', ids('L_speed_n').value);
      form.set('L_phase', ids('L_phase_n').value);
      form.set('L_width', ids('L_width_n').value);
      form.set('L_branch', ids('L_branch').checked ? '1':'0');
      form.set('L_invert', ids('L_invert').checked ? '1':'0');
      form.set('F_anim', ids('F_anim').value);
      form.set('F_speed', ids('F_speed_n').value);
      form.set('F_phase', ids('F_phase_n').value);
      form.set('F_width', ids('F_width_n').value);
      form.set('F_branch', ids('F_branch').checked ? '1':'0');
  form.set('F_invert', ids('F_invert').checked ? '1':'0');
  form.set('globalMin', ids('globalMin_n').value);
  form.set('globalMax', ids('globalMax_n').value);
      const r = await fetch('/api/apply',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:String(form)});
      const t = await r.text();
      ids('status').textContent = t || 'applied';
      setTimeout(()=>ids('status').textContent='\u00a0', 1500);
    }
    ids('apply').addEventListener('click', apply);
    load();
  </script>
</body>
</html>
)HTML";
    server->send(200, "text/html; charset=utf-8", INDEX);
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
