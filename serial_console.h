#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "anim_schema.h"

struct ConsoleAdapter {
  void* user{nullptr};
  void (*sendSync)(void* user) = nullptr;
  void (*setBrightness)(void* user, float b) = nullptr;
  void (*applyFollowerCfg2)(void* user, uint8_t animIndex, const uint8_t* ids, const float* vals, uint8_t count) = nullptr;
};

class SerialConsole {
 public:
  void begin(const ConsoleAdapter& a){ adapter_ = a; state_ = Idle; bufLen_ = 0; promptShown_ = false; }
  void loop(){
    if (!adapter_.sendSync) return; // not initialized
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') { handleLine(); bufLen_ = 0; lineBuf_[0] = '\0'; return; }
      if (bufLen_ + 1 < sizeof(lineBuf_)) { lineBuf_[bufLen_++] = c; lineBuf_[bufLen_] = '\0'; }
    }
  }
 private:
  enum State { Idle, AwaitCmd, AnimSelect, ParamEntry, BrightnessEntry };
  State state_{Idle};
  ConsoleAdapter adapter_{};
  char lineBuf_[128]{}; size_t bufLen_{0};
  bool promptShown_{false};

  // param entry context
  uint8_t curAnim_{0};
  uint8_t curParamIds_[16]{}; float curParamVals_[16]{}; uint8_t curParamCount_{0}; uint8_t curParamIdx_{0};

  static bool isBlank(const char* s){ while(*s){ if(!isspace((unsigned char)*s)) return false; ++s; } return true; }
  static int toInt(const char* s){ return atoi(s); }
  static float toFloat(const char* s){ return (float)atof(s); }

  void printMenu(){
    Serial.println("Commands:");
    Serial.println("  help            - show this menu");
    Serial.println("  sync            - send SYNC now");
    Serial.println("  bright [0..1]  - set brightness");
    Serial.println("  anim            - change follower animation & params");
    Serial.print("> ");
  }

  void handleLine(){
    // If idle and user hit enter, show menu and go to AwaitCmd
    if (state_ == Idle) { printMenu(); state_ = AwaitCmd; return; }

    switch(state_){
      case AwaitCmd: handleCmd(); break;
      case BrightnessEntry: handleBrightnessEntry(); break;
      case AnimSelect: handleAnimSelect(); break;
      case ParamEntry: handleParamEntry(); break;
      default: state_ = AwaitCmd; break;
    }
  }

  void handleCmd(){
    if (isBlank(lineBuf_)) { printMenu(); return; }
    if (startsWith(lineBuf_, "help")) { printMenu(); return; }
    if (startsWith(lineBuf_, "sync")) { if (adapter_.sendSync) adapter_.sendSync(adapter_.user); Serial.println("Sent SYNC"); Serial.print("> "); return; }
    if (startsWith(lineBuf_, "bright")) {
      // parse optional value
      const char* p = skipWord(lineBuf_); while(*p==' ') ++p; if (*p) {
        float v = toFloat(p); if (adapter_.setBrightness) adapter_.setBrightness(adapter_.user, constrain(v,0.0f,1.0f));
        Serial.print("brightness set to "); Serial.println(v,3); Serial.print("> ");
      } else {
        Serial.print("brightness (0..1) > "); state_ = BrightnessEntry;
      }
      return;
    }
    if (startsWith(lineBuf_, "anim")) {
      listAnimations();
      Serial.print("anim index > "); state_ = AnimSelect; return;
    }
    Serial.println("Unknown command. Type 'help'."); Serial.print("> ");
  }

  void handleBrightnessEntry(){
    if (isBlank(lineBuf_)) { Serial.print("> "); state_ = AwaitCmd; return; }
    float v = toFloat(lineBuf_); if (adapter_.setBrightness) adapter_.setBrightness(adapter_.user, constrain(v,0.0f,1.0f));
    Serial.print("brightness set to "); Serial.println(v,3); Serial.print("> "); state_ = AwaitCmd;
  }

  void handleAnimSelect(){
    if (isBlank(lineBuf_)) { Serial.print("> "); state_ = AwaitCmd; return; }
    int idx = toInt(lineBuf_);
    const AnimSchema::AnimDef* adPGM = AnimSchema::findAnim((uint8_t)idx);
    if (!adPGM) { Serial.println("invalid index"); Serial.print("> "); state_ = AwaitCmd; return; }
    // load params
    AnimSchema::AnimDef ad; memcpy_P(&ad, adPGM, sizeof(ad));
    curAnim_ = (uint8_t)idx; curParamCount_ = ad.paramCount; curParamIdx_ = 0;
    if (curParamCount_ > 16) curParamCount_ = 16;
    for (uint8_t i=0;i<curParamCount_;++i){ curParamIds_[i] = pgm_read_byte(ad.paramIds + i); curParamVals_[i] = getDefault(curParamIds_[i]); }
    promptNextParam(); state_ = ParamEntry;
  }

  void handleParamEntry(){
    // blank -> keep default and move on
    if (!isBlank(lineBuf_)) { curParamVals_[curParamIdx_] = toFloat(lineBuf_); }
    curParamIdx_++;
    if (curParamIdx_ >= curParamCount_) {
      // send
      if (adapter_.applyFollowerCfg2) adapter_.applyFollowerCfg2(adapter_.user, curAnim_, curParamIds_, curParamVals_, curParamCount_);
      Serial.println("applied"); Serial.print("> "); state_ = AwaitCmd; return;
    }
    promptNextParam();
  }

  void promptNextParam(){
    uint8_t pid = curParamIds_[curParamIdx_];
    AnimSchema::ParamDef pd; const AnimSchema::ParamDef* pdPGM = AnimSchema::findParam(pid);
    if (pdPGM) memcpy_P(&pd, pdPGM, sizeof(pd)); else { memset(&pd,0,sizeof(pd)); pd.id=pid; pd.name="p"; }
    Serial.print(pd.name); Serial.print(" ["); Serial.print(pd.minVal,3); Serial.print(", "); Serial.print(pd.maxVal,3); Serial.print("] def="); Serial.print(pd.defVal,3); Serial.print(" > ");
  }

  float getDefault(uint8_t pid){ AnimSchema::ParamDef pd; const AnimSchema::ParamDef* pdPGM=AnimSchema::findParam(pid); if (pdPGM){ memcpy_P(&pd,pdPGM,sizeof(pd)); return pd.defVal; } return 0.0f; }

  static bool startsWith(const char* s, const char* p){ size_t n=strlen(p); return strncasecmp(s,p,n)==0; }
  static const char* skipWord(const char* s){ while(*s && !isspace((unsigned char)*s)) ++s; return s; }

  void listAnimations(){
    Serial.println("Animations:");
    for (size_t i=0;i<sizeof(AnimSchema::ANIMS)/sizeof(AnimSchema::ANIMS[0]); ++i){
      AnimSchema::AnimDef ad; memcpy_P(&ad, &AnimSchema::ANIMS[i], sizeof(ad));
      Serial.print("  "); Serial.print(ad.index); Serial.print(": "); Serial.println(ad.name);
    }
  }
};

#endif // ARDUINO
