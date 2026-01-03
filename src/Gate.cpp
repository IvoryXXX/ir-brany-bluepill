#include "Gate.h"

void Gate::begin(uint8_t adcPin, const AppConfig* cfg) {
  _pin = adcPin;
  _cfg = cfg;
  pinMode(_pin, INPUT_ANALOG);

  _lastRaw = 0;
  _lastSampleMs = 0;
  _emaInit = false;
  _ema = 0.0f;

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
  return _cfg && span >= _cfg->cal_minSpanRaw;
}

uint16_t Gate::spanRaw() const {
  const uint16_t lo = (_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw;
  const uint16_t hi = (_zeroRaw < _maxRaw) ? _maxRaw : _zeroRaw;
  return (uint16_t)(hi - lo);
}

uint16_t Gate::applyFilter(uint16_t raw, uint32_t nowMs) {
  if (!_cfg || _cfg->filter_type == AppConfig::NONE) return raw;

  // sample period gate
  if (_lastSampleMs != 0 && (nowMs - _lastSampleMs) < _cfg->filter_samplePeriodMs) {
    return _lastRaw;
  }
  _lastSampleMs = nowMs;

  if (!_emaInit) {
    _ema = (float)raw;
    _emaInit = true;
  } else {
    // use configured alpha (fallback to 0.25 if out of range)
    float alpha = _cfg->filter_emaAlpha;
    if (!(alpha > 0.0f && alpha <= 1.0f)) alpha = 0.25f;
    _ema = _ema + alpha * ((float)raw - _ema);
  }

  return (uint16_t)lroundf(_ema);
}

uint16_t Gate::normalizeRaw(uint16_t raw) const {
#if defined(GATE_RAW_INVERT) && (GATE_RAW_INVERT == 1)
  // Invert raw meaning using calibrated endpoints.
  // raw' = lo+hi-raw  (maps low<->high, keeps thresholds logic intact)
  if (!isCalibrated()) return raw;
  const uint16_t lo = (_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw;
  const uint16_t hi = (_zeroRaw < _maxRaw) ? _maxRaw : _zeroRaw;
  const uint32_t sum = (uint32_t)lo + (uint32_t)hi;
  uint32_t inv = (sum >= raw) ? (sum - (uint32_t)raw) : 0u;
  if (inv > 4095u) inv = 4095u; // STM32 ADC is 12-bit
  return (uint16_t)inv;
#else
  return raw;
#endif
}

uint16_t Gate::readRaw(uint32_t nowMs) {
  uint16_t r = analogRead(_pin);
  r = applyFilter(r, nowMs);
  _lastRaw = r;
  return r;
}

float Gate::level01(uint16_t raw) const {
  if (!isCalibrated()) return 0.0f;

  // IMPORTANT: use normalized raw so "higher = better" stays true even if hardware is inverted
  const uint16_t r = normalizeRaw(raw);

  const float lo = (float)((_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw);
  const float hi = (float)((_zeroRaw < _maxRaw) ? _maxRaw : _zeroRaw);
  const float denom = (hi - lo);
  if (denom <= 0.0f) return 0.0f;

  float v = ((float)r - lo) / denom;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return v;
}

uint8_t Gate::levelPct(uint16_t raw) const {
  float v = level01(raw);
  int p = (int)lroundf(v * 100.0f);
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (uint8_t)p;
}

void Gate::computeThresholds(uint16_t& onThr, uint16_t& offThr) const {
  // tolerate swapped ZERO/MAX in EEPROM:
  const uint16_t lo = (_zeroRaw < _maxRaw) ? _zeroRaw : _maxRaw;
  const uint16_t span = spanRaw();

  // break_onPct is the "bad low band" width from lo
  const uint32_t on  = (uint32_t)lo + (uint32_t)span * (uint32_t)_cfg->break_onPct / 100u;
  const uint32_t off = (uint32_t)lo + (uint32_t)span * (uint32_t)(_cfg->break_onPct + _cfg->break_hystPct) / 100u;

  onThr  = (uint16_t)on;
  offThr = (uint16_t)off;
}

void Gate::runUpdate(uint32_t nowMs) {
  _justBroken = _justOk = false;

  const uint16_t raw0 = readRaw(nowMs);
  if (!isCalibrated()) {
    _broken = false;
    _brokenSinceMs = 0;
    return;
  }

  // normalize meaning if needed (fixes "gate reversed" symptom)
  const uint16_t raw = normalizeRaw(raw0);

  uint16_t onThr, offThr;
  computeThresholds(onThr, offThr);

  if (!_broken) {
    if (raw <= onThr) {
      _broken = true;
      _justBroken = true; // OK -> BROKEN edge
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
