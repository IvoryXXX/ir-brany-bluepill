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

  // raw read
  uint16_t readRaw(uint32_t nowMs);

  // derived values (valid only if calibrated)
  bool isCalibrated() const;
  uint16_t spanRaw() const;

  // normalized LEVEL (0..1) and pct (0..100) for calibrated gates
  float level01(uint16_t diffRaw) const;
  uint8_t levelPct(uint16_t diffRaw) const;

  // RUN break detection state machine (OK/BROKEN) for calibrated gates only
  void runUpdate(uint32_t nowMs);

  bool isBroken() const { return _broken; }
  bool justBroken() const { return _justBroken; } // OK->BROKEN edge
  bool justOk() const { return _justOk; }         // BROKEN->OK edge

  // how long current BROKEN lasts (ms), 0 if OK
  uint32_t brokenDurationMs(uint32_t nowMs) const;

  // Access last sample
  uint16_t lastRaw() const { return _lastRaw; }

private:
  uint8_t _pin = 0;
  const AppConfig* _cfg = nullptr;

  uint16_t _zeroRaw = 0;
  uint16_t _maxRaw  = 0;

  uint16_t _lastRaw = 0;

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
