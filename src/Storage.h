#pragma once
#include <Arduino.h>
#include "config.h"
#include "Gate.h"

struct StoredState {
  uint32_t magic = 0;
  uint16_t version = 0;

  AppConfig cfg{};

  GateCal cal[GATE_COUNT]{};

  uint32_t events[GATE_COUNT]{};
  uint32_t timeMs[GATE_COUNT]{};
};

class Storage {
public:
  void begin();
  bool load(StoredState& out);
  void save(const StoredState& s);
};
