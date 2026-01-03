#pragma once
#include <Arduino.h>
#include "config.h"

struct GateCal {
  uint16_t zeroRaw = 0;
  uint16_t maxRaw  = 0;
};

class Gate {
public:
  void begin(uint8_t adcPin, const AppConfig* cfg);

  // calibration endpoints (stored externally)
  void setCal(uint16_t zeroRaw, uint16_t maxRaw);
  GateCal getCal() const { return {_zeroRaw, _maxRaw}; }

  bool isCalibrated() const;

  // read ADC raw (filtered if enabled)
  uint16_t readRaw(uint32_t nowMs);

  // 0..1 and 0..100 "quality" derived from calibrated endpoints
  float   level01(uint16_t raw) const;
  uint8_t levelPct(uint16_t raw) const;

  // RUN logic (broken = beam interrupted)
  void runUpdate(uint32_t nowMs);
  bool isBroken() const { return _broken; }
  bool justBroken() const { return _justBroken; }
  bool justOk() const { return _justOk; }
  uint32_t brokenDurationMs(uint32_t nowMs) const;

private:
  uint8_t _pin = 0;
  const AppConfig* _cfg = nullptr;

  // filter state
  uint16_t _lastRaw = 0;
  uint32_t _lastSampleMs = 0;
  bool _emaInit = false;
  float _ema = 0.0f;

  // calibration endpoints
  uint16_t _zeroRaw = 0;
  uint16_t _maxRaw  = 0;

  // runtime state
  bool _broken = false;
  bool _justBroken = false;
  bool _justOk = false;
  uint32_t _brokenSinceMs = 0;

  uint16_t spanRaw() const;
  uint16_t applyFilter(uint16_t raw, uint32_t nowMs);

  // adjust raw meaning if hardware is inverted
  uint16_t normalizeRaw(uint16_t raw) const;

  void computeThresholds(uint16_t& onThr, uint16_t& offThr) const;
};
