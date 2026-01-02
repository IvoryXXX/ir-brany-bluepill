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

  void update(uint32_t nowMs, uint32_t debounceMs=30) {
    uint8_t r = digitalRead(pin);
    if (r != last) {
      last = r;
      lastChangeMs = nowMs;
    }
    if ((nowMs - lastChangeMs) >= debounceMs && stable != r) {
      stable = r;
    }
  }

  bool isOn() const { return stable == LOW; } // because pullup active LOW
};

// ---------------------- App state ----------------------
enum AppMode : uint8_t { MODE_RUN=0, MODE_DIAG=1 };

static Storage storage;
static StoredState st;

static UiOled ui;
static Buzzer buzzer;

static AppMode mode = MODE_RUN;
static uint32_t failsafeUntilMs = 0;

static DebouncedPin t1;
static DebouncedPin t2;

// Gates
static Gate gates[GATE_COUNT];

// RUN counters (persisted per tvoje zadání)
static inline void resetTimeEventsAll() {
  for (uint8_t i=0;i<GATE_COUNT;i++){
    st.events[i] = 0;
    st.timeMs[i] = 0;
  }
  storage.save(st);
}

// Gesture tracking for RUN reset / enter DIAG
static uint32_t t2PressMs = 0;
static uint32_t clickWindowStartMs = 0;
static uint8_t clickCount = 0;

// DIAG state
static uint8_t selectedGate = 0;
static bool diagSaveZeroPhase = true; // ZERO -> MAX -> ZERO ... :contentReference[oaicite:22]{index=22}

static uint8_t clampU8(int v){ if(v<0) return 0; if(v>255) return 255; return (uint8_t)v; }

static void buildActiveText(char* out, size_t outSz, const AppConfig& cfg, bool armed) {
  if (!armed) { snprintf(out, outSz, "-"); return; }

  // list up to ui_active_maxShown
  uint8_t shown = 0;
  uint8_t total = 0;

  out[0] = 0;
  char tmp[8];

  for (uint8_t i=0;i<GATE_COUNT;i++){
    if (!gates[i].isCalibrated()) continue;
    if (!gates[i].isBroken()) continue;
    total++;
    if (shown < cfg.ui_active_maxShown) {
      snprintf(tmp, sizeof(tmp), "G%u ", (unsigned)(i+1));
      strncat(out, tmp, outSz-1);
      shown++;
    }
  }

  if (total == 0) {
    snprintf(out, outSz, "-");
    return;
  }

  if (total > shown) {
    char plus[10];
    snprintf(plus, sizeof(plus), "+%u", (unsigned)(total - shown));
    strncat(out, plus, outSz-1);
  } else {
    // trim last space
    size_t n = strlen(out);
    if (n && out[n-1]==' ') out[n-1]=0;
  }
}

static void enterDiag(uint32_t nowMs) {
  mode = MODE_DIAG;
  buzzer.setMode(Buzzer::MODE_DIAG);
  // selected gate stays
  diagSaveZeroPhase = true;
  ui.showBoot();
}

static void exitDiagToRun(uint32_t nowMs) {
  // FAILSAFE after DIAG exit: behaves like DISARM for 5s :contentReference[oaicite:23]{index=23}
  mode = MODE_RUN;
  failsafeUntilMs = nowMs + FAILSAFE_AFTER_DIAG_MS;
  buzzer.setMode(Buzzer::MODE_SILENT);
  storage.save(st);
}

static bool isFailsafe(uint32_t nowMs) {
  return (failsafeUntilMs != 0 && (int32_t)(nowMs - failsafeUntilMs) < 0);
}

static bool isArmed() {
  // RUN has DISARM/ARM by T1 position :contentReference[oaicite:24]{index=24}
  return t1.isOn();
}

// ---------------------- setup ----------------------
void setup() {
  delay(50);
  storage.begin();

  // init default state
  memset(&st, 0, sizeof(st));
  st.magic = STORAGE_MAGIC;
  st.version = STORAGE_VERSION;
  // st.cfg defaults are from struct initializer

  // load from EEPROM if valid
  StoredState loaded{};
  if (storage.load(loaded)) st = loaded;

  // buttons
  t1.begin(T1_PIN);
  t2.begin(T2_PIN);

  // gates
  for (uint8_t i=0;i<GATE_COUNT;i++){
    gates[i].begin(GATE_ADC_PINS[i], &st.cfg);
    gates[i].setCal(st.cal[i].zeroRaw, st.cal[i].maxRaw);
  }

  // UI + buzzer
  ui.begin();
  ui.showBoot();

  buzzer.begin(BUZZ_A_PIN, BUZZ_B_PIN);
  buzzer.setMode(Buzzer::MODE_SILENT);

  // start in RUN, but if we had been in DIAG previously, failsafe is handled on exit only
  mode = MODE_RUN;
  failsafeUntilMs = 0;
}

// ---------------------- loop helpers ----------------------
static void runModeUpdate(uint32_t nowMs) {
  const bool failsafe = isFailsafe(nowMs);
  const bool armed = (!failsafe) && isArmed();

  // In DISARM or FAILSAFE: absolute silence + nothing counts :contentReference[oaicite:25]{index=25}
  if (!armed) {
    buzzer.setMode(Buzzer::MODE_SILENT);
  } else {
    buzzer.setMode(Buzzer::MODE_RUN);
  }

  // Handle T2 gestures only in RUN (implementation choice; DIAG has different rules)
  // Track press/release
  const bool t2Pressed = t2.isOn();
  if (t2Pressed && t2PressMs == 0) {
    t2PressMs = nowMs;
  }
  if (!t2Pressed && t2PressMs != 0) {
    // released
    const uint32_t held = nowMs - t2PressMs;
    t2PressMs = 0;

    // Long hold -> enter DIAG (UX/implementation; not contradicting MASTER)
    if (held >= st.cfg.run_enterDiag_holdMs) {
      enterDiag(nowMs);
      return;
    }

    // Short click -> count for RESET clicks window
    if (clickWindowStartMs == 0 || (nowMs - clickWindowStartMs) > st.cfg.run_reset_windowMs) {
      clickWindowStartMs = nowMs;
      clickCount = 0;
    }
    clickCount++;
    if (clickCount >= st.cfg.run_reset_clicks) {
      resetTimeEventsAll();
      clickCount = 0;
      clickWindowStartMs = 0;
    }
  }

  // If not armed: do not evaluate gates
  if (!armed) {
    char active[32]; buildActiveText(active, sizeof(active), st.cfg, false);
    // show totals (persisted) even if disarmed
    uint32_t sumE=0, sumT=0;
    for(uint8_t i=0;i<GATE_COUNT;i++){ sumE += st.events[i]; sumT += st.timeMs[i]; }
    ui.showRun(false, failsafe, active, sumE, sumT);
    return;
  }

  // RUN evaluation: update gates, count EVENTS/TIME, build ACTIVE, drive sound priorities :contentReference[oaicite:26]{index=26}
  uint8_t newEventsThisLoop = 0;
  uint8_t globalLevel = 0;

  for (uint8_t i=0;i<GATE_COUNT;i++){
    // RUN ignores uncalibrated gates completely :contentReference[oaicite:27]{index=27}
    if (!gates[i].isCalibrated()) continue;

    const bool wasBroken = gates[i].isBroken();
    gates[i].runUpdate(nowMs);

    if (gates[i].justBroken()) {
      st.events[i]++;              // EVENTS increments only on OK->BROKEN :contentReference[oaicite:28]{index=28}
      newEventsThisLoop++;
    }

    // TIME sums per-gate broken time; parallel interruptions add :contentReference[oaicite:29]{index=29}
    if (gates[i].isBroken()) {
      // add dt since last loop roughly using millis step:
      // We don't have per-gate lastMs; simplest: add fixed "loop dt" requires more scaffolding.
      // We'll do per-gate integration with a static lastNow.
    }

    // compute global alarm level based on broken duration thresholds
    const uint32_t bd = gates[i].brokenDurationMs(nowMs);
    if (bd >= RUN_L3_TIME_MS) globalLevel = max<uint8_t>(globalLevel, 3);
    else if (bd >= RUN_L2_TIME_MS) globalLevel = max<uint8_t>(globalLevel, 2);
    else if (gates[i].isBroken()) globalLevel = max<uint8_t>(globalLevel, 1);
  }

  // TIME integration (simple and correct): use a single dt per loop, add to each broken gate
  static uint32_t lastRunMs = 0;
  if (lastRunMs == 0) lastRunMs = nowMs;
  uint32_t dt = nowMs - lastRunMs;
  lastRunMs = nowMs;
  for (uint8_t i=0;i<GATE_COUNT;i++){
    if (!gates[i].isCalibrated()) continue;
    if (gates[i].isBroken()) st.timeMs[i] += dt;
  }

  // Sound rules:
  // - Sound only in ARM :contentReference[oaicite:30]{index=30}
  // - NEW sound on each new interruption; interrupts state sound; not played if global L3 :contentReference[oaicite:31]{index=31}
  // - If multiple gates break in same instant -> NEW once
  buzzer.setRunLevel(globalLevel);
  if (newEventsThisLoop > 0 && globalLevel < 3) {
    buzzer.triggerNewEvent();
  }

  // UI
  char active[32]; buildActiveText(active, sizeof(active), st.cfg, true);
  uint32_t sumE=0, sumT=0;
  for(uint8_t i=0;i<GATE_COUNT;i++){ sumE += st.events[i]; sumT += st.timeMs[i]; }
  ui.showRun(true, false, active, sumE, sumT);

  // persist occasionally (cheap safety)
  static uint32_t lastSaveMs = 0;
  if (nowMs - lastSaveMs > 2000) {
    // refresh stored cals too (if changed in DIAG)
    for (uint8_t i=0;i<GATE_COUNT;i++){
      st.cal[i] = gates[i].getCal();
    }
    storage.save(st);
    lastSaveMs = nowMs;
  }
}

static void diagModeUpdate(uint32_t nowMs) {
  // In DIAG: T1 each change -> next gate :contentReference[oaicite:32]{index=32}
  static uint8_t lastT1Stable = HIGH;
  if (t1.stable != lastT1Stable) {
    lastT1Stable = t1.stable;
    selectedGate = (uint8_t)((selectedGate + 1) % GATE_COUNT);
  }

  // Read current gate raw
  uint16_t raw = gates[selectedGate].readRaw(nowMs);
  bool calOk = gates[selectedGate].isCalibrated();
  GateCal cal = gates[selectedGate].getCal();

  // DIAG: T2 actions evaluated ONLY on release :contentReference[oaicite:33]{index=33}
  const bool t2Pressed = t2.isOn();

  // BAR = how long T2 held mapped to 0..100+ (>=100 triggers EXIT zone)
  uint8_t barPct = 0;
  if (t2Pressed) {
    if (t2PressMs == 0) t2PressMs = nowMs;
    uint32_t held = nowMs - t2PressMs;
    uint32_t pct = (held * 100u) / DIAG_BAR_FULL_MS;
    if (pct > 255u) pct = 255u;
    barPct = (uint8_t)pct;
  }

  if (!t2Pressed && t2PressMs != 0) {
    uint32_t held = nowMs - t2PressMs;
    t2PressMs = 0;

    uint32_t pct = (held * 100u) / DIAG_BAR_FULL_MS;

    if (pct < DIAG_BAR_NO_ACTION_PCT) {
      // no action
    } else if (pct >= DIAG_BAR_EXIT_PCT) {
      // EXIT to RUN :contentReference[oaicite:34]{index=34}
      exitDiagToRun(nowMs);
      return;
    } else {
      // save ZERO/MAX depending on phase (ZERO->MAX->ZERO...) :contentReference[oaicite:35]{index=35}
      if (diagSaveZeroPhase) {
        cal.zeroRaw = raw;
      } else {
        cal.maxRaw = raw;
      }
      gates[selectedGate].setCal(cal.zeroRaw, cal.maxRaw);
      st.cal[selectedGate] = cal;
      storage.save(st);
      diagSaveZeroPhase = !diagSaveZeroPhase;
    }
  }

  // DIAG sound: quality from LEVEL_PCT if calibrated, else no smooth feedback :contentReference[oaicite:36]{index=36}
  uint8_t qPct = 0;
  if (gates[selectedGate].isCalibrated()) {
    qPct = gates[selectedGate].levelPct(raw);
  } else {
    // If uncalibrated: we can still give crude feedback using raw span guess, but MASTER says
    // no smooth sound response for UNCALIBRATED :contentReference[oaicite:37]{index=37}
    qPct = 0;
  }
  buzzer.setDiagQualityPct(qPct);

  const char* phase = diagSaveZeroPhase ? "SAVE ZERO" : "SAVE MAX";
  ui.showDiag(selectedGate, raw, gates[selectedGate].isCalibrated(), cal.zeroRaw, cal.maxRaw, barPct, phase);
}

// ---------------------- main loop ----------------------
void loop() {
  const uint32_t nowMs = millis();

  t1.update(nowMs);
  t2.update(nowMs);

  if (mode == MODE_RUN) {
    runModeUpdate(nowMs);
  } else {
    diagModeUpdate(nowMs);
  }

  buzzer.update();
}
 