#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "Gate.h"

struct StoredState {
  uint32_t magic;
  uint16_t version;
  uint16_t crc;

  AppConfig cfg;

  GateCal cal[GATE_COUNT];

  // RUN: přerušení brány (P) / alarm ze slabosti (A)
  uint32_t events2s[GATE_COUNT];
  uint32_t events3s[GATE_COUNT];
  uint32_t timeMs[GATE_COUNT];

  GateRunProfile gateRun[GATE_COUNT];
};

class Storage {
public:
  bool begin();
  bool load(StoredState& out);
  bool save(const StoredState& s);

  static uint16_t crc16(const uint8_t* data, size_t len);

private:
  static constexpr int EEPROM_SIZE = sizeof(StoredState);
};
