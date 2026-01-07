#pragma once
#include <Arduino.h>
#include "config.h"

// Pasivní piezo potřebuje stabilní periodu.
// Na STM32 (BluePill) to řešíme HW timerem (ISR), aby OLED/loop nezanesly jitter.

class Buzzer {
public:
  enum Mode : uint8_t { MODE_SILENT = 0, MODE_RUN = 1, MODE_DIAG = 2 };

  void begin(uint8_t pinA, uint8_t pinB);

  void setMode(Mode m);
  void stopAll();

  void setRunLevel(uint8_t level);      // 0..3
  void triggerNewEvent();               // krátký chirp
  void setDiagQualityPct(uint8_t pct);  // 0..100

  void update(); // volej často z loop()

private:
  uint8_t _a = 0xFF;
  uint8_t _b = 0xFF;

  Mode _mode = MODE_SILENT;

  // RUN
  uint8_t  _runLevel = 0;
  uint32_t _runNextMs = 0;
  bool     _runOn = false;
  bool     _runAlt = false;
  uint32_t _newUntilMs = 0;

  // DIAG
  uint8_t  _diagPct = 0;
  bool     _diagOn = false;
  uint32_t _diagNextMs = 0;

  // generator
  uint16_t _freq = 0;

  void toneOut(uint16_t freq);
  void noToneOut();

  void updateRun(uint32_t nowMs);
  void updateDiag(uint32_t nowMs);

  // timer-backed generator (STM32)
  void timerStop();
  void timerSetFreq(uint16_t freq);

  // fallback (non-STM32)
  void softService();

  static void isrThunk();
  void isrTick();

  volatile bool _out = false;
  volatile uint32_t _softNextUs = 0;

#if defined(ARDUINO_ARCH_STM32)
  class HardwareTimer* _tmr = nullptr;
#endif

  static Buzzer* s_inst;
};
