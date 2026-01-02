#pragma once
#include <Arduino.h>
#include "config.h"

class UiOled {
public:
  bool begin();
  void clear();
  void showBoot();
  void showRun(bool armed, bool failsafe, const char* activeText, uint32_t totalEvents, uint32_t totalTimeMs);
  void showDiag(uint8_t gateIndex, uint16_t raw, bool calibrated, uint16_t zeroRaw, uint16_t maxRaw, uint8_t barPct, const char* phaseText);

private:
  bool _ok = false;
};
