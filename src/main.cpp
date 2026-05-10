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

  bool isOn() const { return stable == LOW; } // ON = pressed (to GND)
};

// ---------------------- Globals ----------------------
static Storage storage;
static StoredState st;

static Gate gates[GATE_COUNT];
static Buzzer buzzer;
static UiOled ui;

static DebouncedPin t1;
static DebouncedPin t2;
static DebouncedPin t3;
static DebouncedPin t4;

enum MenuPage : uint8_t { PAGE_RUN = 0, PAGE_DIAG = 1, PAGE_SETTINGS = 2, PAGE_COUNT = 3 };
static MenuPage page = PAGE_RUN;

static bool settingsDetailActive = false;
static bool settingsEditMode = false;
static uint8_t settingsDetailIdx = 0;
static bool settingsGlobal = true;
static uint8_t settingsGateIdx = 0;
static bool settingsQuickReturn = false;

// DIAG state
static uint8_t diagGate = 0;           // DIAG can switch between all gates
static uint32_t lastDiagSaveMs = 0;
static char diagStatusText[22] = "";
static uint8_t diagCursor = 0;

enum DiagMenuItem : uint8_t {
  DIAG_NEXT_GATE = 0,
  DIAG_SAVE_ZERO = 1,
  DIAG_SAVE_MAX = 2,
  DIAG_RESET_STATS = 3,
  DIAG_ITEM_COUNT = 4
};

// RUN session + menu na hlavní obrazovce
static bool sessionRunning = false;
static uint32_t sessionAccumMs = 0;
static uint32_t sessionSegmentStartMs = 0;

static uint8_t runMenuSel = 0;
static uint8_t runMenuFirstVisible = 0;
static uint8_t runMenuVisibleGateIdx[GATE_COUNT];
static uint8_t runMenuVisibleGateCount = 0;

static uint32_t resetHoldStartMs = 0;
static bool resetHoldDidFire = false;
static uint8_t runResetHoldPct = 0;

struct ButtonEdges {
  bool k1Pressed = false;
  bool k2Pressed = false;
  bool k3Pressed = false;
  bool k4Pressed = false;
};

static ButtonEdges readButtonEdges() {
  static bool prevK1 = false, prevK2 = false, prevK3 = false, prevK4 = false;
  ButtonEdges e{};
  const bool k1 = t1.isOn();
  const bool k2 = t2.isOn();
  const bool k3 = t3.isOn();
  const bool k4 = t4.isOn();
  e.k1Pressed = (k1 && !prevK1);
  e.k2Pressed = (k2 && !prevK2);
  e.k3Pressed = (k3 && !prevK3);
  e.k4Pressed = (k4 && !prevK4);
  prevK1 = k1;
  prevK2 = k2;
  prevK3 = k3;
  prevK4 = k4;
  return e;
}

static uint32_t sessionDisplayMs(uint32_t nowMs) {
  uint32_t t = sessionAccumMs;
  if (sessionRunning) t += (nowMs - sessionSegmentStartMs);
  return t;
}

// Slaby signal: hysteréze + od gateWeakSinceMs meritko slabosti (viz RUN_SIGNAL_* v config.h)
static bool gateWeakActive[GATE_COUNT];
static uint32_t gateWeakSinceMs[GATE_COUNT];
static bool gateRecorded2s[GATE_COUNT];
static bool gateRecorded3s[GATE_COUNT];

static void clearRunWeakState() {
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    gateWeakActive[i] = false;
    gateWeakSinceMs[i] = 0;
    gateRecorded2s[i] = false;
    gateRecorded3s[i] = false;
  }
}

static void sessionStop(uint32_t nowMs) {
  if (!sessionRunning) return;
  sessionAccumMs += (nowMs - sessionSegmentStartMs);
  sessionRunning = false;
  clearRunWeakState();
}

static void sessionStart(uint32_t nowMs) {
  if (sessionRunning) return;
  sessionRunning = true;
  sessionSegmentStartMs = nowMs;
  clearRunWeakState();
}

static void runSessionFullReset(uint32_t nowMs) {
  if (sessionRunning) {
    sessionAccumMs += (nowMs - sessionSegmentStartMs);
  }
  sessionRunning = false;
  sessionAccumMs = 0;
  clearRunWeakState();
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    st.events2s[i] = 0;
    st.events3s[i] = 0;
    st.timeMs[i] = 0;
  }
  storage.save(st);
}

static void updateRunResetHold(uint32_t nowMs) {
  runResetHoldPct = 0;
  if (page != PAGE_RUN || runMenuSel != 2) {
    resetHoldStartMs = 0;
    resetHoldDidFire = false;
    return;
  }
  if (!t4.isOn()) {
    resetHoldStartMs = 0;
    resetHoldDidFire = false;
    return;
  }
  if (resetHoldStartMs == 0) resetHoldStartMs = nowMs;
  const uint32_t held = nowMs - resetHoldStartMs;
  runResetHoldPct = (held >= RUN_RESET_HOLD_MS)
                        ? 100
                        : (uint8_t)((held * 100u) / RUN_RESET_HOLD_MS);
  if (held >= RUN_RESET_HOLD_MS && !resetHoldDidFire) {
    resetHoldDidFire = true;
    runSessionFullReset(nowMs);
  }
}

static uint8_t runMenuTotalItems() {
  return (uint8_t)(3u + runMenuVisibleGateCount);
}

static void rebuildRunMenuGateList() {
  runMenuVisibleGateCount = 0;
  for (uint8_t g = 0; g < GATE_COUNT; g++) {
    if (gateIsHwConnected(g) && gates[g].isCalibrated()) {
      runMenuVisibleGateIdx[runMenuVisibleGateCount++] = g;
    }
  }
}

static void clampRunMenuSelAndScroll() {
  const uint8_t total = runMenuTotalItems();
  if (total == 0) return;
  if (runMenuSel >= total) runMenuSel = (uint8_t)(total - 1u);

  constexpr uint8_t lines = UiOled::RUN_MENU_LINES;
  if (total <= lines) {
    runMenuFirstVisible = 0;
    return;
  }
  if (runMenuSel < runMenuFirstVisible) runMenuFirstVisible = runMenuSel;
  if (runMenuSel >= runMenuFirstVisible + lines)
    runMenuFirstVisible = (uint8_t)(runMenuSel - lines + 1u);
}

static uint8_t settingsActiveItemCount() {
  return UiOled::SETTINGS_ITEM_COUNT;
}

static void getEffectiveRunThresholds(uint8_t gateIdx, uint8_t& okPct, uint8_t& weakEnterPct,
                                      uint16_t& noScoreMs, uint16_t& velkyMs) {
  if (st.gateRun[gateIdx].use_global) {
    okPct = st.cfg.run_ok_min_pct;
    weakEnterPct = st.cfg.run_weak_enter_pct;
    noScoreMs = st.cfg.run_weak_no_score_ms;
    velkyMs = st.cfg.run_weak_velky_ms;
  } else {
    okPct = st.gateRun[gateIdx].run_ok_min_pct;
    weakEnterPct = st.gateRun[gateIdx].run_weak_enter_pct;
    noScoreMs = st.gateRun[gateIdx].run_weak_no_score_ms;
    velkyMs = st.gateRun[gateIdx].run_weak_velky_ms;
  }
}

static void gateProfileDetach(uint8_t g) {
  if (!st.gateRun[g].use_global) return;
  st.gateRun[g].use_global = false;
  st.gateRun[g].run_ok_min_pct = st.cfg.run_ok_min_pct;
  st.gateRun[g].run_weak_enter_pct = st.cfg.run_weak_enter_pct;
  st.gateRun[g].run_weak_no_score_ms = st.cfg.run_weak_no_score_ms;
  st.gateRun[g].run_weak_velky_ms = st.cfg.run_weak_velky_ms;
}

static void openGateSettings(uint8_t gateIdx) {
  settingsGlobal = false;
  settingsGateIdx = gateIdx;
  gateProfileDetach(gateIdx);
  settingsQuickReturn = true;
  settingsDetailActive = false;
  settingsEditMode = false;
  settingsDetailIdx = 0;
  page = PAGE_SETTINGS;
  storage.save(st);
}

// DIAG bar: empty at high raw, full at low raw. Fixed 900..15 until Z/M span is valid, then adaptive min(Z,M)..max(Z,M).
static uint8_t diagBarFillPct(uint16_t raw, uint16_t zeroRaw, uint16_t maxRaw, bool adaptiveSpan) {
  const uint16_t loEp = (zeroRaw < maxRaw) ? zeroRaw : maxRaw;
  const uint16_t hiEp = (zeroRaw < maxRaw) ? maxRaw : zeroRaw;
  uint16_t hi;
  uint16_t lo;
  if (adaptiveSpan && hiEp > loEp) {
    hi = hiEp;
    lo = loEp;
  } else {
    hi = DIAG_BAR_FIXED_HI_RAW;
    lo = DIAG_BAR_FIXED_LO_RAW;
  }
  if (hi <= lo) return 0;
  const int32_t num = (int32_t)hi - (int32_t)raw;
  const int32_t den = (int32_t)hi - (int32_t)lo;
  int32_t pct = (num * 100) / den;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t)pct;
}

static uint8_t gateBarPctRun[GATE_COUNT];
static uint8_t gateRunPhaseRun[GATE_COUNT];

static void fillRunGateBarPct(uint32_t nowMs) {
  for (uint8_t g = 0; g < GATE_COUNT; g++) {
    gateBarPctRun[g] = 0;
    if (!gateIsHwConnected(g) || !gates[g].isCalibrated()) continue;
    const uint16_t raw = gates[g].readRaw(nowMs);
    gateBarPctRun[g] =
        diagBarFillPct(raw, st.cal[g].zeroRaw, st.cal[g].maxRaw, true);
  }
}

static void showRunScreen(uint32_t nowMs, bool sessionRun, uint32_t sessMs) {
  fillRunGateBarPct(nowMs);
  ui.showRun(sessionRun, sessMs, st.events2s, st.events3s, runMenuSel, runMenuFirstVisible,
             runMenuTotalItems(), runMenuVisibleGateIdx, runMenuVisibleGateCount,
             runResetHoldPct, gateBarPctRun, gateRunPhaseRun, nowMs);
}

// ---------------------- RUN update ----------------------
static void runModeUpdate(uint32_t nowMs, bool renderRunScreen, bool controlBuzzer) {
  const uint32_t sessMs = sessionDisplayMs(nowMs);

  if (!sessionRunning) {
    clearRunWeakState();
    for (uint8_t i = 0; i < GATE_COUNT; i++) gateRunPhaseRun[i] = 0;
    if (controlBuzzer) {
      buzzer.setMode(Buzzer::MODE_SILENT);
      buzzer.stopAll();
    }
    if (renderRunScreen) showRunScreen(nowMs, false, sessMs);
    return;
  }

  if (controlBuzzer) buzzer.setMode(Buzzer::MODE_RUN);

  uint8_t buzzPhase = 0;
  bool needSave = false;

  static uint32_t lastRunMs = 0;
  if (lastRunMs == 0) lastRunMs = nowMs;
  uint32_t dt = nowMs - lastRunMs;
  lastRunMs = nowMs;

  for (uint8_t i = 0; i < GATE_COUNT; i++) gateRunPhaseRun[i] = 0;

  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    if (!gateIsHwConnected(i)) continue;
    if (!gates[i].isCalibrated()) continue;

    const uint16_t raw = gates[i].readRaw(nowMs);
    const uint8_t pct = gates[i].levelPct(raw);

    uint8_t okPct = 0;
    uint8_t weakEnterPct = 0;
    uint16_t noScoreMs = 0;
    uint16_t velkyMs = 0;
    getEffectiveRunThresholds(i, okPct, weakEnterPct, noScoreMs, velkyMs);

    if (!gateWeakActive[i]) {
      if (pct > weakEnterPct) {
        continue;
      }
      gateWeakActive[i] = true;
      gateWeakSinceMs[i] = nowMs;
      gateRecorded2s[i] = false;
      gateRecorded3s[i] = false;
    } else {
      if (pct >= okPct) {
        gateWeakActive[i] = false;
        gateWeakSinceMs[i] = 0;
        gateRecorded2s[i] = false;
        gateRecorded3s[i] = false;
        continue;
      }
    }

    const uint32_t weakMs = nowMs - gateWeakSinceMs[i];

    if (weakMs > noScoreMs && !gateRecorded2s[i]) {
      st.events2s[i]++;
      gateRecorded2s[i] = true;
      needSave = true;
    }
    if (weakMs >= velkyMs && !gateRecorded3s[i]) {
      st.events3s[i]++;
      gateRecorded3s[i] = true;
      needSave = true;
    }

    st.timeMs[i] += dt;

    uint8_t ph = 1;
    if (weakMs >= velkyMs) ph = 3;
    else if (weakMs > noScoreMs) ph = 2;
    gateRunPhaseRun[i] = ph;
    buzzPhase = max<uint8_t>(buzzPhase, ph);
  }

  if (controlBuzzer) buzzer.setRunLevel(buzzPhase);
  if (needSave) storage.save(st);

  if (renderRunScreen) showRunScreen(nowMs, true, sessMs);
}

// ---------------------- SETTINGS + DIAG update ----------------------
static void settingsAdjust(int8_t delta) {
  if (delta == 0) return;

  if (settingsGlobal) {
    AppConfig& c = st.cfg;
    switch (settingsDetailIdx) {
      case 0:
        c.run_ok_min_pct = (uint8_t)((int)c.run_ok_min_pct + (int)delta);
        break;
      case 1:
        c.run_weak_enter_pct = (uint8_t)((int)c.run_weak_enter_pct + (int)delta);
        break;
      case 2: {
        int v = (int)c.run_weak_no_score_ms + (int)delta * 10;
        if (v < 0) v = 0;
        if (v > 10000) v = 10000;
        c.run_weak_no_score_ms = (uint16_t)v;
        break;
      }
      case 3: {
        int v = (int)c.run_weak_velky_ms + (int)delta * 10;
        if (v < 0) v = 0;
        if (v > 10000) v = 10000;
        c.run_weak_velky_ms = (uint16_t)v;
        break;
      }
      default:
        break;
    }
    clampRunSettings(c);
    storage.save(st);
    return;
  }

  GateRunProfile& p = st.gateRun[settingsGateIdx];
  const uint8_t g = settingsGateIdx;

  gateProfileDetach(g);

  switch (settingsDetailIdx) {
    case 0:
      p.run_ok_min_pct = (uint8_t)((int)p.run_ok_min_pct + (int)delta);
      break;
    case 1:
      p.run_weak_enter_pct = (uint8_t)((int)p.run_weak_enter_pct + (int)delta);
      break;
    case 2: {
      int v = (int)p.run_weak_no_score_ms + (int)delta * 10;
      if (v < 0) v = 0;
      if (v > 10000) v = 10000;
      p.run_weak_no_score_ms = (uint16_t)v;
      break;
    }
    case 3: {
      int v = (int)p.run_weak_velky_ms + (int)delta * 10;
      if (v < 0) v = 0;
      if (v > 10000) v = 10000;
      p.run_weak_velky_ms = (uint16_t)v;
      break;
    }
    default:
      break;
  }
  clampGateRunProfile(p);
  storage.save(st);
}

static void settingsModeUpdate(uint32_t nowMs) {
  (void)nowMs;
  if (!settingsDetailActive) {
    ui.showSettingsSplash(!settingsGlobal, settingsGateIdx);
    return;
  }

  const uint8_t n = settingsActiveItemCount();
  if (settingsDetailIdx >= n) settingsDetailIdx = 0;

  char val[14];
  const char* lab = "";

  if (settingsGlobal) {
    switch (settingsDetailIdx) {
      case 0:
        lab = "OK paprsek min %";
        snprintf(val, sizeof(val), "%u", (unsigned)st.cfg.run_ok_min_pct);
        break;
      case 1:
        lab = "Vstup slabost %";
        snprintf(val, sizeof(val), "%u", (unsigned)st.cfg.run_weak_enter_pct);
        break;
      case 2:
        lab = "Ignor slabosti do (ms)";
        snprintf(val, sizeof(val), "%u", (unsigned)st.cfg.run_weak_no_score_ms);
        break;
      case 3:
        lab = "Alarm slabosti (ms)";
        snprintf(val, sizeof(val), "%u", (unsigned)st.cfg.run_weak_velky_ms);
        break;
      default:
        settingsDetailIdx = 0;
        lab = "?";
        snprintf(val, sizeof(val), "--");
        break;
    }
  } else {
    const GateRunProfile& gp = st.gateRun[settingsGateIdx];
    switch (settingsDetailIdx) {
      case 0:
        lab = "OK paprsek min %";
        snprintf(val, sizeof(val), "%u", (unsigned)gp.run_ok_min_pct);
        break;
      case 1:
        lab = "Vstup slabost %";
        snprintf(val, sizeof(val), "%u", (unsigned)gp.run_weak_enter_pct);
        break;
      case 2:
        lab = "Ignor slabosti do (ms)";
        snprintf(val, sizeof(val), "%u", (unsigned)gp.run_weak_no_score_ms);
        break;
      case 3:
        lab = "Alarm slabosti (ms)";
        snprintf(val, sizeof(val), "%u", (unsigned)gp.run_weak_velky_ms);
        break;
      default:
        settingsDetailIdx = 0;
        lab = "?";
        snprintf(val, sizeof(val), "--");
        break;
    }
  }

  const char* foot = settingsEditMode ? "K2- K3+  K4 OK" : "K2 K3 pol  K4 ladit";
  ui.showSettingsDetail(settingsDetailIdx, n, lab, val, foot, settingsEditMode);
}

static void diagModeUpdate(uint32_t nowMs) {
  buzzer.setMode(Buzzer::MODE_DIAG);

  GateCal cal = st.cal[diagGate];
  const bool conn = gateIsHwConnected(diagGate);
  uint16_t raw = 0;
  uint8_t fillPct = 0;
  bool adaptiveBar = false;

  if (conn) {
    raw = gates[diagGate].readRaw(nowMs);
    adaptiveBar = gates[diagGate].isCalibrated();
    fillPct = diagBarFillPct(raw, cal.zeroRaw, cal.maxRaw, adaptiveBar);
    buzzer.setDiagQualityPct(fillPct);
  } else {
    buzzer.setDiagQualityPct(100);
  }

  const bool showStatus = (lastDiagSaveMs != 0) && ((nowMs - lastDiagSaveMs) < 1500);
  const char* status = showStatus ? diagStatusText : nullptr;
  ui.showDiag(diagGate, conn, raw, conn && gates[diagGate].isCalibrated(),
              cal.zeroRaw, cal.maxRaw, fillPct, conn && adaptiveBar, status, (int8_t)diagCursor);
}

static void handleMenuActions(const ButtonEdges& e, uint32_t nowMs) {
  if (e.k1Pressed) {
    if (page == PAGE_SETTINGS && settingsDetailActive) {
      if (settingsEditMode) {
        settingsEditMode = false;
      } else {
        settingsDetailActive = false;
        settingsEditMode = false;
      }
    } else if (page == PAGE_SETTINGS && !settingsDetailActive && settingsQuickReturn) {
      page = PAGE_RUN;
      settingsQuickReturn = false;
      settingsDetailActive = false;
      settingsEditMode = false;
    } else {
      if (page == PAGE_SETTINGS) {
        settingsDetailActive = false;
        settingsEditMode = false;
      }
      page = (MenuPage)(((uint8_t)page + 1u) % (uint8_t)PAGE_COUNT);
      if (page == PAGE_SETTINGS) {
        settingsGlobal = true;
        settingsQuickReturn = false;
      }
    }
  }

  if (page == PAGE_RUN) {
    rebuildRunMenuGateList();
    const uint8_t total = runMenuTotalItems();
    if (e.k2Pressed && total > 0) {
      runMenuSel = (uint8_t)((runMenuSel + total - 1u) % total);
    }
    if (e.k3Pressed && total > 0) {
      runMenuSel = (uint8_t)((runMenuSel + 1u) % total);
    }
    clampRunMenuSelAndScroll();
    if (e.k4Pressed) {
      if (runMenuSel >= 3 && runMenuVisibleGateCount > 0) {
        const uint8_t gi = runMenuVisibleGateIdx[runMenuSel - 3];
        openGateSettings(gi);
      } else if (runMenuSel != 2) {
        if (runMenuSel == 0 && !sessionRunning) sessionStart(nowMs);
        if (runMenuSel == 1 && sessionRunning) sessionStop(nowMs);
      }
    }
  } else if (page == PAGE_DIAG) {
    if (e.k2Pressed) {
      if (diagCursor == 0) diagCursor = (uint8_t)(DIAG_ITEM_COUNT - 1);
      else diagCursor--;
    }
    if (e.k3Pressed) {
      diagCursor = (uint8_t)((diagCursor + 1u) % (uint8_t)DIAG_ITEM_COUNT);
    }
    if (e.k4Pressed) {
      GateCal cal = st.cal[diagGate];
      const bool conn = gateIsHwConnected(diagGate);
      const uint16_t raw = conn ? gates[diagGate].readRaw(nowMs) : 0;

      if (diagCursor == DIAG_NEXT_GATE) {
        diagGate = (uint8_t)((diagGate + 1u) % GATE_COUNT);
      } else if (diagCursor == DIAG_SAVE_ZERO) {
        if (!conn) {
          snprintf(diagStatusText, sizeof(diagStatusText), "NEZAPOJENO");
          lastDiagSaveMs = nowMs;
        } else {
          cal.zeroRaw = raw;
          snprintf(diagStatusText, sizeof(diagStatusText), "ULOZENO ZERO");
          gates[diagGate].setCal(cal.zeroRaw, cal.maxRaw);
          st.cal[diagGate] = cal;
          storage.save(st);
          lastDiagSaveMs = nowMs;
        }
      } else if (diagCursor == DIAG_SAVE_MAX) {
        if (!conn) {
          snprintf(diagStatusText, sizeof(diagStatusText), "NEZAPOJENO");
          lastDiagSaveMs = nowMs;
        } else {
          cal.maxRaw = raw;
          snprintf(diagStatusText, sizeof(diagStatusText), "ULOZENO MAX");
          gates[diagGate].setCal(cal.zeroRaw, cal.maxRaw);
          st.cal[diagGate] = cal;
          storage.save(st);
          lastDiagSaveMs = nowMs;
        }
      } else if (diagCursor == DIAG_RESET_STATS) {
        cal.zeroRaw = 0;
        cal.maxRaw = 0;
        gates[diagGate].setCal(0, 0);
        st.cal[diagGate] = cal;
        storage.save(st);
        snprintf(diagStatusText, sizeof(diagStatusText), "Z/M VYMAZANO");
        lastDiagSaveMs = nowMs;
      }
    }
  } else if (page == PAGE_SETTINGS) {
    if (!settingsDetailActive) {
      if (e.k4Pressed) {
        settingsDetailActive = true;
        settingsEditMode = false;
        settingsDetailIdx = 0;
      }
    } else if (!settingsEditMode) {
      const uint8_t n = settingsActiveItemCount();
      if (e.k2Pressed) {
        settingsDetailIdx = (uint8_t)((settingsDetailIdx + n - 1u) % n);
      }
      if (e.k3Pressed) {
        settingsDetailIdx = (uint8_t)((settingsDetailIdx + 1u) % n);
      }
      if (e.k4Pressed) {
        settingsEditMode = true;
      }
    } else {
      if (e.k2Pressed) settingsAdjust(-1);
      if (e.k3Pressed) settingsAdjust(1);
      if (e.k4Pressed) {
        settingsEditMode = false;
      }
    }
  }
}

static void renderMenuOverlay(uint32_t nowMs) {
  if (page == PAGE_RUN) return;

  if (page == PAGE_SETTINGS) {
    buzzer.setMode(Buzzer::MODE_SILENT);
    buzzer.stopAll();
    settingsModeUpdate(nowMs);
    return;
  }

  if (page == PAGE_DIAG) {
    diagModeUpdate(nowMs);
    return;
  }
}

// ---------------------- Arduino entry points ----------------------
void setup() {
  delay(50);
  storage.begin();

  memset(&st, 0, sizeof(st));
  st.magic = STORAGE_MAGIC;
  st.version = STORAGE_VERSION;

  StoredState loaded{};
  if (storage.load(loaded)) {
    st = loaded;
  } else {
    st.cfg = AppConfig();
    for (uint8_t gi = 0; gi < GATE_COUNT; gi++) {
      st.gateRun[gi] = GateRunProfile();
    }
  }

  // SANITY clamps: prevents instant DIAG if EEPROM garbage says holdMs=0
  if (st.cfg.run_enterDiag_holdMs < 300) st.cfg.run_enterDiag_holdMs = 1500;
  if (st.cfg.run_enterDiag_holdMs > 10000) st.cfg.run_enterDiag_holdMs = 10000;
  if (st.cfg.run_reset_clicks < 3) st.cfg.run_reset_clicks = 10;
  if (st.cfg.run_reset_windowMs < 1000) st.cfg.run_reset_windowMs = 5000;
  clampRunSettings(st.cfg);
  for (uint8_t gi = 0; gi < GATE_COUNT; gi++) {
    clampGateRunProfile(st.gateRun[gi]);
  }

  t1.begin(T1_PIN);
  t2.begin(T2_PIN);
  t3.begin(T3_PIN);
  t4.begin(T4_PIN);

  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    gates[i].begin(GATE_ADC_PINS[i], &st.cfg);
    gates[i].setCal(st.cal[i].zeroRaw, st.cal[i].maxRaw);
  }

  buzzer.begin(BUZZ_A_PIN, BUZZ_B_PIN);
  buzzer.setMode(Buzzer::MODE_SILENT);
  ui.begin();

  page = PAGE_RUN;
}

void loop() {
  const uint32_t nowMs = millis();

  t1.update(nowMs);
  t2.update(nowMs);
  t3.update(nowMs);
  t4.update(nowMs);

  rebuildRunMenuGateList();
  clampRunMenuSelAndScroll();

  const ButtonEdges edges = readButtonEdges();
  handleMenuActions(edges, nowMs);
  updateRunResetHold(nowMs);

  // Keep RUN logic active all the time. Non-RUN pages render as an overlay.
  runModeUpdate(nowMs, page == PAGE_RUN, page == PAGE_RUN);
  renderMenuOverlay(nowMs);

  buzzer.update();
}
