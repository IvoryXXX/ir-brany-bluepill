#pragma once
#include <Arduino.h>
#include "config.h"

class UiOled {
public:
  bool begin();
  void clear();
  void showBoot();

  // RUN: posuvný seznam START, STOP, RESET + aktivní brány (jen zapojené a nakalibrované).
  static constexpr uint8_t RUN_MENU_LINES = 6;

  void showRun(bool sessionRunning, uint32_t sessionMs,
               const uint32_t events2s[GATE_COUNT], const uint32_t events3s[GATE_COUNT],
               uint8_t menuSel, uint8_t firstVisible, uint8_t menuTotalItems,
               const uint8_t visibleGateIdx[GATE_COUNT], uint8_t visibleGateCount,
               uint8_t resetHoldPct, const uint8_t gateBarPct[GATE_COUNT],
               const uint8_t gateRunPhase[GATE_COUNT], uint32_t uiNowMs);

  void showDiag(uint8_t gateIndex, bool hwConnected, uint16_t raw, bool calOk,
                uint16_t zeroRaw, uint16_t maxRaw,
                uint8_t barFillPct, bool barUsesSavedEndpoints,
                const char* phaseText, int8_t selectedLine);

  void showMenuPage(const char* title,
                    const char* line1,
                    const char* line2,
                    const char* line3,
                    const char* line4,
                    int8_t selectedLine);

  static constexpr uint8_t SETTINGS_ITEM_COUNT = 4;
  void showSettingsSplash(bool perGate, uint8_t gateIdx);
  void showSettingsDetail(uint8_t index, uint8_t total, const char* label, const char* valueStr,
                          const char* footerLine, bool editing);

private:
  bool _ok = false;
};
