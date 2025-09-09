#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LoRaWan_APP.h>
#include "interfaces.h"
#include "protocol.h"

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
  explicit Pca9685LEDs(uint8_t addr1 = 0x40, uint8_t addr2 = 0x41, bool useSecond = true)
      : _pwm1(addr1), _pwm2(addr2), _useSecond(useSecond) {}
  void begin() override {
    _pwm1.begin();
    _pwm1.setPWMFreq(1000); // fast PWM for dimming
    if (_useSecond) {
      _pwm2.begin();
      _pwm2.setPWMFreq(1000);
    }
    setBrightness(1.0f);
  }
  void setBrightness(float b) override { _global = constrain(b, 0.0f, 1.0f); }
  void setLEDs(const float *values, size_t count) override {
    // Map first 16 channels to addr1, remaining to addr2
    for (size_t i = 0; i < count; ++i) {
      float v = constrain(values[i] * _global, 0.0f, 1.0f);
      uint16_t pwm = (uint16_t)(v * 4095.0f);
      if (i < 16) {
        _pwm1.setPWM((uint8_t)i, 0, pwm);
      } else if (_useSecond) {
        _pwm2.setPWM((uint8_t)(i - 16), 0, pwm);
      }
    }
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

  void onTxDone() { /* optional: set flags */ }
  void onTxTimeout() { Radio.Sleep(); Radio.Rx(0); }
  void onRxDone(uint8_t *payload, uint16_t size, int16_t, int8_t) {
    _rxSize = min<uint16_t>(size, sizeof(_rxBuf));
    memcpy(_rxBuf, payload, _rxSize);
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
LEDInterface* createLEDs() { return new Pca9685LEDs(0x40, 0x41, true); }
TimeInterface* createTimeIf() { return new ArduinoTime(); }
