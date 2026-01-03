#include "Buzzer.h"

void Buzzer::begin(uint8_t pinA, uint8_t pinB) {
  _a = pinA;
  _b = pinB;

  pinMode(_a, OUTPUT);
  pinMode(_b, OUTPUT);
  digitalWrite(_a, LOW);
  digitalWrite(_b, LOW);

  _mode = MODE_SILENT;

  _runLevel = 0;
  _effLevel = 0;

  _newPending = false;
  _newUntilMs = 0;

  _l2On = false;
  _l2NextMs = 0;

  _l3Phase = false;
  _l3NextMs = 0;

  _diagPct = 0;
  _diagOn = false;
  _diagNextMs = 0;

  _freq = 0;
  _out = false;
  _nextToggleUs = micros();
}

void Buzzer::setMode(Mode m) {
  if (_mode == m) return;
  _mode = m;

  // reset patterns to avoid “ghost beeps”
  _newPending = false;
  _newUntilMs = 0;

  _l2On = false;
  _l2NextMs = 0;

  _l3Phase = false;
  _l3NextMs = 0;

  _diagOn = false;
  _diagNextMs = 0;

  noToneDiff();
}

void Buzzer::stopAll() {
  _newPending = false;
  _newUntilMs = 0;

  _runLevel = 0;
  _effLevel = 0;

  _diagPct = 0;
  _diagOn = false;
  _diagNextMs = 0;

  _l2On = false;
  _l2NextMs = 0;

  _l3Phase = false;
  _l3NextMs = 0;

  noToneDiff();
}

void Buzzer::setRunLevel(uint8_t level) {
  if (level > 3) level = 3;
  _runLevel = level;
}

void Buzzer::triggerNewEvent() {
  if (_mode != MODE_RUN) return;
  if (_effLevel >= 3) return; // L3 has priority
  _newPending = true;
}

void Buzzer::setDiagQualityPct(uint8_t pct) {
  if (pct > 100) pct = 100;
  _diagPct = pct;
}

// ---------------- differential square wave ----------------

void Buzzer::toneDiff(uint16_t freq) {
  _freq = freq;
  if (_freq == 0) {
    noToneDiff();
    return;
  }
  _out = false;
  _nextToggleUs = micros();
}

void Buzzer::noToneDiff() {
  _freq = 0;
  digitalWrite(_a, LOW);
  digitalWrite(_b, LOW);
}

void Buzzer::serviceToneGen() {
  if (_freq == 0) return;

  const uint32_t nowUs = micros();
  if ((int32_t)(nowUs - _nextToggleUs) >= 0) {
    _out = !_out;

    // differential drive
    digitalWrite(_a, _out ? HIGH : LOW);
    digitalWrite(_b, _out ? LOW  : HIGH);

    // half-period in microseconds
    const uint32_t halfPeriodUs = 500000UL / (uint32_t)_freq;
    _nextToggleUs = nowUs + halfPeriodUs;
  }
}

// ------------------------------------------------------------

void Buzzer::update() {
  const uint32_t nowMs = millis();

  if (_mode == MODE_SILENT) {
    noToneDiff();
    return;
  }

  if (_mode == MODE_RUN) updateRun(nowMs);
  else updateDiag(nowMs);

  serviceToneGen();
}

void Buzzer::updateRun(uint32_t nowMs) {
  _effLevel = _runLevel;

  // NEW beep has priority unless L3
  if (_newPending && _effLevel < 3) {
    _newPending = false;
    _newUntilMs = nowMs + 120;
    toneDiff(BUZ_FREQ_NEW);
    return;
  }

  if (_newUntilMs != 0) {
    if ((int32_t)(nowMs - _newUntilMs) < 0) return; // still playing NEW
    _newUntilMs = 0;
  }

  if (_effLevel == 0) {
    noToneDiff();
    return;
  }

  if (_effLevel == 1) {
    toneDiff(BUZ_FREQ_L1);
    return;
  }

  if (_effLevel == 2) {
    // L2: 90ms on / 90ms off
    if (_l2NextMs == 0 || (int32_t)(nowMs - _l2NextMs) >= 0) {
      _l2On = !_l2On;
      if (_l2On) {
        toneDiff(BUZ_FREQ_L2);
        _l2NextMs = nowMs + 90;
      } else {
        noToneDiff();
        _l2NextMs = nowMs + 90;
      }
    }
    return;
  }

  // L3: alternating A/B
  if (_l3NextMs == 0 || (int32_t)(nowMs - _l3NextMs) >= 0) {
    _l3Phase = !_l3Phase;
    toneDiff(_l3Phase ? BUZ_FREQ_L3_A : BUZ_FREQ_L3_B);
    _l3NextMs = nowMs + 220;
  }
}

void Buzzer::updateDiag(uint32_t nowMs) {
  const uint8_t pct = _diagPct;

  if (pct < 10) {
    noToneDiff();
    _diagOn = false;
    _diagNextMs = 0;
    return;
  }

  if (pct >= 85) {
    // continuous (reuse L2 freq; config.h has no dedicated DIAG freq)
    toneDiff(BUZ_FREQ_L2);
    _diagOn = true;
    return;
  }

  // 10..84%: beep speed increases with pct
  const uint16_t minPeriod = 800;
  const uint16_t maxPeriod = 120;
  const uint8_t spanPct = 74; // 84-10

  const uint16_t period =
      (uint16_t)(minPeriod - (uint32_t)(minPeriod - maxPeriod) * (uint32_t)(pct - 10) / spanPct);

  if (_diagNextMs == 0 || (int32_t)(nowMs - _diagNextMs) >= 0) {
    _diagOn = !_diagOn;
    if (_diagOn) {
      toneDiff(BUZ_FREQ_L2);
      _diagNextMs = nowMs + (period / 3);
    } else {
      noToneDiff();
      _diagNextMs = nowMs + ((period * 2) / 3);
    }
  }
}
