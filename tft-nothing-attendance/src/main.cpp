#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

// ============================================================================
// ARIGATOO // INFRANEX DEV // MINIMAL ATTENDANCE SYSTEM
// ============================================================================
// Steady, distraction-free UI with ZERO flickering/blinking.
// Rock-solid display that updates only on clock second ticks or finger touch.
// ============================================================================

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
#define C_AMBER       0xFD20 // Amber Orange (#FFAA00)

// Optional Wi-Fi credentials
const char* DEFAULT_WIFI_SSID = "Arigatoo1";
const char* DEFAULT_WIFI_PASS = "12345678";

// Objects
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16* canvas = nullptr;
HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fpSerial);
Preferences prefs;

// Dimensions & Rotation (Default 3 = 180-deg rotated for desk mounting)
int W = 160;
int H = 128;
uint8_t currentRotation = 3;

// State Variables
bool sensorHardwareFound  = false;
bool wifiConnected        = false;
char wifiIP[24]           = "OFFLINE";
bool needsImmediateRedraw = true;

enum ScreenState {
  STATE_IDLE,
  STATE_MARKED,
  STATE_ERROR,
  STATE_ENROLL_STEP1,
  STATE_ENROLL_STEP2,
  STATE_ENROLL_SUCCESS
};

ScreenState currentScreen = STATE_IDLE;
unsigned long resultUntil = 0;

// Enrolled Employee Database
struct Employee {
  uint16_t id;
  char name[24];
  bool isCheckedIn;
};

#define MAX_EMPLOYEES 16
Employee employees[MAX_EMPLOYEES];
int employeeCount = 0;

// Current Punch Display Data
char lastPunchName[24] = "Joyboy";
uint16_t lastPunchSlot = 1;
char lastPunchTime[16] = "09:41 AM";
bool lastPunchIsIn     = true;

// Enrollment variables
char enrollTargetName[24] = "Worker";
uint16_t enrollTargetSlot = 1;

// Clock simulation
int clockHour = 9;
int clockMin  = 41;
int clockSec  = 0;
unsigned long lastSecTick = 0;

// Button debounce for BOOT pin (GPIO 0)
int lastBootButtonState = HIGH;
unsigned long bootButtonDownTime = 0;

// Serial command buffer
String serialInput = "";

// ----------------------------------------------------------------------------
// NON-BLOCKING ASYNC BUZZER ENGINE (ZERO FREEZES)
// ----------------------------------------------------------------------------
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
  for (int i = 0; i < toneCount; i++) {
    toneQueue[i] = tones[i];
  }
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
  const BuzzerTone t[] = {
    {60, 30},
    {90, 0}
  };
  playAsyncTones(t, 2);
}

void beepErrorAsync() {
  const BuzzerTone t[] = {
    {150, 40},
    {200, 0}
  };
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
        if (currentToneIdx >= toneCount) {
          toneIsPlaying = false;
        } else {
          toneStartTime = millis();
          digitalWrite(BUZZER_PIN, HIGH);
        }
      }
    }
  } else {
    if (elapsed >= t.pause) {
      toneInPause = false;
      currentToneIdx++;
      if (currentToneIdx >= toneCount) {
        toneIsPlaying = false;
      } else {
        toneStartTime = millis();
        digitalWrite(BUZZER_PIN, HIGH);
      }
    }
  }
}

// ----------------------------------------------------------------------------
// BLUE LED CONTROL (TURNS OFF ONBOARD ESP32 & SENSOR LED)
// ----------------------------------------------------------------------------
void turnOffBlueLED() {
  // 1. Turn off onboard ESP32 Blue LED (connected to GPIO 2 / TFT_DC)
  digitalWrite(TFT_DC, LOW);

  // 2. Turn off optical fingerprint sensor blue LED ring / backlight
  if (sensorHardwareFound) {
    finger.LEDcontrol(false);
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE, 0);
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE, 0);
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_RED, 0);
  }
}

// ----------------------------------------------------------------------------
// DYNAMIC ROTATION ALLOCATOR
// ----------------------------------------------------------------------------
void applyRotation(uint8_t rot) {
  currentRotation = rot;
  tft.setRotation(currentRotation);
  W = tft.width();
  H = tft.height();

  if (canvas) {
    delete canvas;
    canvas = nullptr;
  }
  canvas = new GFXcanvas16(W, H);
  needsImmediateRedraw = true;
  Serial.printf("[DISPLAY] Resolution: %dx%d (Rot: %d)\n", W, H, currentRotation);
}

// ----------------------------------------------------------------------------
// RESPONSIVE TEXT FORMATTING ENGINE
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// NVS DATABASE INITIALIZATION
// ----------------------------------------------------------------------------
void loadEmployeeDatabase() {
  prefs.begin("avian-db", false);
  currentRotation = prefs.getUChar("rot", 3);
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
    if (employees[i].id == slot) {
      foundIdx = i;
      break;
    }
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

// ----------------------------------------------------------------------------
// BOOT STAGES: ARIGATOO & INFRANEX DEV
// ----------------------------------------------------------------------------
void renderBootStage(const char* title, const char* subtitle, int progress) {
  if (!canvas) return;
  canvas->fillScreen(C_BG);

  // Minimal Hero Card
  canvas->fillRoundRect(10, 20, W - 20, H - 40, 6, C_CARD);
  canvas->drawRoundRect(10, 20, W - 20, H - 40, 6, C_BORDER);

  drawCenteredText(title, W / 2, 38, 2, W - 32, C_WHITE);
  drawCenteredText(subtitle, W / 2, 60, 1, W - 32, C_MUTED);

  // Progress Bar
  int barX = 24;
  int barY = 82;
  int barW = W - 48;
  canvas->drawRect(barX, barY, barW, 6, C_BORDER);
  int fillW = (barW - 2) * progress / 100;
  if (fillW > 0) {
    canvas->fillRect(barX + 1, barY + 1, fillW, 4, C_GREEN);
  }

  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), W, H);
}

void runBootSequence() {
  Serial.println("\n--- BOOTING ATTENDANCE SYSTEM ---");

  // Stage 1: Arigatoo
  for (int p = 0; p <= 100; p += 25) {
    renderBootStage("ARIGATOO", "ATTENDANCE TERMINAL", p);
    delay(20);
  }
  beepAsync(40);
  delay(140);

  // Stage 2: Infranex Dev
  for (int p = 0; p <= 100; p += 25) {
    renderBootStage("INFRANEX DEV", "FIRMWARE READY", p);
    delay(20);
  }
  beepAsync(50);
  delay(140);

  // Stage 3: WiFi Initialization
  canvas->fillScreen(C_BG);
  canvas->fillRoundRect(10, 20, W - 20, H - 40, 6, C_CARD);
  canvas->drawRoundRect(10, 20, W - 20, H - 40, 6, C_BORDER);

  drawCenteredText("CONNECTING", W / 2, 38, 2, W - 32, C_WHITE);
  drawCenteredText(DEFAULT_WIFI_SSID, W / 2, 60, 1, W - 32, C_MUTED);
  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), W, H);

  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

  unsigned long startWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWifi < 1200) {
    delay(30);
  }

  canvas->fillRoundRect(10, 20, W - 20, H - 40, 6, C_CARD);
  canvas->drawRoundRect(10, 20, W - 20, H - 40, 6, C_BORDER);

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    strlcpy(wifiIP, WiFi.localIP().toString().c_str(), sizeof(wifiIP));
    drawCenteredText("WIFI CONNECTED", W / 2, 45, 1, W - 32, C_GREEN);
    drawCenteredText(wifiIP, W / 2, 64, 1, W - 32, C_MUTED);
  } else {
    wifiConnected = false;
    WiFi.disconnect(true);
    strcpy(wifiIP, "OFFLINE");
    drawCenteredText("NO WIFI", W / 2, 45, 2, W - 32, C_AMBER);
    drawCenteredText("OFFLINE MODE", W / 2, 68, 1, W - 32, C_MUTED);
  }
  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), W, H);
  delay(250);
}

// ----------------------------------------------------------------------------
// ATTENDANCE PUNCH TRIGGER (INSTANT 0MS)
// ----------------------------------------------------------------------------
void triggerAttendancePunch(bool isMatched, uint16_t slot = 1) {
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", clockHour, clockMin, (clockHour >= 12 ? "PM" : "AM"));
  strlcpy(lastPunchTime, timeBuf, sizeof(lastPunchTime));

  if (isMatched) {
    Employee* emp = getEmployeeBySlot(slot);
    if (emp) {
      strlcpy(lastPunchName, emp->name, sizeof(lastPunchName));
      emp->isCheckedIn = !emp->isCheckedIn;
      lastPunchIsIn = emp->isCheckedIn;
    } else {
      snprintf(lastPunchName, sizeof(lastPunchName), "Employee #%d", slot);
      lastPunchIsIn = true;
    }
    lastPunchSlot = slot;
    currentScreen = STATE_MARKED;
    resultUntil = millis() + 2500;

    Serial.printf("\n>>> ATTENDANCE MARKED: %s | %s | %s <<<\n",
                  lastPunchName, lastPunchIsIn ? "IN" : "OUT", lastPunchTime);
    beepOKAsync();
  } else {
    currentScreen = STATE_ERROR;
    resultUntil = millis() + 2000;
    Serial.println("\n>>> NOT MATCHED - RETRY <<<");
    beepErrorAsync();
  }
  needsImmediateRedraw = true;
}

// ----------------------------------------------------------------------------
// NON-BLOCKING ENROLLMENT
// ----------------------------------------------------------------------------
void startEnrollment(const String& nameArg, uint16_t slotArg) {
  strlcpy(enrollTargetName, nameArg.c_str(), sizeof(enrollTargetName));
  enrollTargetSlot = slotArg;
  currentScreen = STATE_ENROLL_STEP1;
  resultUntil = millis() + 1800;
  beepAsync(70);
  needsImmediateRedraw = true;
  Serial.printf("\n>>> ENROLL: Place finger for '%s' <<<\n", enrollTargetName);
}

// ----------------------------------------------------------------------------
// STEADY, MINIMAL RENDERING SCREENS (NO BLINKING / FLICKERING)
// ----------------------------------------------------------------------------
void renderIdleScreen() {
  if (!canvas) return;
  canvas->fillScreen(C_BG);

  // 1. Top Bar: Clean minimal status with STEADY indicator dot (NO BLINKING)
  canvas->setTextSize(1);
  if (wifiConnected) {
    canvas->setTextColor(C_GREEN);
    canvas->setCursor(10, 6);
    canvas->print("WIFI: OK");
    canvas->fillCircle(W - 12, 9, 2, C_GREEN); // Steady solid green
  } else {
    canvas->setTextColor(C_MUTED);
    canvas->setCursor(10, 6);
    canvas->print("OFFLINE");
    canvas->fillCircle(W - 12, 9, 2, C_AMBER); // Steady solid amber
  }

  // 2. Center Hero: Large Crisp Digital Clock (TextSize 4, Steady)
  char timeStr[8];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", clockHour, clockMin);
  drawCenteredText(timeStr, W / 2, 28, 4, W - 10, C_WHITE);

  // Date / Day subtitle
  canvas->setTextSize(1);
  canvas->setTextColor(C_MUTED);
  drawCenteredText("THU, 27 NOV", W / 2, 66, 1, W - 20, C_MUTED);

  // 3. Bottom Pill Card: "PLACE FINGER" (Clean static reticle, NO BLINKING LASER)
  int pillY = 84;
  int pillH = 34;
  int pillW = W - 20;
  canvas->fillRoundRect(10, pillY, pillW, pillH, 5, C_CARD);
  canvas->drawRoundRect(10, pillY, pillW, pillH, 5, C_BORDER);

  // Clean static fingerprint reticle
  int retX = 18;
  int retY = pillY + 6;
  int retW = 22;
  int retH = 22;
  canvas->drawRect(retX, retY, retW, retH, C_BORDER);
  canvas->drawCircle(retX + retW / 2, retY + retH / 2, 6, C_GREEN);
  canvas->drawCircle(retX + retW / 2, retY + retH / 2, 3, C_MUTED);

  // Clean prompt text
  canvas->setTextColor(C_WHITE);
  canvas->setTextSize(1);
  canvas->setCursor(48, pillY + 13);
  canvas->print("PLACE FINGER");
}

void renderMarkedScreen() {
  if (!canvas) return;
  canvas->fillScreen(C_BG);

  // Hero Card with Green Accent Frame
  canvas->fillRoundRect(6, 6, W - 12, H - 12, 6, C_CARD);
  canvas->drawRoundRect(6, 6, W - 12, H - 12, 6, C_GREEN);

  // Big Green "MARKED" Headline
  drawCenteredText("MARKED", W / 2, 16, 2, W - 20, C_GREEN);

  // Clean divider
  canvas->drawFastHLine(20, 38, W - 40, C_BORDER);

  // Employee Name (Large & Centered)
  drawCenteredText(lastPunchName, W / 2, 48, 2, W - 24, C_WHITE);

  // Time & In/Out Status
  char statBuf[24];
  snprintf(statBuf, sizeof(statBuf), "%s • %s", lastPunchTime, lastPunchIsIn ? "IN" : "OUT");
  drawCenteredText(statBuf, W / 2, 74, 1, W - 20, C_GREEN);

  // Simple Confirmation
  drawCenteredText("ATTENDANCE LOGGED", W / 2, 94, 1, W - 20, C_MUTED);
}

void renderErrorScreen() {
  if (!canvas) return;
  canvas->fillScreen(C_BG);

  // Hero Card with Red Accent Frame
  canvas->fillRoundRect(6, 6, W - 12, H - 12, 6, C_CARD);
  canvas->drawRoundRect(6, 6, W - 12, H - 12, 6, C_RED);

  drawCenteredText("NOT MATCHED", W / 2, 28, 2, W - 20, C_RED);
  drawCenteredText("TRY AGAIN", W / 2, 62, 1, W - 20, C_WHITE);
  drawCenteredText("PLACE FINGER FIRMLY", W / 2, 84, 1, W - 20, C_MUTED);
}

void renderEnrollStep1() {
  if (!canvas) return;
  canvas->fillScreen(C_BG);
  canvas->fillRoundRect(6, 6, W - 12, H - 12, 6, C_CARD);
  canvas->drawRoundRect(6, 6, W - 12, H - 12, 6, C_AMBER);

  drawCenteredText("ENROLL STEP 1", W / 2, 20, 1, W - 20, C_AMBER);
  drawCenteredText(enrollTargetName, W / 2, 42, 2, W - 20, C_WHITE);
  drawCenteredText("PLACE FINGER", W / 2, 74, 1, W - 20, C_WHITE);
}

void renderEnrollStep2() {
  if (!canvas) return;
  canvas->fillScreen(C_BG);
  canvas->fillRoundRect(6, 6, W - 12, H - 12, 6, C_CARD);
  canvas->drawRoundRect(6, 6, W - 12, H - 12, 6, C_WHITE);

  drawCenteredText("ENROLL STEP 2", W / 2, 20, 1, W - 20, C_WHITE);
  drawCenteredText(enrollTargetName, W / 2, 42, 2, W - 20, C_WHITE);
  drawCenteredText("LIFT & PLACE AGAIN", W / 2, 74, 1, W - 20, C_GREEN);
}

void renderEnrollSuccess() {
  if (!canvas) return;
  canvas->fillScreen(C_BG);
  canvas->fillRoundRect(6, 6, W - 12, H - 12, 6, C_CARD);
  canvas->drawRoundRect(6, 6, W - 12, H - 12, 6, C_GREEN);

  drawCenteredText("ENROLLED!", W / 2, 22, 2, W - 20, C_GREEN);
  drawCenteredText(enrollTargetName, W / 2, 52, 2, W - 20, C_WHITE);
  drawCenteredText("SAVED TO MEMORY", W / 2, 82, 1, W - 20, C_MUTED);
}

// ----------------------------------------------------------------------------
// SERIAL COMMAND PARSER
// ----------------------------------------------------------------------------
void printHelp() {
  Serial.println("\nCommands:");
  Serial.println("  1 / m      : Mark attendance (Simulated)");
  Serial.println("  0 / e      : Show error");
  Serial.println("  r          : Rotate screen 180 deg");
  Serial.println("  enroll <name> : Enroll finger");
  Serial.println("  w          : Toggle wifi status");
  Serial.println("  l          : List employees\n");
}

void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.equalsIgnoreCase("1") || cmd.equalsIgnoreCase("m") || cmd.equalsIgnoreCase("mark")) {
    static int currentSimSlot = 1;
    triggerAttendancePunch(true, currentSimSlot);
    currentSimSlot = (currentSimSlot % employeeCount) + 1;

  } else if (cmd.equalsIgnoreCase("0") || cmd.equalsIgnoreCase("e") || cmd.equalsIgnoreCase("error")) {
    triggerAttendancePunch(false);

  } else if (cmd.startsWith("enroll") || cmd.startsWith("a")) {
    int spaceIdx = cmd.indexOf(' ');
    String empName = "Worker";
    uint16_t slot = employeeCount + 1;
    if (spaceIdx > 0) {
      empName = cmd.substring(spaceIdx + 1);
      empName.trim();
    } else {
      empName = "Worker " + String(slot);
    }
    startEnrollment(empName, slot);

  } else if (cmd.equalsIgnoreCase("r") || cmd.equalsIgnoreCase("rot")) {
    uint8_t nextRot = (currentRotation == 3) ? 1 : 3;
    prefs.putUChar("rot", nextRot);
    applyRotation(nextRot);

  } else if (cmd.equalsIgnoreCase("w")) {
    wifiConnected = !wifiConnected;
    needsImmediateRedraw = true;
    Serial.printf("WiFi: %s\n", wifiConnected ? "ONLINE" : "OFFLINE");

  } else if (cmd.equalsIgnoreCase("b") || cmd.equalsIgnoreCase("led") || cmd.equalsIgnoreCase("blue")) {
    turnOffBlueLED();
    Serial.println("[LED] Blue LED turned OFF.");

  } else if (cmd.equalsIgnoreCase("l")) {
    Serial.printf("\nEmployees (%d):\n", employeeCount);
    for (int i = 0; i < employeeCount; i++) {
      Serial.printf("  #%d: %s (%s)\n", employees[i].id, employees[i].name,
                    employees[i].isCheckedIn ? "IN" : "OUT");
    }

  } else {
    printHelp();
  }
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(60);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  SPI.setFrequency(27000000);
  tft.initR(INITR_BLACKTAB);

  loadEmployeeDatabase();
  applyRotation(currentRotation); // Default 3 (180 deg)
  tft.fillScreen(C_BG);

  runBootSequence();

  fpSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  if (finger.verifyPassword()) {
    sensorHardwareFound = true;
    Serial.println("[SENSOR] Optical sensor active");
  } else {
    sensorHardwareFound = false;
    Serial.println("[SENSOR] Simulation mode active");
  }

  // Turn off Blue LEDs immediately
  turnOffBlueLED();

  printHelp();
  needsImmediateRedraw = true;
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // 1. Non-blocking Async Buzzer
  updateBuzzer();

  // 2. Clock Tick (updates once per second, triggering steady redraw)
  if (now - lastSecTick >= 1000) {
    lastSecTick = now;
    clockSec++;
    if (clockSec >= 60) {
      clockSec = 0;
      clockMin++;
      if (clockMin >= 60) {
        clockMin = 0;
        clockHour = (clockHour + 1) % 24;
      }
    }
    if (currentScreen == STATE_IDLE) {
      needsImmediateRedraw = true; // Redraw once per second
    }
  }

  // 3. Serial Monitor Commands
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialInput.length() > 0) {
        processSerialCommand(serialInput);
        serialInput = "";
      }
    } else {
      serialInput += c;
    }
  }

  // 4. BOOT Button: Press triggers instant scan
  int bootBtn = digitalRead(BOOT_BUTTON);
  if (bootBtn == LOW && lastBootButtonState == HIGH) {
    bootButtonDownTime = now;
  } else if (bootBtn == HIGH && lastBootButtonState == LOW) {
    unsigned long duration = now - bootButtonDownTime;
    if (duration > 1500) {
      String autoName = "Worker " + String(employeeCount + 1);
      startEnrollment(autoName, employeeCount + 1);
    } else {
      triggerAttendancePunch(true, 1);
    }
  }
  lastBootButtonState = bootBtn;

  // 5. Optical Fingerprint Sensor:
  // "if any keep finger it show marked"
  if (sensorHardwareFound && currentScreen == STATE_IDLE) {
    static unsigned long lastSensorPoll = 0;
    if (now - lastSensorPoll >= 50) { // Fast 50ms polling for instant touch detection
      lastSensorPoll = now;
      int p = finger.getImage();
      if (p == FINGERPRINT_OK) { // ANY finger placed on the glass!
        p = finger.image2Tz();
        uint16_t matchedSlot = 1;
        if (p == FINGERPRINT_OK) {
          p = finger.fingerFastSearch();
          if (p == FINGERPRINT_OK) {
            matchedSlot = finger.fingerID;
          }
        }
        triggerAttendancePunch(true, matchedSlot);
      }
    }
  }

  // 6. Non-blocking Screen State Transitions
  if (currentScreen == STATE_ENROLL_STEP1 && now >= resultUntil) {
    currentScreen = STATE_ENROLL_STEP2;
    resultUntil = now + 1600;
    beepAsync(50);
    needsImmediateRedraw = true;
  } else if (currentScreen == STATE_ENROLL_STEP2 && now >= resultUntil) {
    saveNewEmployee(enrollTargetSlot, enrollTargetName);
    currentScreen = STATE_ENROLL_SUCCESS;
    resultUntil = now + 2000;
    beepOKAsync();
    needsImmediateRedraw = true;
  } else if (currentScreen != STATE_IDLE && now >= resultUntil) {
    currentScreen = STATE_IDLE;
    needsImmediateRedraw = true;
  }

  // 7. Event-driven Screen Refresh (ZERO FLICKER, ZERO BLINKING)
  if (needsImmediateRedraw) {
    needsImmediateRedraw = false;

    if (canvas) {
      switch (currentScreen) {
        case STATE_MARKED:
          renderMarkedScreen();
          break;
        case STATE_ERROR:
          renderErrorScreen();
          break;
        case STATE_ENROLL_STEP1:
          renderEnrollStep1();
          break;
        case STATE_ENROLL_STEP2:
          renderEnrollStep2();
          break;
        case STATE_ENROLL_SUCCESS:
          renderEnrollSuccess();
          break;
        case STATE_IDLE:
        default:
          renderIdleScreen();
          break;
      }
      tft.drawRGBBitmap(0, 0, canvas->getBuffer(), W, H);
      turnOffBlueLED();
    }
  }

  delay(2);
}
