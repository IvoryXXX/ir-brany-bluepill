#include "UiOled.h"

#if defined(UI_USE_ADAFRUIT)
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>

  static Adafruit_SSD1306 display(128, 64, &Wire, -1);
#endif

void UiOled::begin() {
#if defined(UI_USE_ADAFRUIT)
  Wire.setSCL(OLED_SCL_PIN);
  Wire.setSDA(OLED_SDA_PIN);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
#endif
}

static void formatTime(char* out, size_t outSz, uint32_t ms) {
  uint32_t s = ms / 1000u;
  uint32_t m = s / 60u;
  uint32_t h = m / 60u;
  s %= 60u;
  m %= 60u;
  snprintf(out, outSz, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

void UiOled::showRun(bool armed, bool failsafe, const char* activeText, uint32_t sumEvents, uint32_t sumTimeMs) {
#if defined(UI_USE_ADAFRUIT)
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("MODE: RUN");

  display.setCursor(0, 12);
  display.print("ARM: ");
  display.print(armed ? "YES" : "NO ");
  display.print("  ");
  display.print(activeText);

  display.setCursor(0, 24);
  display.print("EVENTS: ");
  display.print((unsigned long)sumEvents);

  char tb[16];
  formatTime(tb, sizeof(tb), sumTimeMs);
  display.setCursor(0, 36);
  display.print("TIME: ");
  display.print(tb);

  if (failsafe) {
    display.setCursor(0, 52);
    display.print("FAILSAFE ACTIVE");
  }

  display.display();
#endif
}

void UiOled::showDiag(uint8_t gateIndex, uint16_t raw, bool calOk, uint16_t zeroRaw, uint16_t maxRaw,
                      uint8_t barPct, const char* phaseText) {
#if defined(UI_USE_ADAFRUIT)
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("MODE: DIAG  G");
  display.print((unsigned)gateIndex + 1);

  display.setCursor(0, 12);
  display.print("RAW: ");
  display.print((unsigned)raw);

  display.setCursor(0, 24);
  display.print("ZERO: ");
  display.print((unsigned)zeroRaw);

  display.setCursor(0, 36);
  display.print("MAX : ");
  display.print((unsigned)maxRaw);

  display.setCursor(0, 48);
  display.print(calOk ? "CAL: OK " : "CAL: -- ");
  display.print(" ");
  display.print(phaseText);

  // Simple bar at bottom (0..100)
  uint8_t w = (barPct > 100) ? 100 : barPct;
  display.drawRect(0, 62, 128, 2, SSD1306_WHITE);
  display.fillRect(0, 62, (uint8_t)(128u * w / 100u), 2, SSD1306_WHITE);

  display.display();
#endif
}
