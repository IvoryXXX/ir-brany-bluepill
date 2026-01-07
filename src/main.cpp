#include <Arduino.h>
#include "config.h"
#include "Gate.h"
#include "Buzzer.h"
#include "UiOled.h"
#include "Storage.h"

// ---------------------- Debounced switch (INPUT_PULLUP, active LOW) ----------------------
struct DebouncedPin {
  uint8_t pin = 0;
  uint8_t stable = HIGH;
  uint8_t last = HIGH;
  uint32_t lastChangeMs = 0;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    stable = digitalRead(pin);
    last = stable;
    lastChangeMs = millis();
  }

  void update(uint32_t nowMs, uint32_t debounceMs = 30) {
    uint8_t r = digitalRead(pin);
    if (r != last) {
      last = r;
      lastChangeMs = nowMs;
    }
    if ((nowMs - lastChangeMs) >= debounceMs) {
      stable = last;
    }
  }

  bool isOn() const { return stable == LOW; } // pressed
};

// ---------------------- Globals ----------------------
static Storage storage;
static StoredState st;

static Gate gates[GATE_COUNT];
static Buzzer buzzer;
static UiOled ui;

static DebouncedPin t1;
static DebouncedPin t2;

enum Mode : uint8_t { MODE_RUN = 0, MODE_DIAG = 1 };
static Mode mode = MODE_RUN;

// DIAG state
static uint8_t selectedGate = 0;
static bool diagSaveZeroPhase = true;
static uint32_t t2PressMs = 0;
static uint8_t lastBarPct = 0;

// RUN state
static uint32_t failsafeUntilMs = 0;

// RESET clicks window (RUN only)
static uint32_t clickWindowStartMs = 0;
static uint8_t  clickCount = 0;

static void resetTimeEventsAll() {
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (!gateEnabled(i)) continue;
    st.events[i] = 0;
    st.timeMs[i] = 0;
  }
  storage.save(st);
}

static void enterDiag(uint32_t nowMs) {
  mode = MODE_DIAG;
  t2PressMs = 0;
  lastBarPct = 0;
  clickWindowStartMs = 0;
  clickCount = 0;
  buzzer.setMode(Buzzer::MODE_DIAG);
}

static void exitDiagToRun(uint32_t nowMs) {
  mode = MODE_RUN;
  failsafeUntilMs = nowMs + FAILSAFE_AFTER_DIAG_MS;
  buzzer.setMode(Buzzer::MODE_SILENT);
  buzzer.stopAll();
  storage.save(st);
}

static bool isFailsafe(uint32_t nowMs) {
  return (failsafeUntilMs != 0 && (int32_t)(nowMs - failsafeUntilMs) < 0);
}

static bool isArmed() {
  return t1.isOn();
}

// ---------------------- RUN update ----------------------
static void runModeUpdate(uint32_t nowMs, uint8_t holdPct, const char* holdText) {
  const bool failsafe = isFailsafe(nowMs);
  const bool armed = (!failsafe) && isArmed();

  if (!armed) {
    buzzer.setMode(Buzzer::MODE_SILENT);
    buzzer.stopAll();

    uint32_t sumE = 0, sumT = 0;
    for (uint8_t i = 0; i < GATE_COUNT; i++) {
      if (!gateEnabled(i)) continue;
      sumE += st.events[i];
      sumT += st.timeMs[i];
    }
    ui.showRun(false, failsafe, failsafe ? "FAILSAFE" : "DISARM", sumE, sumT, holdPct, holdText);
    return;
  }

  buzzer.setMode(Buzzer::MODE_RUN);

  uint8_t newEventsThisLoop = 0;
  uint8_t globalLevel = 0;

  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (!gateEnabled(i)) continue;
    if (!gates[i].isCalibrated()) continue;

    gates[i].runUpdate(nowMs);

    if (gates[i].justBroken()) {
      st.events[i]++;
      newEventsThisLoop++;
    }

    const uint32_t bd = gates[i].brokenDurationMs(nowMs);
    if (bd >= RUN_L3_TIME_MS) globalLevel = max<uint8_t>(globalLevel, 3);
    else if (bd >= RUN_L2_TIME_MS) globalLevel = max<uint8_t>(globalLevel, 2);
    else if (gates[i].isBroken()) globalLevel = max<uint8_t>(globalLevel, 1);
  }

  static uint32_t lastRunMs = 0;
  if (lastRunMs == 0) lastRunMs = nowMs;
  uint32_t dt = nowMs - lastRunMs;
  lastRunMs = nowMs;

  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (!gateEnabled(i)) continue;
    if (!gates[i].isCalibrated()) continue;
    if (gates[i].isBroken()) st.timeMs[i] += dt;
  }

  buzzer.setRunLevel(globalLevel);
  if (newEventsThisLoop > 0) {
    buzzer.triggerNewEvent();
    storage.save(st);
  }

  uint32_t sumE = 0, sumT = 0;
  uint8_t active = 0;
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (!gateEnabled(i)) continue;
    sumE += st.events[i];
    sumT += st.timeMs[i];
    if (gates[i].isBroken()) active++;
  }

  static char activeBuf[8];
  const char* activeText = "OK";
  if (active > 0) {
    snprintf(activeBuf, sizeof(activeBuf), "+%u", (unsigned)active);
    activeText = activeBuf;
  }

  ui.showRun(true, false, activeText, sumE, sumT, holdPct, holdText);
}

// ---------------------- DIAG update ----------------------
static void diagModeUpdate(uint32_t nowMs) {
  buzzer.setMode(Buzzer::MODE_DIAG);

  static bool lastT1 = false;
  const bool t1Now = t1.isOn();
  if (t1Now && !lastT1) {
    for (uint8_t k = 0; k < GATE_COUNT; k++) {
      uint8_t cand = (uint8_t)((selectedGate + 1 + k) % GATE_COUNT);
      if (gateEnabled(cand)) { selectedGate = cand; break; }
    }
  }
  lastT1 = t1Now;

  const uint16_t raw = gates[selectedGate].readRaw(nowMs);
  GateCal cal = st.cal[selectedGate];

  const bool t2Pressed = t2.isOn();
  if (t2Pressed && t2PressMs == 0) t2PressMs = nowMs;

  uint8_t barPct = 0;
  if (t2Pressed && t2PressMs != 0) {
    uint32_t held = nowMs - t2PressMs;
    uint32_t pct = (held * 100u) / DIAG_BAR_FULL_MS;
    if (pct > 120) pct = 120;
    barPct = (uint8_t)pct;
    lastBarPct = barPct;
  } else {
    barPct = lastBarPct;
  }

  if (!t2Pressed && t2PressMs != 0) {
    const uint32_t held = nowMs - t2PressMs;
    t2PressMs = 0;

    uint32_t pct = (held * 100u) / DIAG_BAR_FULL_MS;

    if (pct < DIAG_BAR_NO_ACTION_PCT) {
      // no action
    } else if (pct >= DIAG_BAR_EXIT_PCT) {
      exitDiagToRun(nowMs);
      return;
    } else {
      if (diagSaveZeroPhase) cal.zeroRaw = raw;
      else cal.maxRaw = raw;

      gates[selectedGate].setCal(cal.zeroRaw, cal.maxRaw);
      st.cal[selectedGate] = cal;
      storage.save(st);

      diagSaveZeroPhase = !diagSaveZeroPhase;
    }
  }

  uint8_t qPct = 0;
  if (gates[selectedGate].isCalibrated()) qPct = gates[selectedGate].levelPct(raw);
  buzzer.setDiagQualityPct(qPct);

  const char* phase = diagSaveZeroPhase ? "SAVE ZERO" : "SAVE MAX";
  ui.showDiag(selectedGate, raw, gates[selectedGate].isCalibrated(),
              cal.zeroRaw, cal.maxRaw, barPct, phase);
}

void setup() {
  delay(50);
  storage.begin();

  memset(&st, 0, sizeof(st));
  st.magic = STORAGE_MAGIC;
  st.version = STORAGE_VERSION;

  StoredState loaded{};
  if (storage.load(loaded)) st = loaded;

  if (st.cfg.run_enterDiag_holdMs < 300) st.cfg.run_enterDiag_holdMs = 1500;
  if (st.cfg.run_enterDiag_holdMs > 10000) st.cfg.run_enterDiag_holdMs = 10000;
  if (st.cfg.run_reset_clicks < 3) st.cfg.run_reset_clicks = 10;
  if (st.cfg.run_reset_windowMs < 1000) st.cfg.run_reset_windowMs = 5000;

  t1.begin(T1_PIN);
  t2.begin(T2_PIN);

  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    gates[i].begin(GATE_ADC_PINS[i], &st.cfg);
    gates[i].setCal(st.cal[i].zeroRaw, st.cal[i].maxRaw);
  }

  buzzer.begin(BUZZ_A_PIN, BUZZ_B_PIN);
  buzzer.setMode(Buzzer::MODE_SILENT);
  ui.begin();

  mode = MODE_RUN;
  failsafeUntilMs = 0;
}

void loop() {
  const uint32_t nowMs = millis();

  t1.update(nowMs);
  t2.update(nowMs);

  // --- RUN: compute hold UI + handle gestures ---
  uint8_t holdPct = 0;
  const char* holdText = nullptr;

  if (mode == MODE_RUN) {
    const bool t2Pressed = t2.isOn();

    if (t2Pressed && t2PressMs == 0) t2PressMs = nowMs;

    if (t2Pressed && t2PressMs != 0) {
      const uint32_t held = nowMs - t2PressMs;
      const uint32_t enterDiagHoldMs = (st.cfg.run_enterDiag_holdMs < 300) ? 1500 : st.cfg.run_enterDiag_holdMs;

      uint32_t pct = (held * 100u) / enterDiagHoldMs;
      if (pct > 100) pct = 100;
      holdPct = (uint8_t)pct;

      holdText = (holdPct >= 100) ? "PUSŤ = DIAG" : "PUSŤ = KLIK (RESET)";
    }

    // release handling
    if (!t2Pressed && t2PressMs != 0) {
      const uint32_t held = nowMs - t2PressMs;
      t2PressMs = 0;

      const uint32_t enterDiagHoldMs = (st.cfg.run_enterDiag_holdMs < 300) ? 1500 : st.cfg.run_enterDiag_holdMs;

      if (held >= enterDiagHoldMs) {
        enterDiag(nowMs);
        return;
      }

      // short click -> count for RESET window
      if (clickWindowStartMs == 0 || (nowMs - clickWindowStartMs) > st.cfg.run_reset_windowMs) {
        clickWindowStartMs = nowMs;
        clickCount = 0;
      }
      clickCount++;
      if (clickCount >= st.cfg.run_reset_clicks) {
        resetTimeEventsAll();
        clickWindowStartMs = 0;
        clickCount = 0;
      }
    }
  }

  if (mode == MODE_RUN) runModeUpdate(nowMs, holdPct, holdText);
  else diagModeUpdate(nowMs);

  buzzer.update();
}
