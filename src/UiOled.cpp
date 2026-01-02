#include "UiOled.h"

#if defined(UI_USE_ADAFRUIT)
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  static Adafruit_SSD1306 display(128, 64, &Wire, -1);
#elif defined(UI_USE_U8G2)
  #include <U8g2lib.h>
  #include <Wire.h>
  static U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
#else
  #error "Pick UI_USE_ADAFRUIT or UI_USE_U8G2 in config.h"
#endif

bool UiOled::begin() {
  Wire.setSCL(OLED_SCL_PIN);
  Wire.setSDA(OLED_SDA_PIN);
  Wire.begin();

#if defined(UI_USE_ADAFRUIT)
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    _ok = false;
    return false;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
  _ok = true;
  return true;
#else
  display.begin();
  _ok = true;
  return true;
#endif
}

void UiOled::clear() {
  if (!_ok) return;
#if defined(UI_USE_ADAFRUIT)
  display.clearDisplay();
  display.display();
#else
  display.clearBuffer();
  display.sendBuffer();
#endif
}

void UiOled::showBoot() {
  if (!_ok) return;
#if defined(UI_USE_ADAFRUIT)
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("IR BRANY");
  display.println("boot...");
  display.display();
#else
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0,12,"IR BRANY");
  display.drawStr(0,28,"boot...");
  display.sendBuffer();
#endif
}

void UiOled::showRun(bool armed, bool failsafe, const char* activeText, uint32_t totalEvents, uint32_t totalTimeMs) {
  if (!_ok) return;

  char line1[22];
  if (failsafe) snprintf(line1, sizeof(line1), "RUN: FAILSAFE");
  else snprintf(line1, sizeof(line1), "RUN: %s", armed ? "ARM" : "DISARM");

  const uint32_t sec = totalTimeMs / 1000u;

#if defined(UI_USE_ADAFRUIT)
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(line1);
  display.print("ACTIVE: ");
  display.println(activeText);
  display.print("EVENTS: ");
  display.println(totalEvents);
  display.print("TIME(s): ");
  display.println(sec);
  display.display();
#else
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0,12,line1);

  char l2[32]; snprintf(l2,sizeof(l2),"ACTIVE: %s", activeText);
  char l3[32]; snprintf(l3,sizeof(l3),"EVENTS: %lu",(unsigned long)totalEvents);
  char l4[32]; snprintf(l4,sizeof(l4),"TIME(s): %lu",(unsigned long)sec);
  display.drawStr(0,28,l2);
  display.drawStr(0,40,l3);
  display.drawStr(0,52,l4);
  display.sendBuffer();
#endif
}

void UiOled::showDiag(uint8_t gateIndex, uint16_t raw, bool calibrated, uint16_t zeroRaw, uint16_t maxRaw, uint8_t barPct, const char* phaseText) {
  if (!_ok) return;

  char head[22];
  snprintf(head, sizeof(head), "DIAG G%u %s", (unsigned)(gateIndex+1), calibrated ? "OK" : "UNCAL");

  char l1[24]; snprintf(l1,sizeof(l1),"RAW:%u", (unsigned)raw);
  char l2[24]; snprintf(l2,sizeof(l2),"ZERO:%u", (unsigned)zeroRaw);
  char l3[24]; snprintf(l3,sizeof(l3),"MAX :%u", (unsigned)maxRaw);
  char l4[24]; snprintf(l4,sizeof(l4),"BAR :%u%% %s", (unsigned)barPct, phaseText);

#if defined(UI_USE_ADAFRUIT)
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(head);
  display.println(l1);
  display.println(l2);
  display.println(l3);
  display.println(l4);
  display.display();
#else
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0,12,head);
  display.drawStr(0,28,l1);
  display.drawStr(0,40,l2);
  display.drawStr(0,52,l3);
  display.drawStr(0,64,l4);
  display.sendBuffer();
#endif
}
