#pragma once
#include <Arduino.h>
#include "config.h"

class Buzzer {
public:
  enum Mode : uint8_t {
    MODE_SILENT = 0,
    MODE_RUN    = 1,
    MODE_DIAG   = 2
  };

  void begin(uint8_t pinA, uint8_t pinB);

  // Called often from loop()
  void update();

  void setMode(Mode m);
  void stopAll();

  // RUN: set desired global alarm level (0=off, 1=L1, 2=L2, 3=L3)
  void setRunLevel(uint8_t level);

  // RUN: trigger NEW sound (ignored if current effective level is L3)
  void triggerNewEvent();

  // DIAG: quality 0..100 (affects beep speed / continuous)
  void setDiagQualityPct(uint8_t pct);

private:
  // pins
  uint8_t _a = 0;
  uint8_t _b = 0;

  // mode/state
  Mode _mode = MODE_SILENT;

  // RUN state
  uint8_t _runLevel = 0;
  uint8_t _effLevel = 0;

  bool _newPending = false;
  uint32_t _newUntilMs = 0;

  // L2 pattern
  bool _l2On = false;
  uint32_t _l2NextMs = 0;

  // L3 pattern
  bool _l3Phase = false;
  uint32_t _l3NextMs = 0;

  // DIAG pattern
  uint8_t _diagPct = 0;
  bool _diagOn = false;
  uint32_t _diagNextMs = 0;

  // tone generator (differential square wave)
  uint16_t _freq = 0;
  bool _out = false;
  uint32_t _nextToggleUs = 0;

  void toneDiff(uint16_t freq);
  void noToneDiff();
  void serviceToneGen();

  void updateRun(uint32_t nowMs);
  void updateDiag(uint32_t nowMs);
};
