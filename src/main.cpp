#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

// ---------- PIN DEFINITIONS (Standard ESP32 VSPI) ----------
#define TFT_CS          5    // Chip Select
#define TFT_DC          2    // Data / Command
#define TFT_RST         4    // Reset
#define TFT_MOSI       23    // VSPI MOSI (SDA)
#define TFT_SCLK       18    // VSPI SCK (SCL)
#define TFT_BL         32    // Backlight PWM pin (LED)

#define FP_RX          16    // ESP32 RX2 <- Fingerprint Sensor TX
#define FP_TX          17    // ESP32 TX2 -> Fingerprint Sensor RX

#define BUZZER_PIN     13    // Buzzer pin
#define BOOT_BUTTON     0    // On-board BOOT button for simulation

// ---------- COLOR PALETTE (RGB565) ----------
#define C_BG          0x0000 // Deep Black (#000000)
#define C_WHITE       0xFFFF // Crisp White (#FFFFFF)
#define C_GREEN       0x07E0 // Bright Emerald Green (#00FF66)
#define C_RED         0xF904 // Crimson Red (#E01A22)
#define C_CARD        0x10A2 // Slate Card (#12151A)
#define C_BORDER      0x2965 // Outline Gray (#282C34)
#define C_MUTED       0x7BEF // Subtitle Gray (#7A808C)
#define C_AMBER       0xFD20

const char* DEFAULT_WIFI_SSID = "Arigatoo1";
const char* DEFAULT_WIFI_PASS = "12345678";
bool wifiConnected = false;
char wifiIP[24] = "OFFLINE";
int clockHour = 9, clockMin = 41, clockSec = 0;
unsigned long lastSecTick = 0;
unsigned long lastFrameTime = 0; // Amber Orange (#FFAA00)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16* canvas = nullptr;
int W = 160;
int H = 128;
uint8_t currentRotation = 3;
bool needsImmediateRedraw = true;
Preferences prefs;

void renderBootStage(const char* title, const char* subtitle, int progress) {
  if (!canvas) return;
  canvas->fillScreen(C_BG);
  canvas->fillRoundRect(10, 20, W - 20, H - 40, 6, C_CARD);
  canvas->drawRoundRect(10, 20, W - 20, H - 40, 6, C_BORDER);
  drawCenteredText(title, W / 2, 38, 2, W - 32, C_WHITE);
  drawCenteredText(subtitle, W / 2, 60, 1, W - 32, C_MUTED);
  int barX = 24, barY = 82, barW = W - 48;
  canvas->drawRect(barX, barY, barW, 6, C_BORDER);
  int fillW = (barW - 2) * progress / 100;
  if (fillW > 0) canvas->fillRect(barX + 1, barY + 1, fillW, 4, C_GREEN);
  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), W, H);
}

void drawCenteredText(const char* text, int cx, int y, uint8_t maxTextSize, int maxWidth, uint16_t color) {
  if (!canvas) return;
  int16_t x1, y1;
  uint16_t w, h;
  for (int sz = maxTextSize; sz >= 1; sz--) {
    canvas->setTextSize(sz);
    canvas->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    if ((int)w <= maxWidth || sz == 1) {
      canvas->setTextColor(color);
      canvas->setCursor(cx - ((int)w / 2), y);
      canvas->print(text);
      return;
    }
  }
}

void applyRotation(uint8_t rot) {
  currentRotation = rot;
  tft.setRotation(currentRotation);
  W = tft.width();
  H = tft.height();
  if (canvas) { delete canvas; canvas = nullptr; }
  canvas = new GFXcanvas16(W, H);
  needsImmediateRedraw = true;
}

struct Employee {
  uint16_t id;
  char name[24];
  bool isCheckedIn;
};

#define MAX_EMPLOYEES 16
Employee employees[MAX_EMPLOYEES];
int employeeCount = 0;

void loadEmployeeDatabase() {
  prefs.begin("avian-db", false);
  employeeCount = prefs.getInt("count", 0);

  if (employeeCount <= 0) {
    employeeCount = 3;
    employees[0] = {1, "Joyboy", false};
    employees[1] = {2, "Alice", false};
    employees[2] = {3, "Pega", false};

    prefs.putInt("count", employeeCount);
    for (int i = 0; i < employeeCount; i++) {
      char keyId[32], keyName[32];
      snprintf(keyId, sizeof(keyId), "id_%d", i);
      snprintf(keyName, sizeof(keyName), "name_%d", i);
      prefs.putUShort(keyId, employees[i].id);
      prefs.putString(keyName, employees[i].name);
    }
  } else {
    for (int i = 0; i < employeeCount && i < MAX_EMPLOYEES; i++) {
      char keyId[32], keyName[32];
      snprintf(keyId, sizeof(keyId), "id_%d", i);
      snprintf(keyName, sizeof(keyName), "name_%d", i);
      employees[i].id = prefs.getUShort(keyId, i + 1);
      String n = prefs.getString(keyName, "Worker");
      strlcpy(employees[i].name, n.c_str(), sizeof(employees[i].name));
      employees[i].isCheckedIn = false;
    }
  }
}

void saveNewEmployee(uint16_t slot, const char* name) {
  int foundIdx = -1;
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == slot) { foundIdx = i; break; }
  }
  if (foundIdx >= 0) {
    strlcpy(employees[foundIdx].name, name, sizeof(employees[foundIdx].name));
    char keyName[32];
    snprintf(keyName, sizeof(keyName), "name_%d", foundIdx);
    prefs.putString(keyName, name);
  } else if (employeeCount < MAX_EMPLOYEES) {
    int idx = employeeCount;
    employees[idx].id = slot;
    strlcpy(employees[idx].name, name, sizeof(employees[idx].name));
    employees[idx].isCheckedIn = false;
    employeeCount++;

    prefs.putInt("count", employeeCount);
    char keyId[32], keyName[32];
    snprintf(keyId, sizeof(keyId), "id_%d", idx);
    snprintf(keyName, sizeof(keyName), "name_%d", idx);
    prefs.putUShort(keyId, slot);
    prefs.putString(keyName, name);
  }
}

Employee* getEmployeeBySlot(uint16_t slot) {
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == slot) return &employees[i];
  }
  return nullptr;
}

struct BuzzerTone {
  uint16_t duration;
  uint16_t pause;
};

#define MAX_TONES 4
BuzzerTone toneQueue[MAX_TONES];
int toneCount = 0;
int currentToneIdx = 0;
unsigned long toneStartTime = 0;
bool toneIsPlaying = false;
bool toneInPause = false;

void playAsyncTones(const BuzzerTone* tones, int count) {
  toneCount = min(count, MAX_TONES);
  for (int i = 0; i < toneCount; i++) toneQueue[i] = tones[i];
  currentToneIdx = 0;
  toneStartTime = millis();
  toneIsPlaying = true;
  toneInPause = false;
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
}

void beepAsync(uint16_t ms) {
  const BuzzerTone t[] = {{ms, 0}};
  playAsyncTones(t, 1);
}

void beepOKAsync() {
  const BuzzerTone t[] = {{60, 30}, {90, 0}};
  playAsyncTones(t, 2);
}

void beepErrorAsync() {
  const BuzzerTone t[] = {{150, 40}, {200, 0}};
  playAsyncTones(t, 2);
}

void updateBuzzer() {
  if (!toneIsPlaying) return;
  unsigned long elapsed = millis() - toneStartTime;
  BuzzerTone& t = toneQueue[currentToneIdx];
  if (!toneInPause) {
    if (elapsed >= t.duration) {
      digitalWrite(BUZZER_PIN, LOW);
      if (t.pause > 0) {
        toneInPause = true;
        toneStartTime = millis();
      } else {
        currentToneIdx++;
        if (currentToneIdx >= toneCount) toneIsPlaying = false;
        else { toneStartTime = millis(); digitalWrite(BUZZER_PIN, HIGH); }
      }
    }
  } else {
    if (elapsed >= t.pause) {
      toneInPause = false;
      currentToneIdx++;
      if (currentToneIdx >= toneCount) toneIsPlaying = false;
      else { toneStartTime = millis(); digitalWrite(BUZZER_PIN, HIGH); }
    }
  }
}

void setup() {
  SPI.setFrequency(27000000);
  tft.initR(INITR_BLACKTAB);
  applyRotation(currentRotation);
  tft.fillScreen(C_BG);
  for (int p = 0; p <= 100; p += 25) {
    renderBootStage("ARIGATOO", "ATTENDANCE TERMINAL", p);
    delay(20);
  }
  beepAsync(40);
  delay(140);
  for (int p = 0; p <= 100; p += 25) {
    renderBootStage("INFRANEX DEV", "FIRMWARE READY", p);
    delay(20);
  }
  beepAsync(50);
  delay(140);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  loadEmployeeDatabase();
}
void loop() { updateBuzzer(); }
