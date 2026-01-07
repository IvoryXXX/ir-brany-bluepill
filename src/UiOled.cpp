#include "UiOled.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- Minimalist 8x8 icons (1-bit) ---
// Encoding for Adafruit_GFX drawBitmap(): 1 byte per row, MSB = leftmost pixel.
static const uint8_t PROGMEM ICON_LOCK_CLOSED_8[8] = {
  0x3C, 0x42, 0x5A, 0x7E, 0x7E, 0x66, 0x66, 0x7E
};

static const uint8_t PROGMEM ICON_LOCK_OPEN_8[8] = {
  0x3C, 0x42, 0x58, 0x7C, 0x7C, 0x66, 0x66, 0x7E
};

static const uint8_t PROGMEM ICON_WARN_8[8] = {
  0x10, 0x38, 0x7C, 0xFE, 0xFE, 0x7C, 0x38, 0x10
};

static const uint8_t PROGMEM ICON_EVENT_8[8] = {
  0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0xDB, 0x7E, 0x3C
};

static const uint8_t PROGMEM ICON_CLOCK_8[8] = {
  0x3C, 0x42, 0x81, 0x89, 0x91, 0x81, 0x42, 0x3C
};

static void formatTime(char* out, size_t outSz, uint32_t ms) {
  uint32_t s = ms / 1000u;
  uint32_t m = s / 60u;
  uint32_t h = m / 60u;
  s %= 60u;
  m %= 60u;
  snprintf(out, outSz, "%02lu:%02lu:%02lu",
           (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

bool UiOled::begin() {
  Wire.setSCL(OLED_SCL_PIN);
  Wire.setSDA(OLED_SDA_PIN);
  Wire.begin();

  _ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!_ok) return false;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  return true;
}

void UiOled::clear() {
  if (!_ok) return;
  display.clearDisplay();
  display.display();
}

void UiOled::showBoot() {
  if (!_ok) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("IR BRANY");
  display.println("BOOT...");
  display.display();
}

void UiOled::showRun(bool armed, bool failsafe, const char* activeText,
                     uint32_t totalEvents, uint32_t totalTimeMs,
                     uint8_t holdPct, const char* holdText) {
  if (!_ok) return;

  // Layout per spec:
  // Row1 (right): status icon (FAILSAFE / ARM / DISARM)
  // Row2: events + activeText on right
  // Row3: time
  // Row4: holdText
  // Row5: hold bar (outline always)

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // --- Row1: status icon on the right ---
  const uint8_t iconX = 128 - 8;
  const uint8_t iconY = 0;
  if (failsafe) {
    display.drawBitmap(iconX, iconY, ICON_WARN_8, 8, 8, SSD1306_WHITE);
  } else if (armed) {
    display.drawBitmap(iconX, iconY, ICON_LOCK_CLOSED_8, 8, 8, SSD1306_WHITE);
  } else {
    display.drawBitmap(iconX, iconY, ICON_LOCK_OPEN_8, 8, 8, SSD1306_WHITE);
  }

  // --- Row2: Events (icon + number) and activeText (right) ---
  display.drawBitmap(0, 16, ICON_EVENT_8, 8, 8, SSD1306_WHITE);
  display.setCursor(10, 16);
  display.print((unsigned long)totalEvents);

  if (activeText && activeText[0]) {
    int16_t xa = 128 - (int16_t)strlen(activeText) * 6;
    if (xa < 0) xa = 0;
    display.setCursor(xa, 16);
    display.print(activeText);
  }

  // --- Row3: Time (icon + hh:mm:ss) ---
  char tb[16];
  formatTime(tb, sizeof(tb), totalTimeMs);
  display.drawBitmap(0, 32, ICON_CLOCK_8, 8, 8, SSD1306_WHITE);
  display.setCursor(10, 32);
  display.print(tb);

  // --- Row4: Hold text (only when holding) ---
  if (holdPct > 0 && holdText && holdText[0]) {
    display.setCursor(0, 48);
    display.print(holdText);
  }

  // --- Row5: Hold bar (always outline, fill only when holding) ---
  const int bx = 0, by = 56, bw = 128, bh = 8;
  display.drawRect(bx, by, bw, bh, SSD1306_WHITE);

  if (holdPct > 0) {
    if (holdPct > 100) holdPct = 100;
    int fillW = (int)((bw - 2) * (uint16_t)holdPct / 100u);
    if (fillW < 0) fillW = 0;
    if (fillW > (bw - 2)) fillW = (bw - 2);
    display.fillRect(bx + 1, by + 1, fillW, bh - 2, SSD1306_WHITE);
  }

  display.display();
}

void UiOled::showDiag(uint8_t gateIndex, uint16_t raw, bool calOk,
                      uint16_t zeroRaw, uint16_t maxRaw,
                      uint8_t barPct, const char* phaseText) {
  if (!_ok) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("DIAG G");
  display.print((unsigned)gateIndex + 1);

  display.setCursor(0, 12);
  display.print("RAW:");
  display.print((unsigned)raw);

  display.setCursor(0, 24);
  display.print("Z:");
  display.print((unsigned)zeroRaw);
  display.print(" M:");
  display.print((unsigned)maxRaw);

  display.setCursor(0, 36);
  display.print(calOk ? "CAL:OK " : "CAL:-- ");
  if (phaseText) display.print(phaseText);

  uint8_t w = (barPct > 100) ? 100 : barPct;
  display.drawRect(0, 62, 128, 2, SSD1306_WHITE);
  display.fillRect(0, 62, (uint8_t)(128u * w / 100u), 2, SSD1306_WHITE);

  display.display();
}
