#include "Storage.h"

bool Storage::begin() {
  EEPROM.begin();
  return true;
}

uint16_t Storage::crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}

bool Storage::load(StoredState& out) {
  StoredState s{};
  EEPROM.get(0, s);

  if (s.magic != STORAGE_MAGIC || s.version != STORAGE_VERSION) return false;

  const uint16_t oldCrc = s.crc;
  s.crc = 0;
  const uint16_t calc = crc16((const uint8_t*)&s, sizeof(StoredState));
  if (calc != oldCrc) return false;

  out = s;
  return true;
}

bool Storage::save(const StoredState& in) {
  StoredState s = in;
  s.magic = STORAGE_MAGIC;
  s.version = STORAGE_VERSION;
  s.crc = 0;
  s.crc = crc16((const uint8_t*)&s, sizeof(StoredState));

  EEPROM.put(0, s);
  // STM32 EEPROM emulation auto-flushes on write; no commit() here.
  return true;
}
