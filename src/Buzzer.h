#pragma once
#include <Arduino.h>
#include "config.h"

class Buzzer {
public:
  enum Mode : uint8_t { MODE_SILENT = 0, MODE_RUN = 1, MODE_DIAG = 2 };

  void begin(uint8_t pinA, uint8_t pinB);

  void setMode(Mode m);
  void setRunLevel(uint8_t lvl);      // 0..3
  void triggerNewEvent();             // audible tick for each new event
  void setDiagQualityPct(uint8_t pct);

  void stopAll();
  void update();

private:
  uint8_t _pinA = 255;
  uint8_t _pinB = 255;
  Mode _mode = MODE_SILENT;

  // RUN
  uint8_t _runLevel = 0;
  bool _newEventPulse = false;
  uint32_t _newEventUntilMs = 0;

  // DIAG
  uint8_t _diagQ = 0;
  uint32_t _diagNextToggleMs = 0;
  bool _diagBeepOn = false;

  void hwTone(uint16_t freq);
  void hwNoTone();

  void runUpdate(uint32_t nowMs);
  void diagUpdate(uint32_t nowMs);
};
