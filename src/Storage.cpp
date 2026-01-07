#include "Storage.h"
#include <EEPROM.h>

void Storage::begin() {
  // STM32 EEPROM emulation: begin() has NO size parameter
  EEPROM.begin();
}

bool Storage::load(StoredState& out) {
  EEPROM.get(0, out);
  if (out.magic != STORAGE_MAGIC) return false;
  if (out.version != STORAGE_VERSION) return false;
  return true;
}

void Storage::save(const StoredState& s) {
  EEPROM.put(0, s);
  // STM32 EEPROM library has NO commit()
}
