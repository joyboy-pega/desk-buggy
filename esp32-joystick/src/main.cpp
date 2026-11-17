#include <Arduino.h>
#include <HijelHID_BLEMouse.h>

// ============================================================================
// ESP32 PURE JOYSTICK BLUETOOTH MOUSE FOR MACBOOK
// ============================================================================
// No screen needed — only 5 jumper wires between ESP32 and Joystick!
//
// Hardware Wiring:
//   Joystick GND  --> ESP32 GND
//   Joystick +5V  --> ESP32 3.3V  (CRITICAL: Do NOT use 5V on ESP32 analog pins!)
//   Joystick VRx  --> ESP32 GPIO 34 (ADC1_CH6)
//   Joystick VRy  --> ESP32 GPIO 35 (ADC1_CH7)
//   Joystick SW   --> ESP32 GPIO 25 (Digital input with pullup)
//
// macOS Controls:
//   - Move Joystick     : Smooth cursor tracking with acceleration
//   - Short Click (SW)  : Left Click
//   - Hold Click (SW)   : Left Click-and-Drag (drag files, windows, text)
//   - Double Click (SW) : Right Click (context menu)
//   - Built-in LED (D2) : Blinks while pairing, Solid ON when connected to Mac
// ============================================================================

#define PIN_JOY_X   34   // VRx
#define PIN_JOY_Y   35   // VRy
#define PIN_JOY_SW  25   // Pushbutton (SW)
#define PIN_LED      2   // Built-in Blue LED on ESP32 Dev Board

// Calibration & Tuning
const int DEADZONE        = 170;   // Jitter rejection threshold (0-2048)
const int MAX_SPEED       = 16;    // Top speed when joystick pushed to edge
const bool INVERT_X       = false; // Set to true if X is mirrored
const bool INVERT_Y       = true;  // Joysticks read inverted Y by default on Mac

// Bluetooth LE Mouse Instance (Shows as "MacBook Joystick Mouse")
HijelBLEMouse bleMouse("MacBook Joystick Mouse", "DeskBuggy", 100);

// Calibration center points
int centerX = 2048;
int centerY = 2048;

// Button timing state
int buttonLast = HIGH;
unsigned long buttonDownTime = 0;
unsigned long buttonUpTime = 0;
bool isDragging = false;
const unsigned long HOLD_DRAG_MS   = 320; // Hold longer than this = drag
const unsigned long DOUBLE_TAP_MS  = 260; // Double tap gap = right click

// LED Blink Timer
unsigned long lastLedBlink = 0;
bool ledState = false;

// ----------------------------------------------------------------------------
// Read Oversampled ADC (Averages 4 samples to eliminate potentiometer noise)
// ----------------------------------------------------------------------------
int readSmoothADC(int pin) {
  int sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(pin);
  }
  return sum / 4;
}

// ----------------------------------------------------------------------------
// Compute Smooth macOS Acceleration Curve
// ----------------------------------------------------------------------------
int computeDelta(int rawVal, int centerVal, bool inverted) {
  int diff = rawVal - centerVal;
  if (abs(diff) <= DEADZONE) return 0;

  int sign = (diff > 0) ? 1 : -1;
  int magnitude = min(abs(diff) - DEADZONE, 2048 - DEADZONE);

  // Power curve (exponent 1.5) provides pixel-accurate micro movement
  // while still reaching full-screen travel speed at stick limits.
  float norm = (float)magnitude / (float)(2048 - DEADZONE);
  float speed = powf(norm, 1.5f) * MAX_SPEED;

  int delta = (int)roundf(speed) * sign;
  if (delta == 0 && magnitude > 0) delta = sign; // Minimum 1px step
  return inverted ? -delta : delta;
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n================================================");
  Serial.println("  MACBOOK BLUETOOTH JOYSTICK MOUSE (STANDALONE) ");
  Serial.println("================================================");

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  analogReadResolution(12); // 12-bit ADC (0 - 4095)

  // Auto-calibrate rest position: do not touch the stick during boot
  Serial.println("[CALIBRATION] Calibrating resting center...");
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 64; i++) {
    sumX += analogRead(PIN_JOY_X);
    sumY += analogRead(PIN_JOY_Y);
    delay(8);
  }
  centerX = sumX / 64;
  centerY = sumY / 64;
  Serial.printf("[CALIBRATION] Rest Center -> X: %d | Y: %d\n", centerX, centerY);

  // Start Bluetooth LE Mouse Service
  bleMouse.begin();
  Serial.println("[BLE] Advertising as 'MacBook Joystick Mouse'");
  Serial.println("[BLE] Open Mac Settings > Bluetooth > Connect!");
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------
void loop() {
  bool connected = bleMouse.isConnected();
  unsigned long now = millis();

  // 1. Status LED: Solid ON when connected, Blinking when pairing
  if (connected) {
    digitalWrite(PIN_LED, HIGH);
  } else {
    if (now - lastLedBlink >= 400) {
      lastLedBlink = now;
      ledState = !ledState;
      digitalWrite(PIN_LED, ledState ? HIGH : LOW);
    }
  }

  // 2. Read Joystick Analog Values
  int rawX = readSmoothADC(PIN_JOY_X);
  int rawY = readSmoothADC(PIN_JOY_Y);

  // 3. Compute Delta Movements
  int dx = computeDelta(rawX, centerX, INVERT_X);
  int dy = computeDelta(rawY, centerY, INVERT_Y);

  // 4. Send Mouse Movement to Mac
  if (connected && (dx != 0 || dy != 0)) {
    bleMouse.move(dx, dy);
  }

  // 5. Handle Joystick Button (SW)
  int sw = digitalRead(PIN_JOY_SW);

  // Button just pressed down
  if (sw == LOW && buttonLast == HIGH) {
    buttonDownTime = now;
  }
  // Button held down (Check for Drag)
  else if (sw == LOW && buttonLast == LOW) {
    if (!isDragging && (now - buttonDownTime >= HOLD_DRAG_MS)) {
      isDragging = true;
      if (connected) {
        bleMouse.press(MouseButton::Left);
        Serial.println("[MOUSE] Drag Press (Left Button Held)");
      }
    }
  }
  // Button released
  else if (sw == HIGH && buttonLast == LOW) {
    unsigned long pressDuration = now - buttonDownTime;

    if (isDragging) {
      isDragging = false;
      if (connected) {
        bleMouse.release(MouseButton::Left);
        Serial.println("[MOUSE] Drag Release");
      }
    } else if (pressDuration < HOLD_DRAG_MS) {
      // Check for Double Click -> Right Click on Mac
      if (now - buttonUpTime < DOUBLE_TAP_MS) {
        if (connected) {
          bleMouse.click(MouseButton::Right);
          Serial.println("[MOUSE] Double Tap -> Right Click");
        }
      } else {
        // Single Click -> Left Click
        if (connected) {
          bleMouse.click(MouseButton::Left);
          Serial.println("[MOUSE] Single Tap -> Left Click");
        }
      }
    }
    buttonUpTime = now;
  }

  buttonLast = sw;

  delay(8); // ~125 Hz polling for low-latency responsiveness
}
