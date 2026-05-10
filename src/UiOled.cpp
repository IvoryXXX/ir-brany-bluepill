#include "UiOled.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 display(128, 64, &Wire, -1);

static void formatTime(char* out, size_t outSz, uint32_t ms) {
  const uint32_t msRem = ms % 1000u;
  uint32_t totSec = ms / 1000u;
  const uint32_t s = totSec % 60u;
  const uint32_t m = (totSec / 60u) % 60u;
  const uint32_t h = totSec / 3600u;
  snprintf(out, outSz, "%02lu:%02lu:%02lu.%03lu", (unsigned long)h, (unsigned long)m, (unsigned long)s,
           (unsigned long)msRem);
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

void UiOled::showRun(bool sessionRunning, uint32_t sessionMs,
                     const uint32_t events2s[GATE_COUNT], const uint32_t events3s[GATE_COUNT],
                     uint8_t menuSel, uint8_t firstVisible, uint8_t menuTotalItems,
                     const uint8_t visibleGateIdx[GATE_COUNT], uint8_t visibleGateCount,
                     uint8_t resetHoldPct, const uint8_t gateBarPct[GATE_COUNT],
                     const uint8_t gateRunPhase[GATE_COUNT], uint32_t uiNowMs) {
  if (!_ok) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  char tb[20];
  formatTime(tb, sizeof(tb), sessionMs);

  display.setCursor(0, 0);
  display.print("CAS ");
  display.print(tb);
  display.print(sessionRunning ? " BEZI" : " STOP");

  (void)visibleGateCount;

  if (firstVisible > 0) {
    display.setCursor(118, 8);
    display.print("^");
  }
  if (firstVisible + RUN_MENU_LINES < menuTotalItems) {
    display.setCursor(118, 56);
    display.print("v");
  }

  for (uint8_t r = 0; r < RUN_MENU_LINES; r++) {
    const uint8_t mi = (uint8_t)(firstVisible + r);
    if (mi >= menuTotalItems) break;
    const int16_t y = (int16_t)(9 + r * 8);

    display.setCursor(0, y);
    display.print(menuSel == mi ? ">" : " ");

    if (mi == 0) {
      if (sessionRunning) display.print("-");
      display.print("START");
      if (sessionRunning) display.print("-");
    } else if (mi == 1) {
      if (!sessionRunning) display.print("-");
      display.print("STOP");
      if (!sessionRunning) display.print("-");
    } else if (mi == 2) {
      display.print("RESET OK");
      if (menuSel == 2 && resetHoldPct > 0) {
        display.print(" ");
        display.print(resetHoldPct);
        display.print("%");
      }
    } else {
      const uint8_t gi = visibleGateIdx[mi - 3];
      char buf[22];
      snprintf(buf, sizeof(buf), "G%u P%lu A%lu", (unsigned)gi + 1u,
               (unsigned long)events2s[gi], (unsigned long)events3s[gi]);
      display.print(buf);

      const uint8_t ph = gateRunPhase[gi];
      const int16_t dotCy = (int16_t)(y + 3);
      constexpr int16_t dotR = 2;
      constexpr int16_t dot1Cx = 73;
      constexpr int16_t dot2Cx = 81;
      if (ph == 2) {
        const bool on = ((uiNowMs / 200u) % 2u) == 0u;
        if (on)
          display.fillCircle(dot1Cx, dotCy, dotR, SSD1306_WHITE);
        else
          display.drawCircle(dot1Cx, dotCy, dotR, SSD1306_WHITE);
      }
      if (ph == 3) {
        const bool on = ((uiNowMs / 160u) % 2u) == 0u;
        if (on)
          display.fillCircle(dot2Cx, dotCy, dotR, SSD1306_WHITE);
        else
          display.drawCircle(dot2Cx, dotCy, dotR, SSD1306_WHITE);
      }

      const int16_t barX = 89;
      const int16_t barY = (int16_t)(y + 2);
      const int16_t barW = 36;
      const int16_t barH = 4;
      uint8_t fp = gateBarPct[gi];
      if (fp > 100) fp = 100;
      display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
      if (fp > 0) {
        int32_t fillW = ((int32_t)(barW - 2) * (int32_t)fp) / 100;
        if (fillW < 1) fillW = 1;
        if (fillW > barW - 2) fillW = barW - 2;
        display.fillRect(barX + 1, barY + 1, (int16_t)fillW, barH - 2, SSD1306_WHITE);
      }
    }
  }

  if (visibleGateCount == 0) {
    display.setCursor(0, 56);
    display.print("Brany: DIAG kalibrace");
  }

  display.display();
}

void UiOled::showSettingsSplash(bool perGate, uint8_t gateIdx) {
  if (!_ok) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("NASTAVENI");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 16);
  if (perGate) {
    display.print("Brana G");
    display.print((unsigned)gateIdx + 1u);
    display.print(" vlastni");
  } else {
    display.print("Globalni prahy RUN");
  }
  display.setCursor(0, 28);
  display.print("K4 = menu polozek");
  display.setCursor(0, 40);
  display.print("K2/K3 vyber K4 ladit");
  display.setCursor(0, 52);
  display.print("K1 zpet / strana");
  display.display();
}

void UiOled::showSettingsDetail(uint8_t index, uint8_t total, const char* label,
                                const char* valueStr, const char* footerLine, bool editing) {
  if (!_ok) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char head[12];
  snprintf(head, sizeof(head), "%u/%u", (unsigned)(index + 1u), (unsigned)total);
  display.setCursor(0, 0);
  display.print(head);
  if (editing) {
    display.setCursor(28, 0);
    display.print("UPRAVA");
  }
  display.setCursor(104, 0);
  display.print("K1");

  display.setCursor(0, 12);
  if (label) display.print(label);

  display.setTextSize(2);
  display.setCursor(0, 28);
  if (valueStr) display.print(valueStr);

  display.setTextSize(1);
  display.setCursor(0, 52);
  if (footerLine) display.print(footerLine);
  display.display();
}



void UiOled::showDiag(uint8_t gateIndex, bool hwConnected, uint16_t raw, bool calOk,
                      uint16_t zeroRaw, uint16_t maxRaw,
                      uint8_t barFillPct, bool barUsesSavedEndpoints,
                      const char* phaseText, int8_t selectedLine) {
  if (!_ok) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  const bool hasPhase = phaseText && phaseText[0];
  const uint8_t bodyTop = hasPhase ? 10 : 2;
  if (hasPhase) {
    display.setCursor(0, 0);
    display.print(phaseText);
  }

  const uint8_t rowStep = hasPhase ? 8u : 10u;
  const uint8_t firstRow = (uint8_t)(bodyTop + 2u);
  uint8_t ay[4];
  for (uint8_t i = 0; i < 4; i++) ay[i] = (uint8_t)(firstRow + i * rowStep);

  const uint8_t lastBaseline = (uint8_t)(firstRow + 3u * rowStep);
  const uint8_t textBottom = (uint8_t)(lastBaseline + 8u);
  constexpr uint8_t kBarH = 6;
  uint8_t barY = (uint8_t)(textBottom + 2u);
  if ((uint16_t)barY + (uint16_t)kBarH > 64u) barY = (uint8_t)(64u - kBarH);

  display.drawFastVLine(64, (int16_t)bodyTop, (int16_t)(barY - bodyTop), SSD1306_WHITE);
  display.drawFastHLine(0, (int16_t)(bodyTop - 2), 128, SSD1306_WHITE);

  char resetLabel[14];
  snprintf(resetLabel, sizeof(resetLabel), "RESET G%u", (unsigned)gateIndex + 1u);
  const char* actions[4] = {"NEXT", "SAVE ZERO", "SAVE MAX", resetLabel};
  for (uint8_t i = 0; i < 4; i++) {
    display.setCursor(0, ay[i]);
    display.print(((int8_t)i == selectedLine) ? ">" : " ");
    display.print(actions[i]);
  }

  display.setCursor(68, (int16_t)ay[0]);
  display.print("RAW:");
  if (hwConnected) display.print((unsigned)raw);
  else display.print("---");

  display.setCursor(68, (int16_t)ay[1]);
  display.print("Z:");
  if (hwConnected) display.print((unsigned)zeroRaw);
  else display.print("--");

  display.setCursor(68, (int16_t)ay[2]);
  display.print("M:");
  if (hwConnected) display.print((unsigned)maxRaw);
  else display.print("--");

  display.setCursor(68, (int16_t)ay[3]);
  display.print("F:");
  if (hwConnected) {
    display.print((unsigned)barFillPct);
    display.print("% ");
    display.print(barUsesSavedEndpoints ? "CAL" : "FIX");
    display.print(calOk ? "" : "!");
  } else {
    display.print("-- NC");
  }

  const int bx = 0;
  const int by = (int)barY;
  const int bw = 128;
  const int bh = (int)kBarH;
  display.drawRect(bx, by, bw, bh, SSD1306_WHITE);

  uint8_t pct = hwConnected ? barFillPct : 0;
  if (pct > 100) pct = 100;
  if (pct > 0) {
    int fillW = (int)((bw - 2) * (uint16_t)pct / 100u);
    if (fillW < 0) fillW = 0;
    if (fillW > (bw - 2)) fillW = (bw - 2);
    display.fillRect(bx + 1, by + 1, fillW, bh - 2, SSD1306_WHITE);
  }

  display.display();
}

void UiOled::showMenuPage(const char* title,
                          const char* line1,
                          const char* line2,
                          const char* line3,
                          const char* line4,
                          int8_t selectedLine) {
  if (!_ok) return;

  const char* lines[4] = {line1, line2, line3, line4};
  const uint8_t y[4] = {16, 28, 40, 52};

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(title ? title : "MENU");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (uint8_t i = 0; i < 4; i++) {
    if (!lines[i]) continue;
    display.setCursor(0, y[i]);
    if ((int8_t)i == selectedLine) display.print(">");
    else display.print(" ");
    display.print(lines[i]);
  }

  display.display();
}
