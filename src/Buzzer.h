#pragma once
#include <Arduino.h>
#include "config.h"

class Buzzer {
public:
  void begin(uint8_t pinA, uint8_t pinB);

  // Called often from loop()
  void update();

  // RUN: set desired global alarm level (0=off, 1=L1, 2=L2, 3=L3)
  void setRunLevel(uint8_t level);

  // RUN: trigger NEW sound (ignored if current effective level is L3) :contentReference[oaicite:17]{index=17}
  void triggerNewEvent();

  // DIAG: quality feedback 0..100 (will beep faster -> continuous tone)
  void setDiagQualityPct(uint8_t pct);

  // hard stop (DISARM / FAILSAFE)
  void stopAll();

  // Switch mode (RUN / DIAG)
  enum Mode : uint8_t { MODE_SILENT=0, MODE_RUN=1, MODE_DIAG=2 };
  void setMode(Mode m);

private:
  uint8_t _a=0,_b=0;

  Mode _mode = MODE_SILENT;

  // tone generator
  bool _out = false;
  uint32_t _nextToggleUs = 0;
  uint16_t _freq = 0; // 0 = off

  void toneDiff(uint16_t freq);
  void noToneDiff();
  void serviceToneGen();

  // RUN sound state
  uint8_t _runLevel = 0;      // requested level 0..3
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

  void updateRun(uint32_t nowMs);
  void updateDiag(uint32_t nowMs);
};
