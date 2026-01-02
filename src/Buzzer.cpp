#include "Buzzer.h"

void Buzzer::begin(uint8_t pinA, uint8_t pinB) {
  _a = pinA; _b = pinB;
  pinMode(_a, OUTPUT);
  pinMode(_b, OUTPUT);
  digitalWrite(_a, LOW);
  digitalWrite(_b, LOW);
  _mode = MODE_SILENT;
  _freq = 0;
}

void Buzzer::setMode(Mode m) {
  if (_mode == m) return;
  _mode = m;
  // reset patterns
  _newPending = false;
  _newUntilMs = 0;
  _l2On = false; _l2NextMs = 0;
  _l3Phase = false; _l3NextMs = 0;
  _diagOn = false; _diagNextMs = 0;
  if (_mode == MODE_SILENT) stopAll();
}

void Buzzer::stopAll() {
  _runLevel = 0;
  _newPending = false;
  _newUntilMs = 0;
  noToneDiff();
  digitalWrite(_a, LOW);
  digitalWrite(_b, LOW);
}

void Buzzer::setRunLevel(uint8_t level) {
  if (level > 3) level = 3;
  _runLevel = level;
}

void Buzzer::triggerNewEvent() {
  // NEW sound must NOT play if globální_level == L3 :contentReference[oaicite:18]{index=18}
  if (_runLevel >= 3) return;
  _newPending = true;
}

void Buzzer::setDiagQualityPct(uint8_t pct) {
  if (pct > 100) pct = 100;
  _diagPct = pct;
}

void Buzzer::toneDiff(uint16_t freq) {
  _freq = freq;
  if (_freq == 0) {
    noToneDiff();
    return;
  }
  // start toggling immediately
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

void Buzzer::update() {
  const uint32_t nowMs = millis();

  if (_mode == MODE_SILENT) {
    noToneDiff();
    return;
  }

  if (_mode == MODE_RUN) updateRun(nowMs);
  else if (_mode == MODE_DIAG) updateDiag(nowMs);

  serviceToneGen();
}

void Buzzer::updateRun(uint32_t nowMs) {
  // RUN sound only when ARM (main decides to be RUN+ARM) :contentReference[oaicite:19]{index=19}
  if (_runLevel == 0) {
    noToneDiff();
    return;
  }

  // NEW sound: short, distinct, interrupts state sound, not during L3 :contentReference[oaicite:20]{index=20}
  if (_newPending) {
    _newPending = false;
    _newUntilMs = nowMs + 120;
    toneDiff(BUZ_FREQ_NEW);
    return;
  }
  if (_newUntilMs != 0) {
    if (nowMs < _newUntilMs) return; // keep chirp
    _newUntilMs = 0;                 // resume state sound
  }

  // L3 absolute priority :contentReference[oaicite:21]{index=21}
  if (_runLevel >= 3) {
    if (_l3NextMs == 0 || nowMs >= _l3NextMs) {
      _l3Phase = !_l3Phase;
      toneDiff(_l3Phase ? BUZ_FREQ_L3_A : BUZ_FREQ_L3_B);
      _l3NextMs = nowMs + 250;
    }
    return;
  }

  if (_runLevel == 2) {
    // fast beeping
    if (_l2NextMs == 0 || nowMs >= _l2NextMs) {
      _l2On = !_l2On;
      if (_l2On) toneDiff(BUZ_FREQ_L2);
      else noToneDiff();
      _l2NextMs = nowMs + (_l2On ? 120 : 80);
    }
    return;
  }

  // L1: continuous tone
  toneDiff(BUZ_FREQ_L1);
}

void Buzzer::updateDiag(uint32_t nowMs) {
  // DIAG: tone/beep based on signal quality (0..100)
  if (_diagPct >= 100) _diagPct = 100;

  if (_diagPct >= 0 && _diagPct >=  (uint8_t)0) {
    if (_diagPct >= 0) { /* no-op */ }
  }

  if (_diagPct >= 0 && _diagPct >= 255) { /* impossible */ }

  if (_diagPct >= 0 && _diagPct >= 100) { /* handled below */ }

  if (_diagPct >= 0) {
    // full tone at configured threshold
    if (_diagPct >= 85) { // default threshold; main can map to cfg if desired
      toneDiff(880);
      return;
    }
  }

  // Map 0..84 -> period between min..max
  // Using config defaults here; can be refined by main passing mapping.
  const uint16_t minP = 800;
  const uint16_t maxP = 120;

  uint8_t pct = _diagPct;
  if (pct > 84) pct = 84;
  // inverse mapping: low pct -> slow
  const uint16_t period = (uint16_t)(minP - (uint32_t)(minP - maxP) * (uint32_t)pct / 84u);

  if (_diagNextMs == 0 || nowMs >= _diagNextMs) {
    _diagOn = !_diagOn;
    if (_diagOn) toneDiff(660);
    else noToneDiff();
    _diagNextMs = nowMs + (_diagOn ? period/3 : (period*2)/3);
  }
}
