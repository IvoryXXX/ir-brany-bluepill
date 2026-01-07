#pragma once
#include <Arduino.h>
#include "config.h"

class UiOled {
public:
  void begin();

  void showRun(bool armed, bool failsafe, const char* activeText, uint32_t sumEvents, uint32_t sumTimeMs);
  void showDiag(uint8_t gateIndex, uint16_t raw, bool calOk, uint16_t zeroRaw, uint16_t maxRaw,
                uint8_t barPct, const char* phaseText);
};
