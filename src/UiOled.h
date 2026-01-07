#pragma once
#include <Arduino.h>
#include "config.h"

class UiOled {
public:
  bool begin();
  void clear();
  void showBoot();

  // RUN: minimalist layout with icons + bar.
  void showRun(bool armed, bool failsafe, const char* activeText,
               uint32_t totalEvents, uint32_t totalTimeMs,
               uint8_t holdPct, const char* holdText);

  void showDiag(uint8_t gateIndex, uint16_t raw, bool calOk,
                uint16_t zeroRaw, uint16_t maxRaw,
                uint8_t barPct, const char* phaseText);

private:
  bool _ok = false;
};
