#include "Buzzer.h"
#if defined(ARDUINO_ARCH_STM32)
#include <HardwareTimer.h>
#endif

Buzzer* Buzzer::s_inst = nullptr;

static inline uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void Buzzer::begin(uint8_t pinA, uint8_t pinB) {
  _a = pinA;
  _b = pinB;

  pinMode(_a, OUTPUT);
  pinMode(_b, OUTPUT);
  digitalWrite(_a, LOW);
  digitalWrite(_b, LOW);

  _mode = MODE_SILENT;

  _runLevel = 0;
  _runNextMs = 0;
  _runOn = false;
  _runAlt = false;
  _newUntilMs = 0;

  _diagPct = 0;
  _diagOn = false;
  _diagNextMs = 0;

  _freq = 0;
  _out = false;
  _softNextUs = micros();

  s_inst = this;

#if defined(ARDUINO_ARCH_STM32)
  // BluePill: TIM4 je obvykle safe volba.
  _tmr = new HardwareTimer(TIM4);
  _tmr->pause();
  _tmr->attachInterrupt(Buzzer::isrThunk);
#endif
}

void Buzzer::setMode(Mode m) {
  if (_mode == m) return;
  _mode = m;

  _runNextMs = 0;
  _runOn = false;
  _runAlt = false;

  _diagNextMs = 0;
  _diagOn = false;

  if (_mode == MODE_SILENT) noToneOut();
}

void Buzzer::stopAll() {
  _runLevel = 0;
  _newUntilMs = 0;

  _diagPct = 0;
  _diagNextMs = 0;
  _diagOn = false;

  noToneOut();
}

void Buzzer::setRunLevel(uint8_t level) {
  if (level > 3) level = 3;
  _runLevel = level;
}

void Buzzer::triggerNewEvent() {
  _newUntilMs = millis() + 70;
}

void Buzzer::setDiagQualityPct(uint8_t pct) {
  if (pct > 100) pct = 100;
  _diagPct = pct;
}

void Buzzer::update() {
  const uint32_t nowMs = millis();

  if (_mode == MODE_SILENT) {
    noToneOut();
  } else if (_mode == MODE_RUN) {
    updateRun(nowMs);
  } else {
    updateDiag(nowMs);
  }

#if !defined(ARDUINO_ARCH_STM32)
  softService();
#endif
}

// ---------------- output control ----------------

void Buzzer::toneOut(uint16_t freq) {
  if (freq == 0) { noToneOut(); return; }
  if (_freq == freq) return;

  _freq = freq;

#if defined(ARDUINO_ARCH_STM32)
  timerSetFreq(freq);
#else
  _softNextUs = micros();
#endif
}

void Buzzer::noToneOut() {
  _freq = 0;
#if defined(ARDUINO_ARCH_STM32)
  timerStop();
#endif
  _out = false;
  digitalWrite(_a, LOW);
  digitalWrite(_b, LOW);
}

// ---------------- RUN patterns ----------------

void Buzzer::updateRun(uint32_t nowMs) {
  if (_runLevel == 0) { noToneOut(); return; }

  const bool newChirp = (_newUntilMs != 0) && ((int32_t)(nowMs - _newUntilMs) < 0);
  if (newChirp) { toneOut(BUZ_FREQ_NEW); return; }

  if (_runLevel == 1) { toneOut(BUZ_FREQ_L1); return; }

  if (_runLevel == 2) {
    if (_runNextMs == 0 || (int32_t)(nowMs - _runNextMs) >= 0) {
      _runOn = !_runOn;
      if (_runOn) { toneOut(BUZ_FREQ_L2); _runNextMs = nowMs + 120; }
      else        { noToneOut();          _runNextMs = nowMs + 80;  }
    }
    return;
  }

  if (_runNextMs == 0 || (int32_t)(nowMs - _runNextMs) >= 0) {
    _runAlt = !_runAlt;
    toneOut(_runAlt ? BUZ_FREQ_L3_A : BUZ_FREQ_L3_B);
    _runNextMs = nowMs + 150;
  }
}

// ---------------- DIAG pattern ----------------

void Buzzer::updateDiag(uint32_t nowMs) {
  const uint8_t fullAtPct = 85;
  const uint16_t slowMs = 900;
  const uint16_t fastMs = 140;

  if (_diagPct >= fullAtPct) {
    toneOut(BUZ_FREQ_L2);
    _diagNextMs = 0;
    _diagOn = true;
    return;
  }

  uint8_t p = _diagPct;
  if (p < 5) p = 5;

  const uint32_t period = slowMs - (uint32_t)(slowMs - fastMs) * (uint32_t)p / 85U;
  const uint32_t per = clampU32(period, fastMs, slowMs);

  if (_diagNextMs == 0 || (int32_t)(nowMs - _diagNextMs) >= 0) {
    _diagOn = !_diagOn;
    if (_diagOn) { toneOut(BUZ_FREQ_L2); _diagNextMs = nowMs + per/3; }
    else         { noToneOut();          _diagNextMs = nowMs + (per*2)/3; }
  }
}

// ---------------- Timer (STM32) ----------------

void Buzzer::isrThunk() {
  if (s_inst) s_inst->isrTick();
}

void Buzzer::isrTick() {
  _out = !_out;

#if BUZZ_DIFFERENTIAL
  digitalWrite(_a, _out ? HIGH : LOW);
  digitalWrite(_b, _out ? LOW  : HIGH);
#else
  digitalWrite(_a, _out ? HIGH : LOW);
  digitalWrite(_b, LOW);
#endif
}

void Buzzer::timerStop() {
#if defined(ARDUINO_ARCH_STM32)
  if (_tmr) _tmr->pause();
#endif
}

void Buzzer::timerSetFreq(uint16_t freq) {
#if defined(ARDUINO_ARCH_STM32)
  if (!_tmr) return;
  if (freq == 0) { timerStop(); return; }

  // ISR musí běžet na 2*freq (přepíná se každou půlperiodu)
  _tmr->setOverflow((uint32_t)freq * 2UL, HERTZ_FORMAT);
  _tmr->resume();
#endif
}

// ---------------- Soft fallback (non-STM32) ----------------

void Buzzer::softService() {
  if (_freq == 0) return;

  const uint32_t nowUs = micros();
  if ((int32_t)(nowUs - _softNextUs) < 0) return;

  _out = !_out;

#if BUZZ_DIFFERENTIAL
  digitalWrite(_a, _out ? HIGH : LOW);
  digitalWrite(_b, _out ? LOW  : HIGH);
#else
  digitalWrite(_a, _out ? HIGH : LOW);
  digitalWrite(_b, LOW);
#endif

  const uint32_t halfPeriodUs = 1000000UL / ((uint32_t)_freq * 2UL);
  _softNextUs = nowUs + halfPeriodUs;
}
