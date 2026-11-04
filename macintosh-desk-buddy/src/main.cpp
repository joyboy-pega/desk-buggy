#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "mac_bitmaps.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum MacMood {
  MOOD_HAPPY,
  MOOD_WINK,
  MOOD_THINKING,
  MOOD_DESKTOP,
  MOOD_SLEEPY,
  MOOD_SURPRISED,
  MOOD_HELLO
};

MacMood currentMood = MOOD_HELLO;
unsigned long moodStartTime = 0;
uint8_t oledAddress = 0x3C;

// Forward Declarations
void drawClassicMacWindow(const char* title);
void drawHappyMacFace(int eyeOffsetX, int eyeOffsetY, int blink);
void drawMacintoshChassis(int cx, int cy, bool happy);
void drawDesktop();
void drawSleepyMac(int frame);
void drawThinkingMac(int frame);
void drawHelloMac();

uint8_t scanI2C(int sda, int scl) {
  Wire.begin(sda, scl);
  delay(100);
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      return addr;
    }
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32-C3 SuperMini Macintosh Desk Buddy (0.96\" OLED)  ");
  Serial.println("========================================================");

  oledAddress = scanI2C(I2C_SDA, I2C_SCL);
  if (oledAddress == 0) {
    oledAddress = scanI2C(9, 8);
    if (oledAddress != 0) {
      Wire.begin(9, 8);
    } else {
      Wire.begin(I2C_SDA, I2C_SCL);
      oledAddress = 0x3C;
    }
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddress)) {
    Serial.printf("[ERROR] OLED initialization failed at 0x%02X!\n", oledAddress);
    while (1) delay(1000);
  }

  Serial.printf("[SUCCESS] Macintosh Desk Buddy OLED online at 0x%02X!\n", oledAddress);
  display.clearDisplay();
  display.display();

  currentMood = MOOD_HELLO;
  moodStartTime = millis();
}

// -------------------------------------------------------------
// Graphic Primitives: Classic 1984 Macintosh Bezels & CRT Screen
// -------------------------------------------------------------
void drawClassicMacWindow(const char* title) {
  // Retro drop shadow window
  display.fillRect(4, 2, 120, 58, SSD1306_BLACK);
  display.drawRect(2, 0, 122, 60, SSD1306_WHITE);
  display.drawRect(4, 2, 118, 56, SSD1306_WHITE);

  // Title bar stripes
  for (int y = 5; y <= 11; y += 2) {
    display.drawFastHLine(6, y, 114, SSD1306_WHITE);
  }

  // Title Box
  int titleWidth = strlen(title) * 6 + 8;
  int titleX = (128 - titleWidth) / 2;
  display.fillRect(titleX, 3, titleWidth, 11, SSD1306_BLACK);
  display.drawRect(titleX, 3, titleWidth, 11, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(titleX + 4, 5);
  display.print(title);
}

// Draw the cute miniature 1984 Macintosh computer
void drawMacintoshChassis(int cx, int cy, bool happy) {
  // Main body
  display.drawRoundRect(cx - 24, cy - 28, 48, 56, 4, SSD1306_WHITE);
  // Beveled chin
  display.drawFastHLine(cx - 22, cy + 18, 44, SSD1306_WHITE);
  // Floppy drive slot
  display.fillRect(cx + 2, cy + 21, 16, 2, SSD1306_WHITE);
  // Tiny Apple logo position (dot)
  display.drawPixel(cx - 16, cy + 22, SSD1306_WHITE);

  // CRT Screen
  display.drawRoundRect(cx - 19, cy - 23, 38, 38, 3, SSD1306_WHITE);

  if (happy) {
    // Eyes
    display.fillRect(cx - 10, cy - 12, 4, 5, SSD1306_WHITE);
    display.fillRect(cx + 6, cy - 12, 4, 5, SSD1306_WHITE);
    // Smile
    display.drawLine(cx - 10, cy + 2, cx - 8, cy + 6, SSD1306_WHITE);
    display.drawLine(cx - 8, cy + 6, cx + 8, cy + 6, SSD1306_WHITE);
    display.drawLine(cx + 8, cy + 6, cx + 10, cy + 2, SSD1306_WHITE);
  }
}

// -------------------------------------------------------------
// Face Animations (Expressions on the CRT Screen)
// -------------------------------------------------------------
void drawHappyMacFace(int eyeOffsetX, int eyeOffsetY, int blink) {
  display.clearDisplay();

  // CRT Inner Bezel
  display.drawRoundRect(8, 2, 112, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(10, 4, 108, 56, 4, SSD1306_WHITE);

  int leftEyeX = 42 + eyeOffsetX;
  int rightEyeX = 86 + eyeOffsetX;
  int eyeY = 24 + eyeOffsetY;

  if (blink == 0) {
    // Wide open Happy Mac eyes (characteristic rounded squares)
    display.fillRoundRect(leftEyeX - 6, eyeY - 7, 12, 14, 2, SSD1306_WHITE);
    display.fillRoundRect(rightEyeX - 6, eyeY - 7, 12, 14, 2, SSD1306_WHITE);
    // Eye shine
    display.drawPixel(leftEyeX + 2, eyeY - 4, SSD1306_BLACK);
    display.drawPixel(rightEyeX + 2, eyeY - 4, SSD1306_BLACK);
  } else if (blink == 1) {
    // Half blink
    display.fillRoundRect(leftEyeX - 6, eyeY - 2, 12, 6, 2, SSD1306_WHITE);
    display.fillRoundRect(rightEyeX - 6, eyeY - 2, 12, 6, 2, SSD1306_WHITE);
  } else {
    // Closed happy curved eye slits (^_^)
    display.drawFastHLine(leftEyeX - 7, eyeY, 14, SSD1306_WHITE);
    display.drawFastHLine(rightEyeX - 7, eyeY, 14, SSD1306_WHITE);
  }

  // Classic Macintosh Smile
  display.drawLine(44, 44, 48, 48, SSD1306_WHITE);
  display.drawLine(48, 48, 80, 48, SSD1306_WHITE);
  display.drawLine(80, 48, 84, 44, SSD1306_WHITE);
  display.drawLine(48, 49, 80, 49, SSD1306_WHITE); // thicker bottom

  display.display();
}

void drawWinkMac(int frame) {
  display.clearDisplay();
  display.drawRoundRect(8, 2, 112, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(10, 4, 108, 56, 4, SSD1306_WHITE);

  int leftEyeX = 42;
  int rightEyeX = 86;
  int eyeY = 24;

  // Left eye: Open & Happy
  display.fillRoundRect(leftEyeX - 6, eyeY - 7, 12, 14, 2, SSD1306_WHITE);
  display.drawPixel(leftEyeX + 2, eyeY - 4, SSD1306_BLACK);

  // Right eye: Winking (Playful slant)
  display.drawLine(rightEyeX - 8, eyeY - 2, rightEyeX, eyeY + 3, SSD1306_WHITE);
  display.drawLine(rightEyeX, eyeY + 3, rightEyeX + 8, eyeY - 2, SSD1306_WHITE);
  display.drawLine(rightEyeX - 8, eyeY - 1, rightEyeX, eyeY + 4, SSD1306_WHITE);
  display.drawLine(rightEyeX, eyeY + 4, rightEyeX + 8, eyeY - 1, SSD1306_WHITE);

  // Grinning Cheeky Smile
  display.drawLine(44, 42, 50, 48, SSD1306_WHITE);
  display.drawLine(50, 48, 78, 48, SSD1306_WHITE);
  display.drawLine(78, 48, 86, 40, SSD1306_WHITE);
  display.fillCircle(84, 41, 2, SSD1306_WHITE); // smirk dimple

  // Little sparkle star near wink
  if (frame % 2 == 0) {
    display.drawPixel(102, 14, SSD1306_WHITE);
    display.drawLine(100, 14, 104, 14, SSD1306_WHITE);
    display.drawLine(102, 12, 102, 16, SSD1306_WHITE);
  }

  display.display();
}

void drawSleepyMac(int frame) {
  display.clearDisplay();
  display.drawRoundRect(8, 2, 112, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(10, 4, 108, 56, 4, SSD1306_WHITE);

  // Sleepy closed eyelids
  display.drawLine(36, 28, 42, 32, SSD1306_WHITE);
  display.drawLine(42, 32, 48, 28, SSD1306_WHITE);

  display.drawLine(80, 28, 86, 32, SSD1306_WHITE);
  display.drawLine(86, 32, 92, 28, SSD1306_WHITE);

  // Small 'o' resting mouth
  display.drawCircle(64, 45, 3, SSD1306_WHITE);

  // Floating Zzz...
  int zOffset = (frame * 2) % 24;
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(96, 26 - zOffset);
  display.print("z");
  display.setCursor(104, 18 - zOffset);
  display.print("Z");

  display.display();
}

void drawThinkingMac(int frame) {
  display.clearDisplay();
  display.drawRoundRect(8, 2, 112, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(10, 4, 108, 56, 4, SSD1306_WHITE);

  // Eyes looking up-right in thought
  display.fillRoundRect(42 - 6, 20 - 7, 12, 14, 2, SSD1306_WHITE);
  display.fillRoundRect(86 - 6, 20 - 7, 12, 14, 2, SSD1306_WHITE);
  display.fillRect(44, 15, 4, 4, SSD1306_BLACK);
  display.fillRect(88, 15, 4, 4, SSD1306_BLACK);

  // Puzzled slightly wavy mouth
  display.drawLine(48, 46, 60, 44, SSD1306_WHITE);
  display.drawLine(60, 44, 76, 47, SSD1306_WHITE);

  // Draw the classic rotating retro Mac wristwatch in top corner
  int wx = 100;
  int wy = 12;
  display.drawBitmap(wx, wy, mac_watch, 14, 14, SSD1306_WHITE);

  display.display();
}

void drawSurprisedMac() {
  display.clearDisplay();
  display.drawRoundRect(8, 2, 112, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(10, 4, 108, 56, 4, SSD1306_WHITE);

  // Big wide circular eyes
  display.fillCircle(42, 22, 10, SSD1306_WHITE);
  display.fillCircle(86, 22, 10, SSD1306_WHITE);
  display.fillCircle(42, 22, 4, SSD1306_BLACK);
  display.fillCircle(86, 22, 4, SSD1306_BLACK);

  // Open "O" mouth
  display.fillCircle(64, 46, 7, SSD1306_WHITE);
  display.fillCircle(64, 46, 5, SSD1306_BLACK);

  display.display();
}

// -------------------------------------------------------------
// Classic System 1.0 Retro Macintosh Desktop Mode
// -------------------------------------------------------------
void drawDesktop(int cursorX, int cursorY) {
  display.clearDisplay();

  // Top Menu Bar
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  // Apple logo
  display.drawBitmap(4, 0, apple_icon, 10, 12, SSD1306_BLACK);

  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(18, 1);
  display.print("File  Edit  Desk");

  // Retro desktop icons
  // Floppy icon labeled "Macintosh"
  display.drawBitmap(10, 18, mac_floppy, 16, 16, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 38);
  display.print("System");

  // Miniature Mac Icon
  drawMacintoshChassis(100, 36, true);
  display.setCursor(85, 52);
  display.print("Buddy");

  // Moving classic arrow cursor
  display.drawBitmap(cursorX, cursorY, mac_cursor, 11, 16, SSD1306_WHITE);

  display.display();
}

// -------------------------------------------------------------
// The Iconic 1984 "hello." Boot Introduction
// -------------------------------------------------------------
void drawHelloMac() {
  display.clearDisplay();

  // Draw tiny Macintosh in center
  drawMacintoshChassis(32, 32, true);

  // Cursive "hello." in classic Mac typography
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(62, 24);
  display.print("hello.");

  display.setTextSize(1);
  display.setCursor(64, 44);
  display.print("1984-2026");

  display.display();
}

// -------------------------------------------------------------
// Main Loop: Lifelike Mood Machine
// -------------------------------------------------------------
int frameCount = 0;

void loop() {
  // Check for incoming serial mood triggers
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    if (cmd == "happy") currentMood = MOOD_HAPPY;
    else if (cmd == "wink") currentMood = MOOD_WINK;
    else if (cmd == "think") currentMood = MOOD_THINKING;
    else if (cmd == "sleep") currentMood = MOOD_SLEEPY;
    else if (cmd == "surprise" || cmd == "wow") currentMood = MOOD_SURPRISED;
    else if (cmd == "desktop") currentMood = MOOD_DESKTOP;
    else if (cmd == "hello") currentMood = MOOD_HELLO;
    moodStartTime = millis();
    Serial.printf("[DESK BUDDY] Switched to mood: %s\n", cmd.c_str());
  }

  unsigned long elapsed = millis() - moodStartTime;

  switch (currentMood) {
    case MOOD_HELLO:
      drawHelloMac();
      if (elapsed > 4000) {
        currentMood = MOOD_HAPPY;
        moodStartTime = millis();
      }
      delay(100);
      break;

    case MOOD_HAPPY:
      // Natural eye movements and random blinks
      if (elapsed < 1800) {
        drawHappyMacFace(0, 0, 0); // look center
      } else if (elapsed < 2000) {
        drawHappyMacFace(0, 0, 1); // blink down
      } else if (elapsed < 2100) {
        drawHappyMacFace(0, 0, 2); // closed
      } else if (elapsed < 2250) {
        drawHappyMacFace(0, 0, 0); // open
      } else if (elapsed < 3800) {
        drawHappyMacFace(-6, 0, 0); // look left
      } else if (elapsed < 5400) {
        drawHappyMacFace(6, 0, 0); // look right
      } else {
        // Switch to next mood
        currentMood = MOOD_WINK;
        moodStartTime = millis();
      }
      delay(40);
      break;

    case MOOD_WINK:
      drawWinkMac(frameCount++);
      if (elapsed > 3500) {
        currentMood = MOOD_THINKING;
        moodStartTime = millis();
      }
      delay(100);
      break;

    case MOOD_THINKING:
      drawThinkingMac(frameCount++);
      if (elapsed > 4000) {
        currentMood = MOOD_DESKTOP;
        moodStartTime = millis();
      }
      delay(150);
      break;

    case MOOD_DESKTOP: {
      // Animate mouse cursor gliding from (20, 20) to (95, 34)
      float progress = (float)elapsed / 4000.0f;
      if (progress > 1.0f) progress = 1.0f;
      int curX = 20 + (int)(progress * 75);
      int curY = 20 + (int)(sin(progress * 3.14159f) * 14);
      drawDesktop(curX, curY);

      if (elapsed > 5000) {
        currentMood = MOOD_SURPRISED;
        moodStartTime = millis();
      }
      delay(30);
      break;
    }

    case MOOD_SURPRISED:
      drawSurprisedMac();
      if (elapsed > 2500) {
        currentMood = MOOD_SLEEPY;
        moodStartTime = millis();
      }
      delay(100);
      break;

    case MOOD_SLEEPY:
      drawSleepyMac(frameCount++);
      if (elapsed > 6000) {
        currentMood = MOOD_HAPPY;
        moodStartTime = millis();
      }
      delay(150);
      break;
  }
}

