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

// ---------------------- UI OLED library choice ----------------------
// Pick ONE:
//#define UI_USE_U8G2
#define UI_USE_ADAFRUIT

static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// ---------------------- MASTER-fixed behavior ----------------------
static constexpr uint32_t FAILSAFE_AFTER_DIAG_MS = 5000;

static constexpr uint32_t RUN_L2_TIME_MS = 3000;   // L2 >= 3s
static constexpr uint32_t RUN_L3_TIME_MS = 10000;  // L3 >= 10s

static constexpr uint8_t DIAG_BAR_NO_ACTION_PCT = 50;  // 0..50% none
static constexpr uint8_t DIAG_BAR_EXIT_PCT      = 100; // >=100% exit

static constexpr uint32_t DIAG_BAR_FULL_MS = 2000;

// ---------------------- Configurable behavior ----------------------
struct AppConfig {
  uint16_t cal_minSpanRaw = 120;

  uint8_t break_onPct   = 20;
  uint8_t break_hystPct = 5;

  uint8_t  diag_toneFullAtPct   = 85;
  uint16_t diag_beepMinPeriodMs = 800;
  uint16_t diag_beepMaxPeriodMs = 120;

  enum FilterType : uint8_t { NONE=0, EMA=1 } filter_type = NONE;
  uint16_t filter_samplePeriodMs = 10;
  float    filter_emaAlpha       = 0.25f;

  uint8_t ui_active_maxShown = 4;

  uint8_t  run_reset_clicks     = 10;
  uint16_t run_reset_windowMs   = 5000;
  uint16_t run_enterDiag_holdMs = 2000;
};

// ---------------------- Storage layout ----------------------
static constexpr uint32_t STORAGE_MAGIC   = 0x1AB01234;
static constexpr uint16_t STORAGE_VERSION = 1;

// ---------------------- Sound tuning ----------------------
// Pasivní piezo typicky hraje nejlíp kolem 2–4 kHz.
static constexpr uint16_t BUZ_FREQ_L1   = 2400;
static constexpr uint16_t BUZ_FREQ_L2   = 3200;
static constexpr uint16_t BUZ_FREQ_L3_A = 2600;
static constexpr uint16_t BUZ_FREQ_L3_B = 3800;
static constexpr uint16_t BUZ_FREQ_NEW  = 4200;
