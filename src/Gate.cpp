#include "Gate.h"

void Gate::begin(uint8_t adcPin, const AppConfig* cfg) {
  _pin = adcPin;
  _cfg = cfg;
  pinMode(_pin, INPUT_ANALOG);
  _lastRaw = 0;
  _lastSampleMs = 0;
  _emaInit = false;
  _broken = false;
  _justBroken = _justOk = false;
  _brokenSinceMs = 0;
}

void Gate::setCal(uint16_t zeroRaw, uint16_t maxRaw) {
  _zeroRaw = zeroRaw;
  _maxRaw  = maxRaw;
}

bool Gate::isCalibrated() const {
  const uint16_t span = spanRaw();
  return span >= _cfg->cal_minSpanRaw;
}

uint16_t Gate::spanRaw() const {
  const uint16_t lo = (_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw;
  const uint16_t hi = (_zeroRaw < _maxRaw) ? _maxRaw : _zeroRaw;
  return (uint16_t)(hi - lo);
}

uint16_t Gate::applyFilter(uint16_t raw, uint32_t nowMs) {
  if (_cfg->filter_type == AppConfig::NONE) return raw;

  // sample period gate
  if (_lastSampleMs != 0 && (nowMs - _lastSampleMs) < _cfg->filter_samplePeriodMs) {
    return _lastRaw;
  }
  _lastSampleMs = nowMs;

  if (!_emaInit) {
    _ema = (float)raw;
    _emaInit = true;
  } else {
    // EMA alpha derived from sample period (simple fixed factor)
    const float alpha = 0.25f;
    _ema = _ema + alpha * ((float)raw - _ema);
  }
  const uint16_t f = (uint16_t)lroundf(_ema);
  return f;
}

uint16_t Gate::readRaw(uint32_t nowMs) {
  uint16_t r = analogRead(_pin);
  r = applyFilter(r, nowMs);
  _lastRaw = r;
  return r;
}

float Gate::level01(uint16_t raw) const {
  if (!isCalibrated()) return 0.0f;
  const float lo = (float)((_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw);
  const float hi = (float)((_zeroRaw < _maxRaw) ? _maxRaw : _zeroRaw);
  const float denom = (hi - lo);
  if (denom <= 0.0f) return 0.0f;
  float v = ((float)raw - lo) / denom;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return v;
}

uint8_t Gate::levelPct(uint16_t diffRaw) const {
  float v = level01(diffRaw);
  int p = (int)lroundf(v * 100.0f);
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (uint8_t)p;
}

void Gate::computeThresholds(uint16_t& onThr, uint16_t& offThr) const {
  // We tolerate swapped ZERO/MAX in EEPROM:
  // Use low-end as baseline and treat "broken" as low-level condition.
  const uint16_t lo = (_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw;
  const uint16_t span = spanRaw();

  const uint32_t on  = (uint32_t)lo + (uint32_t)span * (uint32_t)_cfg->break_onPct / 100u;
  const uint32_t off = (uint32_t)lo + (uint32_t)span * (uint32_t)(_cfg->break_onPct + _cfg->break_hystPct) / 100u;

  onThr  = (uint16_t)on;
  offThr = (uint16_t)off;
}

void Gate::runUpdate(uint32_t nowMs) {
  _justBroken = _justOk = false;

  const uint16_t raw = readRaw(nowMs);
  if (!isCalibrated()) {
    _broken = false;
    _brokenSinceMs = 0;
    return;
  }

  uint16_t onThr, offThr;
  computeThresholds(onThr, offThr);

  if (!_broken) {
    if (raw <= onThr) {
      _broken = true;
      _justBroken = true;  // OK -> BROKEN edge
      _brokenSinceMs = nowMs;
    }
  } else {
    if (raw >= offThr) {
      _broken = false;
      _justOk = true;
      _brokenSinceMs = 0;
    }
  }
}

uint32_t Gate::brokenDurationMs(uint32_t nowMs) const {
  if (!_broken || _brokenSinceMs == 0) return 0;
  return nowMs - _brokenSinceMs;
}
