#pragma once
#include <Arduino.h>

// ---------------------- HW: BluePill mapping ----------------------
// OLED I2C: SCL PB6, SDA PB7
// T1 PB10 (INPUT_PULLUP active LOW)
// T2 PB11 (INPUT_PULLUP active LOW)
// T3 PB12 (INPUT_PULLUP active LOW)
// T4 PB13 (INPUT_PULLUP active LOW)
// Buzzer: PB8 / PB9 (doporučeno: piezo mezi PB8 a PB9 = differential)
// Gates ADC: PA0..PA7 (G1..G8)

#define OLED_SCL_PIN PB6
#define OLED_SDA_PIN PB7

#define T1_PIN PB10
#define T2_PIN PB11
#define T3_PIN PB12
#define T4_PIN PB13

#define BUZZ_A_PIN PB8
#define BUZZ_B_PIN PB9

// 1 = piezo BETWEEN PB8 and PB9 (differential push-pull, louder/cleaner)
// 0 = piezo BETWEEN PB8 and GND (single-ended, quieter)
#define BUZZ_DIFFERENTIAL 0

static constexpr uint8_t GATE_COUNT = 8;
static constexpr uint8_t GATE_ADC_PINS[GATE_COUNT] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};

// Počet skutečně zapojených vstupů na desce (index od 0). Jen G1/PA0 -> poslední platný index 0.
static constexpr uint8_t HW_GATE_LAST_CONNECTED = 0;

static inline bool gateIsHwConnected(uint8_t gateIndex) {
  return gateIndex <= HW_GATE_LAST_CONNECTED;
}

// 1 = invert ADC meaning (pokud máš chování brány přesně obráceně)
//     typicky: při svícení raw klesá a kód to bere jako BROKEN -> invert to.
#define GATE_RAW_INVERT 1

// ---------------------- UI OLED library choice ----------------------
// Pick ONE:
//#define UI_USE_U8G2
#define UI_USE_ADAFRUIT

// Typical SSD1306 I2C address
static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// ---------------------- MASTER-fixed behavior ----------------------
// FAILSAFE after DIAG exit
static constexpr uint32_t FAILSAFE_AFTER_DIAG_MS = 5000;

// RUN menu: podržet OK (K4) na položce RESET pro vynulování měření
static constexpr uint32_t RUN_RESET_HOLD_MS = 2000;

// RUN: výchozí hodnoty (lze měnit v menu NASTAVENÍ, ukládá se v AppConfig / EEPROM)
static constexpr uint8_t RUN_SIGNAL_OK_MIN_PCT_DEFAULT = 90;
static constexpr uint8_t RUN_SIGNAL_WEAK_ENTER_PCT_DEFAULT = 84;
static constexpr uint16_t RUN_WEAK_NO_SCORE_MS_DEFAULT = 500;
static constexpr uint16_t RUN_WEAK_VELKY_MS_DEFAULT = 1010;

// DIAG BAR zones (MASTER)
static constexpr uint8_t DIAG_BAR_NO_ACTION_PCT = 50;  // 0..50% none
static constexpr uint8_t DIAG_BAR_EXIT_PCT      = 100; // >=100% exit

static constexpr uint32_t DIAG_BAR_FULL_MS = 2000; // how long to reach 100% bar (tunable)

// DIAG OLED bar: fixed scale before span is valid (empty at hiRaw, full at loRaw)
static constexpr uint16_t DIAG_BAR_FIXED_HI_RAW = 900;
static constexpr uint16_t DIAG_BAR_FIXED_LO_RAW = 15;

// ---------------------- Configurable behavior (per Config pro lidi) ----------------------
struct AppConfig {
  // 1.1 calibration validity
  uint16_t cal_minSpanRaw = 120;

  // 1.2 break detection (RUN only ARM)
  uint8_t break_onPct = 20;
  uint8_t break_hystPct = 5;

  // 1.4 DIAG tone quality
  uint8_t  diag_toneFullAtPct = 85;     // >= this -> continuous tone
  uint16_t diag_beepMinPeriodMs = 800;  // worst signal
  uint16_t diag_beepMaxPeriodMs = 120;  // near full tone

  // 1.7 filter
  enum FilterType : uint8_t { NONE=0, EMA=1 } filter_type = NONE;
  uint16_t filter_samplePeriodMs = 10;
  float    filter_emaAlpha = 0.25f; // 0..1 (higher=faster)

  // 1.8 UI
  uint8_t ui_active_maxShown = 4;

  // RUN gestures
  uint8_t  run_reset_clicks = 10;
  uint16_t run_reset_windowMs = 5000;
  uint16_t run_enterDiag_holdMs = 2000; // hold T2 to enter DIAG

  // RUN: prahy slabosti + ignor do přerušení + práh délky slabosti pro alarm (menu NASTAVENÍ)
  uint8_t  run_ok_min_pct = RUN_SIGNAL_OK_MIN_PCT_DEFAULT;
  uint8_t  run_weak_enter_pct = RUN_SIGNAL_WEAK_ENTER_PCT_DEFAULT;
  uint16_t run_weak_no_score_ms = RUN_WEAK_NO_SCORE_MS_DEFAULT;
  uint16_t run_weak_velky_ms = RUN_WEAK_VELKY_MS_DEFAULT;
};

// Spolecne limity pro globalni cfg i profil brany
static inline void clampRunThresholdFields(uint8_t& okPct, uint8_t& weakEnterPct, uint16_t& noScoreMs,
                                           uint16_t& velkyMs) {
  if (okPct < 75u) okPct = 75u;
  if (okPct > 98u) okPct = 98u;
  if (weakEnterPct < 50u) weakEnterPct = 50u;
  if (weakEnterPct > 89u) weakEnterPct = 89u;
  if (weakEnterPct >= okPct) {
    weakEnterPct = (uint8_t)(okPct > 2u ? okPct - 2u : 50u);
  }
  if (noScoreMs > 2000u) noScoreMs = 2000u;
  if (velkyMs < 300u) velkyMs = 300u;
  if (velkyMs > 5000u) velkyMs = 5000u;
  if (velkyMs <= noScoreMs) velkyMs = (uint16_t)(noScoreMs + 10u);
}

static inline void clampRunSettings(AppConfig& c) {
  clampRunThresholdFields(c.run_ok_min_pct, c.run_weak_enter_pct, c.run_weak_no_score_ms,
                          c.run_weak_velky_ms);
}

// Vlastni prahy jedne brany; pri use_global == true se ignoruji (bere se AppConfig)
struct GateRunProfile {
  bool use_global = true;
  uint8_t run_ok_min_pct = RUN_SIGNAL_OK_MIN_PCT_DEFAULT;
  uint8_t run_weak_enter_pct = RUN_SIGNAL_WEAK_ENTER_PCT_DEFAULT;
  uint16_t run_weak_no_score_ms = RUN_WEAK_NO_SCORE_MS_DEFAULT;
  uint16_t run_weak_velky_ms = RUN_WEAK_VELKY_MS_DEFAULT;
};

static inline void clampGateRunProfile(GateRunProfile& p) {
  if (!p.use_global) {
    clampRunThresholdFields(p.run_ok_min_pct, p.run_weak_enter_pct, p.run_weak_no_score_ms,
                            p.run_weak_velky_ms);
  }
}

// ---------------------- Storage layout ----------------------
static constexpr uint32_t STORAGE_MAGIC = 0x1AB01234;
static constexpr uint16_t STORAGE_VERSION = 4;

// ---------------------- Sound tuning (implementation) ----------------------
// Pasivní piezo typicky hraje nejlíp kolem 2–4 kHz.
static constexpr uint16_t BUZ_FREQ_L1   = 1800;
static constexpr uint16_t BUZ_FREQ_L2   = 2300;
static constexpr uint16_t BUZ_FREQ_L3_A = 1700;
static constexpr uint16_t BUZ_FREQ_L3_B = 2600;
static constexpr uint16_t BUZ_FREQ_NEW  = 2800;
