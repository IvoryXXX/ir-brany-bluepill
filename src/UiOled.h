#pragma once
#include <Arduino.h>
#include "config.h"

class UiOled {
public:
  void begin();

  // holdPct: 0..100 (progress to DIAG)
  // holdText: short label rendered inside/near the bar (e.g. "PUSŤ=DIAG")
  void showRun(bool armed, bool failsafe, const char* activeText,
               uint32_t sumEvents, uint32_t sumTimeMs,
               uint8_t holdPct = 0, const char* holdText = nullptr);

  void showDiag(uint8_t gateIndex, uint16_t raw, bool calOk,
                uint16_t zeroRaw, uint16_t maxRaw,
                uint8_t barPct, const char* phaseText);
};
