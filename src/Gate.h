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

  // set calibration (stored externally)
  void setCal(uint16_t zeroRaw, uint16_t maxRaw);
  GateCal getCal() const { return {_zeroRaw, _maxRaw}; }

  // read ADC raw (filtered if enabled)
  uint16_t readRaw(uint32_t nowMs);

  // calibration helpers
  bool isCalibrated() const;
  uint16_t spanRaw() const;

  // DIAG helpers
  float level01(uint16_t raw) const;
  uint8_t levelPct(uint16_t diffRaw) const;

  // RUN state
  void runUpdate(uint32_t nowMs);
  bool isBroken() const { return _broken; }
  bool justBroken() const { return _justBroken; }
  bool justOk() const { return _justOk; }
  uint32_t brokenDurationMs(uint32_t nowMs) const;

private:
  uint8_t _pin = 0;
  const AppConfig* _cfg = nullptr;

  // last raw
  uint16_t _lastRaw = 0;

  // calibration endpoints
  uint16_t _zeroRaw = 0;
  uint16_t _maxRaw  = 0;

  // filter
  uint32_t _lastSampleMs = 0;
  float _ema = 0.0f;
  bool  _emaInit = false;

  // RUN state
  bool _broken = false;
  bool _justBroken = false;
  bool _justOk = false;
  uint32_t _brokenSinceMs = 0;

  uint16_t applyFilter(uint16_t raw, uint32_t nowMs);
  void computeThresholds(uint16_t& onThr, uint16_t& offThr) const;
};
