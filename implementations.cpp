#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LoRaWan_APP.h>
#include "interfaces.h"
#include "protocol.h"
#include "dyn_config.h"
#include "node_config.h"
#include "led_channel_inverse.h"

// I2C pin configuration
#ifndef SDA_PIN
#define SDA_PIN 5
#endif
#ifndef SCL_PIN
#define SCL_PIN 6
#endif

// Heltec Radio events structure instance (required by library)
static RadioEvents_t RadioEvents;

// ----- Time -----
class ArduinoTime : public TimeInterface {
 public:
  uint32_t nowMs() const override { return millis(); }
  void sleepMs(uint32_t ms) override { delay(ms); }
};

// ----- LEDs (PCA9685) -----
class Pca9685LEDs : public LEDInterface {
 public:
  explicit Pca9685LEDs(uint8_t addr1 = 0x40, uint8_t addr2 = 0x60, bool useSecond = true)
      : _pwm1(addr1), _pwm2(addr2), _useSecond(useSecond) {}
  void begin() override {
    // Initialize I2C on specified pins (user requirement: SDA=5, SCL=6, 400kHz)
  Wire.begin(SDA_PIN, SCL_PIN, 400000); // SDA, SCL, Frequency
    _pwm1.begin();
    _pwm1.setPWMFreq(1600); // maximum PWM frequency per PCA9685 datasheet / library
    for(int i=0; i<16; ++i) _pwm1.setPWM(i, 0, 0); // turn off all channels
    if (_useSecond) {
      _pwm2.begin();
      _pwm2.setPWMFreq(1600);
      for(int i=0; i<16; ++i) _pwm2.setPWM(i, 0, 0); // turn off all channels
    }
    setBrightness(0.1f);
  }
  void setBrightness(float b) override { _global = constrain(b, 0.0f, 1.0f); }
  void setLEDs(const float *values, size_t count) override {
    // Serial.print("Setting LEDs: ");
    // Serial.print(count);
    // Serial.print("  ");

    // Map first 16 channels to addr1, remaining to addr2
    for (size_t logical = 0; logical < count && logical < LED_CHANNEL_COUNT; ++logical) {
      int16_t phys = LED_CHANNEL_INV[logical];
      if (phys < 0) continue; // logical LED not connected
      float v = constrain(values[logical] * _global, 0.0f, 1.0f);
      uint16_t pwm = (uint16_t)(v * 4095.0f);
      if (phys < 16) {
        _pwm1.setPWM((uint8_t)phys, 0, pwm);
      } else if (_useSecond && phys < 32) {
        _pwm2.setPWM((uint8_t)(phys - 16), 0, pwm);
      }
    }
    // Serial.println();
  }
 private:
  Adafruit_PWMServoDriver _pwm1;
  Adafruit_PWMServoDriver _pwm2;
  bool _useSecond{true};
  float _global{1.0f};
};

// ----- LoRa (Heltec) -----
class HeltecLoRa : public CommunicationInterface {
 public:
  void begin() override {
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    RadioEvents.TxDone = onTxDoneStatic;
    RadioEvents.TxTimeout = onTxTimeoutStatic;
    RadioEvents.RxDone = onRxDoneStatic;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(915000000);
    Radio.SetTxConfig(MODEM_LORA, 5, 0, 0, 7, 1, 8, false, true, 0, 0, false, 3000);
    Radio.SetRxConfig(MODEM_LORA, 0, 7, 1, 0, 8, 0, false, 0, true, 0, 0, false, true);
    Radio.Rx(0);
    instance_ = this;
  }

  bool sendSync(uint32_t time_ms, uint32_t frame, uint16_t animCode) override {
  Proto::SyncPacket p; p.time_ms = time_ms; p.frame = frame; p.anim_code = animCode;
  // Log transmit
  Serial.print("TX SYNC time_ms="); Serial.print(p.time_ms);
  Serial.print(" frame="); Serial.print(p.frame);
  Serial.print(" anim="); Serial.println(p.anim_code);
  return sendRaw((uint8_t*)&p, sizeof(p));
  }
  bool sendAck(uint32_t frame) override {
  Proto::AckPacket p; p.frame = frame;
  Serial.print("TX ACK frame="); Serial.println(p.frame);
  return sendRaw((uint8_t*)&p, sizeof(p));
  }
  bool sendBrightness(float brightness) override {
  Proto::BrightnessPacket p; p.percent = (uint8_t)constrain((int)(brightness*100.0f),0,100);
  Serial.print("TX BRIGHTNESS percent="); Serial.println(p.percent);
  return sendRaw((uint8_t*)&p, sizeof(p));
  }
  bool sendReq() override {
  Proto::ReqPacket p; Serial.println("TX REQ"); return sendRaw((uint8_t*)&p, sizeof(p));
  }
  bool sendAnimCfg(uint8_t role,
                   uint8_t animIndex,
                   float speed,
                   float phase,
                   uint8_t width,
                   bool branchMode,
                   bool invert,
                   float globalSpeed,
                   float minScale,
                   float maxScale) override {
    Proto::CfgPacket p;
    p.role = role;
    p.animIndex = animIndex;
    p.width = width;
    if (animIndex == 4) {
      // Encode single LED index into upper bits of flags (bits2-7)
      uint8_t idx = width & 0x3F; // 0..63
      p.flags = (idx << Proto::FLAG_SINGLE_SHIFT);
    } else {
      p.flags = (branchMode ? Proto::FLAG_BRANCH : 0) | (invert ? Proto::FLAG_INVERT : 0);
    }
    p.speed = speed;
    p.phase = phase;
    p.globalSpeed = globalSpeed;
    p.minScale = minScale;
    p.maxScale = maxScale;
    Serial.print("TX CFG role="); Serial.print(p.role);
    Serial.print(" anim="); Serial.print(p.animIndex);
    Serial.print(" speed="); Serial.print(p.speed);
    Serial.print(" phase="); Serial.print(p.phase);
    Serial.print(" width="); Serial.print(p.width);
    Serial.print(" flags="); Serial.print(p.flags);
    Serial.print(" gSpeed="); Serial.print(p.globalSpeed);
    Serial.print(" min="); Serial.print(p.minScale);
    Serial.print(" max="); Serial.println(p.maxScale);
    return sendRaw((uint8_t*)&p, sizeof(p));
  }

  bool sendAnimCfg2(uint8_t role,
                    uint8_t animIndex,
                    const uint8_t *animParamIds, const float *animParamValues, uint8_t animParamCount,
                    const uint8_t *globalParamIds, const float *globalParamValues, uint8_t globalParamCount) override {
    // Build DynCfg::ParamValue arrays on stack
    DynCfg::ParamValue localAnim[16];
    DynCfg::ParamValue localGlobal[16];
    if (animParamCount > 16) animParamCount = 16;
    if (globalParamCount > 16) globalParamCount = 16;
    for (uint8_t i=0;i<animParamCount;i++){ localAnim[i] = { animParamIds[i], animParamValues[i] }; }
    for (uint8_t i=0;i<globalParamCount;i++){ localGlobal[i] = { globalParamIds[i], globalParamValues[i] }; }
    uint8_t buf[64];
    uint8_t len = DynCfg::encodeCfg2(role, animIndex, localAnim, animParamCount, localGlobal, globalParamCount, buf, sizeof(buf));
    if (!len) return false;
    Serial.print("TX CFG2 role="); Serial.print(role);
    Serial.print(" anim="); Serial.print(animIndex);
    Serial.print(" aParams="); Serial.print(animParamCount);
    Serial.print(" gParams="); Serial.println(globalParamCount);
    return sendRaw(buf, len);
  }

  bool poll(Message &outMsg) override {
    if (!_hasRx) return false;
    _hasRx = false;
    // parse rx buffer
    if (_rxSize < 1) return false;
    uint8_t type = _rxBuf[0];
    if (type == Proto::MSG_SYNC && _rxSize >= sizeof(Proto::SyncPacket)) {
      auto *p = reinterpret_cast<Proto::SyncPacket*>(_rxBuf);
      outMsg.type = (Message::Type)Proto::MSG_SYNC;
      outMsg.time_ms = p->time_ms; outMsg.frame = p->frame; outMsg.anim_code = p->anim_code;
  Serial.print("RX SYNC time_ms="); Serial.print(p->time_ms);
  Serial.print(" frame="); Serial.print(p->frame);
  Serial.print(" anim="); Serial.println(p->anim_code);
      return true;
    } else if (type == Proto::MSG_ACK && _rxSize >= sizeof(Proto::AckPacket)) {
  auto *p = reinterpret_cast<Proto::AckPacket*>(_rxBuf);
  outMsg.type = (Message::Type)Proto::MSG_ACK; outMsg.frame = p->frame;
  Serial.print("RX ACK frame="); Serial.println(p->frame);
  return true;
    } else if (type == Proto::MSG_BRIGHTNESS && _rxSize >= sizeof(Proto::BrightnessPacket)) {
  auto *p = reinterpret_cast<Proto::BrightnessPacket*>(_rxBuf);
  outMsg.type = (Message::Type)Proto::MSG_BRIGHTNESS; outMsg.brightness = p->percent/100.0f;
  Serial.print("RX BRIGHTNESS percent="); Serial.println(p->percent);
  return true;
  } else if (type == Proto::MSG_CFG && _rxSize >= sizeof(Proto::CfgPacket)) {
  auto *p = reinterpret_cast<Proto::CfgPacket*>(_rxBuf);
  outMsg.type = Message::CFG;
  outMsg.cfg_role = p->role;
  outMsg.cfg_animIndex = p->animIndex;
  if (p->animIndex == 4) {
    // Extract single LED index from flags bits2-7
    uint8_t raw = (p->flags >> Proto::FLAG_SINGLE_SHIFT) & 0x3F;
    outMsg.cfg_width = raw; // reuse width field in message for LED index
    outMsg.cfg_flags = 0;   // branch/invert not applicable for single
  } else {
    outMsg.cfg_width = p->width;
    outMsg.cfg_flags = p->flags;
  }
  outMsg.cfg_speed = p->speed;
  outMsg.cfg_phase = p->phase;
  outMsg.cfg_globalSpeed = p->globalSpeed;
  outMsg.cfg_min = p->minScale;
  outMsg.cfg_max = p->maxScale;
  Serial.print("RX CFG role="); Serial.print(p->role);
  Serial.print(" anim="); Serial.print(p->animIndex);
  Serial.print(" speed="); Serial.print(p->speed);
  Serial.print(" phase="); Serial.print(p->phase);
  Serial.print(" width="); Serial.print(p->width);
  Serial.print(" flags="); Serial.print(p->flags);
  Serial.print(" gSpeed="); Serial.print(p->globalSpeed);
  Serial.print(" min="); Serial.print(p->minScale);
  Serial.print(" max="); Serial.println(p->maxScale);
  return true;
    } else if (type == Proto::MSG_CFG2) {
      // Dynamic configuration decode
      DynCfg::ParamValue animVals[16]; uint8_t animCount=0;
      DynCfg::ParamValue globalVals[8]; uint8_t globalCount=0;
      uint8_t role=0, animIndex=0;
      if (!DynCfg::decodeCfg2(_rxBuf, (uint8_t)_rxSize, role, animIndex, animVals, animCount, globalVals, globalCount)) {
        Serial.println("RX CFG2 decode failed");
        return false;
      }
      outMsg.type = Message::CFG2;
      outMsg.cfg2_role = role;
      outMsg.cfg2_animIndex = animIndex;
      outMsg.cfg2_paramCount = animCount;
      outMsg.cfg2_globalCount = globalCount;
      for (uint8_t i=0;i<animCount && i<16;i++){ outMsg.cfg2_paramIds[i]=animVals[i].id; outMsg.cfg2_paramValues[i]=animVals[i].value; }
      for (uint8_t i=0;i<globalCount && i<8;i++){ outMsg.cfg2_globalIds[i]=globalVals[i].id; outMsg.cfg2_globalValues[i]=globalVals[i].value; }
      Serial.print("RX CFG2 role="); Serial.print(role);
      Serial.print(" anim="); Serial.print(animIndex);
      Serial.print(" aParams="); Serial.print(animCount);
      Serial.print(" gParams="); Serial.println(globalCount);
      return true;
    } else if (type == Proto::MSG_REQ && _rxSize >= sizeof(Proto::ReqPacket)) {
  outMsg.type = (Message::Type)Proto::MSG_REQ;
  Serial.println("RX REQ");
  return true;
    }
    return false;
  }

  void loop() override {
    Radio.IrqProcess();
  }

 private:
  bool sendRaw(uint8_t *data, uint8_t len) {
    Radio.Send(data, len); return true;
  }

  static void onTxDoneStatic() { if (instance_) instance_->onTxDone(); }
  static void onTxTimeoutStatic() { if (instance_) instance_->onTxTimeout(); }
  static void onRxDoneStatic(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    if (instance_) instance_->onRxDone(payload, size, rssi, snr);
  }

  void onTxDone() {
    // After any transmission, immediately go back to RX to listen for responses (e.g., ACK)
    Serial.println("RADIO: TX done -> RX");
    Radio.Sleep();
    Radio.Rx(0);
  }
  void onTxTimeout() { Radio.Sleep(); Radio.Rx(0); }
  void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    _rxSize = min<uint16_t>(size, sizeof(_rxBuf));
    memcpy(_rxBuf, payload, _rxSize);
    Serial.print("RADIO: RX done size="); Serial.print(_rxSize);
    Serial.print(" rssi="); Serial.print(rssi);
    Serial.print(" snr="); Serial.println(snr);
    Radio.Sleep();
    _hasRx = true;
    Radio.Rx(0);
  }

  static HeltecLoRa *instance_;
  bool _hasRx{false};
  uint16_t _rxSize{0};
  uint8_t _rxBuf[64]{};
};
HeltecLoRa* HeltecLoRa::instance_ = nullptr;

// Factories to expose in sketch
CommunicationInterface* createCommunication() { return new HeltecLoRa(); }
LEDInterface* createLEDs() { return new Pca9685LEDs(0x40, 0x60, true); }
TimeInterface* createTimeIf() { return new ArduinoTime(); }
