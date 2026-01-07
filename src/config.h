#pragma once
#include <Arduino.h>

// ---------------------- HW: BluePill mapping ----------------------
// OLED I2C: SCL PB6, SDA PB7
// T1 PB10 (INPUT_PULLUP active LOW)
// T2 PB11 (INPUT_PULLUP active LOW)
// Buzzer: PB8 / PB9 (doporučeno: piezo mezi PB8 a PB9 = differential)
// Gates ADC: PA0..PA7 (G1..G8)

#define OLED_SCL_PIN PB6
#define OLED_SDA_PIN PB7

#define T1_PIN PB10
#define T2_PIN PB11

#define BUZZ_A_PIN PB8
#define BUZZ_B_PIN PB9

// 1 = piezo BETWEEN PB8 and PB9 (differential push-pull, louder/cleaner)
// 0 = piezo BETWEEN PB8 and GND (single-ended)
#define BUZZ_DIFFERENTIAL 1

static constexpr uint8_t GATE_COUNT = 8;
static constexpr uint8_t GATE_ADC_PINS[GATE_COUNT] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};

// ---------------------- Gate enable mask ----------------------
// Bit i = 1 => Gate (i+1) is active.
// Use this to "hard-disable" not-yet-connected gates so they cannot create false activity.
// Default here: only G1 enabled.
static constexpr uint8_t GATE_ENABLED_MASK = 0x01;

static inline bool gateEnabled(uint8_t gateIndex0) {
  return (GATE_ENABLED_MASK & (1u << gateIndex0)) != 0;
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

// RUN alarm escalation times (MASTER)
static constexpr uint32_t RUN_L2_TIME_MS = 3000;   // L2 >= 3s
static constexpr uint32_t RUN_L3_TIME_MS = 10000;  // L3 >= 10s

// DIAG BAR zones (MASTER)
static constexpr uint8_t DIAG_BAR_NO_ACTION_PCT = 50;  // 0..50% none
static constexpr uint8_t DIAG_BAR_EXIT_PCT      = 100; // >=100% exit

static constexpr uint32_t DIAG_BAR_FULL_MS = 2000; // how long to reach 100% bar (tunable)

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
};

// ---------------------- Storage layout ----------------------
static constexpr uint32_t STORAGE_MAGIC = 0x1AB01234;
static constexpr uint16_t STORAGE_VERSION = 1;

// ---------------------- Sound tuning (implementation) ----------------------
// Pasivní piezo typicky hraje nejlíp kolem 2–4 kHz.
static constexpr uint16_t BUZ_FREQ_L1   = 2400;
static constexpr uint16_t BUZ_FREQ_L2   = 3200;
static constexpr uint16_t BUZ_FREQ_L3_A = 2600;
static constexpr uint16_t BUZ_FREQ_L3_B = 3800;
static constexpr uint16_t BUZ_FREQ_NEW  = 4200;
