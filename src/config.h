#pragma once
#include <Arduino.h>

// ---------------------- HW: BluePill mapping ----------------------
// OLED I2C: SCL PB6, SDA PB7
// T1 PB10 (INPUT_PULLUP active LOW)
// T2 PB11 (INPUT_PULLUP active LOW)
// Buzzer differential: PB8 / PB9
// Gates ADC: PA0..PA7 (G1..G8)

#define OLED_SCL_PIN PB6
#define OLED_SDA_PIN PB7

#define T1_PIN PB10
#define T2_PIN PB11

#define BUZZ_A_PIN PB8
#define BUZZ_B_PIN PB9

static constexpr uint8_t GATE_COUNT = 8;
static constexpr uint8_t GATE_ADC_PINS[GATE_COUNT] = {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};

// ---------------------- UI OLED library choice ----------------------
// Pick ONE:
//#define UI_USE_U8G2
#define UI_USE_ADAFRUIT

// Typical SSD1306 I2C address
static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// ---------------------- MASTER-fixed behavior ----------------------
// FAILSAFE after DIAG exit
static constexpr uint32_t FAILSAFE_AFTER_DIAG_MS = 5000; // MASTER :contentReference[oaicite:4]{index=4}

// RUN alarm escalation times (MASTER)
static constexpr uint32_t RUN_L2_TIME_MS = 3000;   // L2 >= 3s :contentReference[oaicite:5]{index=5}
static constexpr uint32_t RUN_L3_TIME_MS = 10000;  // L3 >= 10s :contentReference[oaicite:6]{index=6}

// DIAG BAR zones (MASTER)
static constexpr uint8_t DIAG_BAR_NO_ACTION_PCT = 50; // 0..50% none :contentReference[oaicite:7]{index=7}
static constexpr uint8_t DIAG_BAR_EXIT_PCT      = 100; // >=100% exit :contentReference[oaicite:8]{index=8}

static constexpr uint32_t DIAG_BAR_FULL_MS = 2000; // how long to reach 100% bar (tunable)

// ---------------------- Configurable behavior (per Config pro lidi) ----------------------
struct AppConfig {
  // 1.1 calibration validity
  uint16_t cal_minSpanRaw = 120;   // tunable :contentReference[oaicite:9]{index=9}

  // 1.2 break detection (RUN only ARM)
  uint8_t break_onPct = 20;        // tunable :contentReference[oaicite:10]{index=10}
  uint8_t break_hystPct = 5;       // tunable :contentReference[oaicite:11]{index=11}

  // 1.4 DIAG tone quality
  uint8_t  diag_toneFullAtPct = 85;     // >= this -> continuous tone :contentReference[oaicite:12]{index=12}
  uint16_t diag_beepMinPeriodMs = 800;  // worst signal :contentReference[oaicite:13]{index=13}
  uint16_t diag_beepMaxPeriodMs = 120;  // near full tone :contentReference[oaicite:14]{index=14}

  // 1.7 filter
  enum FilterType : uint8_t { NONE=0, EMA=1 } filter_type = NONE;
  uint16_t filter_samplePeriodMs = 10;
  float    filter_emaAlpha = 0.25f; // 0..1 (higher=faster)

  // 1.8 UI
  uint8_t ui_active_maxShown = 4;

  // RUN gestures (not specified by MASTER; lives in UX/implementation area)
  uint8_t  run_reset_clicks = 10;
  uint16_t run_reset_windowMs = 5000;
  uint16_t run_enterDiag_holdMs = 2000; // hold T2 to enter DIAG (tunable)
};

// ---------------------- Storage layout ----------------------
static constexpr uint32_t STORAGE_MAGIC = 0x1AB01234; // just an identifier (not from spec)
static constexpr uint16_t STORAGE_VERSION = 1;

// ---------------------- Sound tuning (implementation) ----------------------
// Frequencies are implementation details; MASTER defines states/priority, not exact tones.
static constexpr uint16_t BUZ_FREQ_L1 = 440;
static constexpr uint16_t BUZ_FREQ_L2 = 660;
static constexpr uint16_t BUZ_FREQ_L3_A = 880;
static constexpr uint16_t BUZ_FREQ_L3_B = 660;
static constexpr uint16_t BUZ_FREQ_NEW = 1200;
