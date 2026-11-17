#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <HijelHID_BLEMouse.h>

// ============================================================================
// ESP32 BLE WIRELESS JOYSTICK MOUSE & REAL-TIME SCREEN HUD
// ============================================================================
// Hardware:
//   - ESP32-WROOM-32 Dev Module
//   - 2-Axis Analog Joystick (R103 / B103 10K Potentiometer + Pushbutton)
//   - 1.8" ST7735 Color SPI TFT Display (160x128)
//
// Features:
//   - Native Bluetooth LE Wireless HID Mouse (Mac, Windows, Linux, iOS, Android)
//   - Smooth non-linear acceleration curve + deadzone jitter filter
//   - Smart button: Short click = Left Click, Hold = Drag, Double Click = Right Click
//   - Real-time on-screen radar crosshair tracking joystick position & clicks
// ============================================================================

// ---------- JOYSTICK PINS (Must use ADC1 for Bluetooth stability) ----------
#define PIN_JOY_X   34   // VRx (ADC1 Channel 6)
#define PIN_JOY_Y   35   // VRy (ADC1 Channel 7)
#define PIN_JOY_SW  25   // Pushbutton (Digital input with pullup)

// ---------- ST7735 TFT PINS (VSPI) ----------
#define TFT_CS       5   // Chip Select
#define TFT_DC       2   // Data/Command (A0)
#define TFT_RST      4   // Reset
#define TFT_MOSI    23   // VSPI MOSI (SDA)
#define TFT_SCLK    18   // VSPI SCK (SCL)
#define TFT_BL      32   // Backlight PWM pin

#define SCREEN_W   160
#define SCREEN_H   128

// ---------- MOUSE TUNING CONSTANTS ----------
const int DEADZONE       = 180;   // Jitter rejection threshold (0 - 2048)
const int MAX_SPEED      = 15;    // Maximum mouse travel speed per frame
const bool INVERT_X      = false; // Set to true if X axis is inverted
const bool INVERT_Y      = true;  // Joysticks typically have Y inverted vs mouse

// Objects
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
HijelBLEMouse bleMouse("DeskBuggy Mouse", "DeskBuggy", 100);

// Joystick Calibration Offsets
int centerX = 2048;
int centerY = 2048;

// Live State
int rawX = 2048;
int rawY = 2048;
int deltaX = 0;
int deltaY = 0;

// Virtual on-screen cursor position (for HUD radar)
float virtualCursorX = 80.0f;
float virtualCursorY = 64.0f;

// Button state machine (Click, Drag, Double-click)
int buttonCurrent = HIGH;
int buttonLast = HIGH;
unsigned long buttonDownTime = 0;
unsigned long buttonUpTime = 0;
bool isDragging = false;
int clickCount = 0;
const unsigned long HOLD_THRESHOLD = 350;      // ms for click-and-drag
const unsigned long DOUBLE_CLICK_GAP = 280;    // ms between clicks

// Display refresh timing
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 33;     // ~30 FPS HUD refresh

// ----------------------------------------------------------------------------
// Joystick Acceleration & Deadzone Curve
// ----------------------------------------------------------------------------
int computeMouseDelta(int rawVal, int centerVal, bool inverted) {
  int diff = rawVal - centerVal;
  if (abs(diff) <= DEADZONE) {
    return 0;
  }

  int sign = (diff > 0) ? 1 : -1;
  int magnitude = abs(diff) - DEADZONE;
  int maxMag = 2048 - DEADZONE;
  if (magnitude > maxMag) magnitude = maxMag;

  // Non-linear power curve for pinpoint fine control + fast sweep
  float normalized = (float)magnitude / (float)maxMag;
  float speed = powf(normalized, 1.45f) * MAX_SPEED;

  int delta = (int)roundf(speed) * sign;
  if (delta == 0 && magnitude > 0) delta = sign;

  return inverted ? -delta : delta;
}

// ----------------------------------------------------------------------------
// Draw Real-time HUD on TFT Screen
// ----------------------------------------------------------------------------
void updateScreenHUD(bool connected) {
  // Clear interior of Radar area to prevent ghosting
  const int radarX = 48;
  const int radarY = 26;
  const int radarW = 64;
  const int radarH = 64;

  // Header Bar (Status)
  tft.fillRect(0, 0, SCREEN_W, 18, 0x10A2); // Dark slate header
  tft.setCursor(4, 5);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.print("DESKBUGGY");

  // BLE Connection Pill
  if (connected) {
    tft.fillRect(94, 3, 62, 12, 0x03E0); // Emerald green
    tft.setCursor(98, 5);
    tft.setTextColor(ST77XX_BLACK);
    tft.print("BT PAIRED");
  } else {
    tft.fillRect(94, 3, 62, 12, 0xFD20); // Amber orange
    tft.setCursor(98, 5);
    tft.setTextColor(ST77XX_BLACK);
    tft.print("PAIRING..");
  }

  // Draw Radar box & reticle
  tft.fillRect(radarX, radarY, radarW, radarH, 0x0841); // Very dark gray
  tft.drawRect(radarX, radarY, radarW, radarH, 0x3186);

  // Crosshairs
  tft.drawFastHLine(radarX + 4, radarY + radarH / 2, radarW - 8, 0x2104);
  tft.drawFastVLine(radarX + radarW / 2, radarY + 4, radarH - 8, 0x2104);

  // Calculate position within radar
  int plotX = radarX + (radarW / 2) + ((rawX - centerX) * (radarW / 2 - 4)) / 2048;
  int plotY = radarY + (radarH / 2) + ((rawY - centerY) * (radarH / 2 - 4)) / 2048;

  // Draw joystick target indicator
  uint16_t cursorColor = isDragging ? ST77XX_YELLOW : (buttonCurrent == LOW ? ST77XX_RED : ST77XX_CYAN);
  tft.fillCircle(plotX, plotY, 3, cursorColor);
  tft.drawCircle(plotX, plotY, 6, cursorColor);

  // Bottom Telemetry Info Box
  tft.fillRect(0, 96, SCREEN_W, 32, 0x0000);
  tft.drawFastHLine(0, 95, SCREEN_W, 0x2965);

  tft.setCursor(4, 100);
  tft.setTextColor(0xAD55); // Muted gray
  tft.print("DX:");
  tft.setTextColor(ST77XX_WHITE);
  tft.printf("%+3d ", deltaX);

  tft.setTextColor(0xAD55);
  tft.print("DY:");
  tft.setTextColor(ST77XX_WHITE);
  tft.printf("%+3d ", deltaY);

  tft.setCursor(4, 114);
  tft.setTextColor(0xAD55);
  tft.print("SW:");
  if (isDragging) {
    tft.setTextColor(ST77XX_YELLOW);
    tft.print("DRAG [HOLD]");
  } else if (buttonCurrent == LOW) {
    tft.setTextColor(ST77XX_RED);
    tft.print("CLICK [LEFT]");
  } else {
    tft.setTextColor(0x7BEF);
    tft.print("IDLE");
  }

  // Right side: Sensitivity indicator
  tft.setCursor(108, 100);
  tft.setTextColor(0xAD55);
  tft.print("RATE:50Hz");
  tft.setCursor(108, 114);
  tft.setTextColor(0xAD55);
  tft.printf("CAL:%d", DEADZONE);
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n============================================");
  Serial.println("  DESK BUGGY — BLE WIRELESS JOYSTICK MOUSE  ");
  Serial.println("============================================");

  // Setup Joystick pins
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  analogReadResolution(12); // 0 - 4095

  // Setup Backlight PWM on ST7735
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize ST7735 TFT
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); // Landscape 160x128
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.println("CALIBRATING JOYSTICK...");
  tft.println("DO NOT TOUCH STICK.");

  // Auto-calibrate center resting position
  long sumX = 0;
  long sumY = 0;
  for (int i = 0; i < 64; i++) {
    sumX += analogRead(PIN_JOY_X);
    sumY += analogRead(PIN_JOY_Y);
    delay(10);
  }
  centerX = sumX / 64;
  centerY = sumY / 64;

  Serial.printf("[CALIBRATION] Rest Center X: %d | Y: %d\n", centerX, centerY);

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("JOYSTICK READY!");
  tft.setTextColor(ST77XX_WHITE);
  tft.println("STARTING BLE MOUSE...");

  // Start BLE Mouse
  bleMouse.begin();
  Serial.println("[BLE] Advertising as 'DeskBuggy Mouse'...");
  delay(500);

  tft.fillScreen(ST77XX_BLACK);
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
  // 1. Read Analog Joystick
  rawX = analogRead(PIN_JOY_X);
  rawY = analogRead(PIN_JOY_Y);

  // 2. Compute Velocity Deltas
  deltaX = computeMouseDelta(rawX, centerX, INVERT_X);
  deltaY = computeMouseDelta(rawY, centerY, INVERT_Y);

  bool isConnected = bleMouse.isConnected();

  // 3. Send Mouse Move over BLE
  if (isConnected && (deltaX != 0 || deltaY != 0)) {
    bleMouse.move(deltaX, deltaY);
  }

  // 4. Handle Joystick Pushbutton (SW)
  int swRead = digitalRead(PIN_JOY_SW);
  unsigned long now = millis();

  // Button pressed down
  if (swRead == LOW && buttonLast == HIGH) {
    buttonDownTime = now;
  }
  // Button held down (Check for Drag)
  else if (swRead == LOW && buttonLast == LOW) {
    if (!isDragging && (now - buttonDownTime >= HOLD_THRESHOLD)) {
      isDragging = true;
      if (isConnected) {
        bleMouse.press(MouseButton::Left);
        Serial.println("[MOUSE] Drag Press (Left Button Held)");
      }
    }
  }
  // Button released
  else if (swRead == HIGH && buttonLast == LOW) {
    unsigned long pressDuration = now - buttonDownTime;

    if (isDragging) {
      isDragging = false;
      if (isConnected) {
        bleMouse.release(MouseButton::Left);
        Serial.println("[MOUSE] Drag Release");
      }
    } else if (pressDuration < HOLD_THRESHOLD) {
      // Check for double click
      if (now - buttonUpTime < DOUBLE_CLICK_GAP) {
        // Double click = Right Mouse Click!
        if (isConnected) {
          bleMouse.click(MouseButton::Right);
          Serial.println("[MOUSE] Double Click -> Right Click!");
        }
      } else {
        // Single click = Left Mouse Click!
        if (isConnected) {
          bleMouse.click(MouseButton::Left);
          Serial.println("[MOUSE] Single Click -> Left Click");
        }
      }
    }
    buttonUpTime = now;
  }

  buttonLast = swRead;
  buttonCurrent = swRead;

  // 5. Refresh Screen HUD (~30 FPS)
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    updateScreenHUD(isConnected);
  }

  delay(10); // Loop pacing (~100 Hz polling)
}
