#include "Buzzer.h"

void Buzzer::begin(uint8_t pinA, uint8_t pinB) {
  _pinA = pinA;
  _pinB = pinB;

  pinMode(_pinA, OUTPUT);
  pinMode(_pinB, OUTPUT);

  digitalWrite(_pinA, LOW);
  digitalWrite(_pinB, LOW);

  _mode = MODE_SILENT;
  _runLevel = 0;
  _newEventPulse = false;
  _newEventUntilMs = 0;

  _diagQ = 0;
  _diagNextToggleMs = 0;
  _diagBeepOn = false;
}

void Buzzer::setMode(Mode m) {
  if (_mode == m) return;
  _mode = m;
  stopAll();
}

void Buzzer::setRunLevel(uint8_t lvl) {
  if (lvl > 3) lvl = 3;
  _runLevel = lvl;
}

void Buzzer::triggerNewEvent() {
  _newEventPulse = true;
  _newEventUntilMs = millis() + 120;
}

void Buzzer::setDiagQualityPct(uint8_t pct) {
  _diagQ = pct;
}

void Buzzer::stopAll() {
  _newEventPulse = false;
  _newEventUntilMs = 0;

  _diagBeepOn = false;
  _diagNextToggleMs = 0;

  hwNoTone();
}

void Buzzer::hwTone(uint16_t freq) {
#if (BUZZ_DIFFERENTIAL == 1)
  // Differential push-pull: drive one pin with PWM, other inverted.
  // On STM32 Arduino core, tone() uses a timer on ONE pin, so we fake differential by toggling B.
  // We still get much cleaner sound by keeping B opposite state during tone bursts.
  tone(_pinA, freq);
  digitalWrite(_pinB, HIGH); // static opposite (good enough for piezo between pins)
#else
  tone(_pinA, freq);
#endif
}

void Buzzer::hwNoTone() {
  noTone(_pinA);
#if (BUZZ_DIFFERENTIAL == 1)
  digitalWrite(_pinB, LOW);
#endif
}

void Buzzer::runUpdate(uint32_t nowMs) {
  if (_newEventPulse) {
    if ((int32_t)(nowMs - _newEventUntilMs) < 0) {
      hwTone(BUZ_FREQ_NEW);
      return;
    } else {
      _newEventPulse = false;
      hwNoTone();
    }
  }

  switch (_runLevel) {
    case 0:
      hwNoTone();
      break;
    case 1:
      hwTone(BUZ_FREQ_L1);
      break;
    case 2: {
      // fast beeping
      uint16_t period = 180;
      bool on = ((nowMs / period) % 2) == 0;
      if (on) hwTone(BUZ_FREQ_L2);
      else hwNoTone();
    } break;
    case 3: {
      // alternating melody-ish
      uint16_t period = 250;
      bool a = ((nowMs / period) % 2) == 0;
      hwTone(a ? BUZ_FREQ_L3_A : BUZ_FREQ_L3_B);
    } break;
  }
}

void Buzzer::diagUpdate(uint32_t nowMs) {
  // if not calibrated/low quality -> slow beep, if high -> continuous
  uint8_t q = _diagQ;
  if (q >= 100) q = 100;

  if (q >= 85) {
    hwTone(BUZ_FREQ_L1);
    return;
  }

  // Map quality to period: worse => slower
  uint16_t pMin = 800;
  uint16_t pMax = 120;
  uint16_t period = (uint16_t)(pMin - (uint32_t)(pMin - pMax) * (uint32_t)q / 100u);
  if (period < pMax) period = pMax;
  if (period > pMin) period = pMin;

  if (_diagNextToggleMs == 0) _diagNextToggleMs = nowMs;

  if ((int32_t)(nowMs - _diagNextToggleMs) >= 0) {
    _diagBeepOn = !_diagBeepOn;
    _diagNextToggleMs = nowMs + (uint32_t)(period / 2);
  }

  if (_diagBeepOn) hwTone(BUZ_FREQ_L1);
  else hwNoTone();
}

void Buzzer::update() {
  uint32_t nowMs = millis();

  if (_mode == MODE_SILENT) {
    hwNoTone();
    return;
  }
  if (_mode == MODE_RUN) {
    runUpdate(nowMs);
    return;
  }
  if (_mode == MODE_DIAG) {
    diagUpdate(nowMs);
    return;
  }
}
