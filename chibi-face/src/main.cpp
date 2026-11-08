#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "chibi_bitmaps.h"

// ============================================================================
// Chibi Cute Face with Motion for ESP32-C3 SuperMini + 0.96" OLED (SSD1306)
// ============================================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Target 60 FPS Engine Timing
const uint32_t TARGET_FRAME_MICROS = 16666; // 60 FPS (~16.6ms)
uint32_t lastFrameMicros = 0;
uint8_t displayContrast = 0x3F; // Soft, comfortable brightness

// ----------------------------------------------------------------------------
// Expressions & States
// ----------------------------------------------------------------------------
enum ChibiExpression {
  EXPR_HAPPY_IDLE,    // Default: Giant sparkly anime eyes, :3 cat mouth, blushing cheeks
  EXPR_JOY_ARCH,      // Maximum happiness: Happy curved eyes ^ ^, open smile, floating hearts
  EXPR_PLAYFUL_WINK,  // Playful wink: Left eye ^, right eye star sparkle, tongue poke :P
  EXPR_CURIOUS,       // Curious: Looking sideways/up, tilted ears, questioning mouth :o
  EXPR_SLEEPY,        // Sleepy: Droopy eyes, slow breathing, floating zzz
  EXPR_SHY_LOVE       // Shy love: Looking down shyly, big blush, fluttering hearts
};

ChibiExpression currentExpr = EXPR_HAPPY_IDLE;
uint32_t exprStartTime = 0;
bool autoCycleExpressions = true;

// ----------------------------------------------------------------------------
// Motion Physics & Dynamics
// ----------------------------------------------------------------------------
float headBobY = 0.0f;
float headSwayX = 0.0f;
float eyeGazeX = 0.0f;
float eyeGazeY = 0.0f;
float targetGazeX = 0.0f;
float targetGazeY = 0.0f;

// Natural Blinking Physics
enum BlinkState { BLINK_OPEN, BLINK_CLOSING, BLINK_CLOSED, BLINK_OPENING };
BlinkState blinkState = BLINK_OPEN;
float eyeOpenRatio = 1.0f; // 1.0 = fully open, 0.0 = closed
uint32_t nextBlinkTime = 0;
uint32_t blinkPhaseTime = 0;

// Cat Ear Twitching Physics
float earTwitchL = 0.0f;
float earTwitchR = 0.0f;
uint32_t nextEarTwitch = 0;

// Floating Particles (Hearts / Sparkles / Zzz)
struct Particle {
  float x;
  float y;
  float vx;
  float vy;
  uint8_t type; // 0=heart, 1=sparkle, 2=star, 3=zzz
  bool active;
};
const int MAX_PARTICLES = 4;
Particle particles[MAX_PARTICLES];
uint32_t nextParticleSpawn = 0;

// ----------------------------------------------------------------------------
// Function Declarations
// ----------------------------------------------------------------------------
void updatePhysics(float timeSec);
void updateBlinking(float timeSec);
void updateParticles(float timeSec);
void drawCatEars(int centerX, int centerY);
void drawChibiEyes(int centerX, int centerY, float timeSec);
void drawBlush(int leftX, int rightX, int y);
void drawChibiMouth(int centerX, int centerY, float timeSec);
void drawParticles();
void handleSerialCommands();

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32-C3 Chibi Cute Face Motion Engine (60 FPS)      ");
  Serial.println("========================================================");

  // Initialize I2C (ESP32-C3 SuperMini: SDA=8, SCL=9)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz high-speed standard

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[WARN] Retrying I2C on alternate pins 9,8...");
    Wire.begin(9, 8);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  }

  // Ensure OLED powers on with comfortable, glare-free contrast
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.dim(false);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(displayContrast);

  display.clearDisplay();
  display.display();

  exprStartTime = millis();
  nextBlinkTime = millis() + 2500;
  nextEarTwitch = millis() + 1800;
  nextParticleSpawn = millis() + 500;

  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }

  Serial.println("[OK] Chibi Cute Face Active!");
  Serial.println("Send commands: happy, wink, joy, curious, sleep, shy, auto, +, -");
}

// ============================================================================
// Main 60 FPS Render Loop
// ============================================================================
void loop() {
  uint32_t nowMicros = micros();
  float timeSec = (float)millis() / 1000.0f;

  handleSerialCommands();

  // Auto-cycle expressions every 7 seconds if enabled
  if (autoCycleExpressions && (millis() - exprStartTime > 7000)) {
    exprStartTime = millis();
    currentExpr = (ChibiExpression)((currentExpr + 1) % 6);
  }

  // 1. Update Physics, Motion, Gaze, and Particles
  updatePhysics(timeSec);
  updateBlinking(timeSec);
  updateParticles(timeSec);

  // 2. Clear Screen
  display.clearDisplay();

  int faceCenterX = 64 + (int)headSwayX;
  int faceCenterY = 32 + (int)headBobY;

  // 3. Render Layers
  drawCatEars(faceCenterX, faceCenterY);
  drawChibiEyes(faceCenterX, faceCenterY, timeSec);
  drawBlush(faceCenterX - 34, faceCenterX + 34, faceCenterY + 12);
  drawChibiMouth(faceCenterX, faceCenterY, timeSec);
  drawParticles();

  // 4. Push frame to OLED
  display.display();

  // 5. Precise 60 FPS pacing
  uint32_t elapsedMicros = micros() - nowMicros;
  if (elapsedMicros < TARGET_FRAME_MICROS) {
    delayMicroseconds(TARGET_FRAME_MICROS - elapsedMicros);
  }
}

// ============================================================================
// Motion & Gaze Physics
// ============================================================================
void updatePhysics(float timeSec) {
  // Breathing & levitation rate varies by expression
  float breathSpeed = (currentExpr == EXPR_SLEEPY) ? 1.5f : 3.0f;
  float bobHeight   = (currentExpr == EXPR_SLEEPY) ? 1.2f : 2.5f;

  headBobY  = sin(timeSec * breathSpeed) * bobHeight;
  headSwayX = sin(timeSec * (breathSpeed * 0.5f)) * 1.8f;

  // Occasional random gaze shifts
  static uint32_t nextGazeShift = 0;
  if (millis() > nextGazeShift) {
    nextGazeShift = millis() + 2000 + (esp_random() % 2500);
    if (currentExpr == EXPR_CURIOUS) {
      targetGazeX = 4.0f + (esp_random() % 3);
      targetGazeY = -3.0f;
    } else if (currentExpr == EXPR_SHY_LOVE) {
      targetGazeX = (esp_random() % 2 == 0) ? -2.0f : 2.0f;
      targetGazeY = 3.5f; // Looking down shyly
    } else if (currentExpr == EXPR_SLEEPY) {
      targetGazeX = 0.0f;
      targetGazeY = 2.0f;
    } else {
      targetGazeX = ((float)(esp_random() % 9) - 4.0f); // -4 to +4
      targetGazeY = ((float)(esp_random() % 5) - 2.0f); // -2 to +2
    }
  }

  // Smooth ease-out towards target gaze (sub-pixel interpolation)
  eyeGazeX += (targetGazeX - eyeGazeX) * 0.12f;
  eyeGazeY += (targetGazeY - eyeGazeY) * 0.12f;

  // Ear twitching trigger
  if (millis() > nextEarTwitch) {
    nextEarTwitch = millis() + 2500 + (esp_random() % 3000);
    if (esp_random() % 2 == 0) {
      earTwitchL = 3.5f;
    } else {
      earTwitchR = 3.5f;
    }
  }
  earTwitchL *= 0.82f; // Smooth spring dampening
  earTwitchR *= 0.82f;
}

// ============================================================================
// Natural Blinking State Machine
// ============================================================================
void updateBlinking(float timeSec) {
  // Joy arch, wink, and sleepy have their own eyelid behaviors
  if (currentExpr == EXPR_JOY_ARCH) {
    eyeOpenRatio = 0.0f; // Curved happy eyes
    return;
  }
  if (currentExpr == EXPR_SLEEPY) {
    eyeOpenRatio = 0.25f + sin(timeSec * 1.5f) * 0.1f; // Droopy sleepy eyes
    return;
  }

  uint32_t now = millis();

  switch (blinkState) {
    case BLINK_OPEN:
      eyeOpenRatio = 1.0f;
      if (now > nextBlinkTime) {
        blinkState = BLINK_CLOSING;
        blinkPhaseTime = now;
      }
      break;

    case BLINK_CLOSING: {
      uint32_t dt = now - blinkPhaseTime;
      if (dt >= 60) {
        eyeOpenRatio = 0.0f;
        blinkState = BLINK_CLOSED;
        blinkPhaseTime = now;
      } else {
        eyeOpenRatio = 1.0f - ((float)dt / 60.0f);
      }
      break;
    }

    case BLINK_CLOSED: {
      uint32_t dt = now - blinkPhaseTime;
      if (dt >= 40) {
        blinkState = BLINK_OPENING;
        blinkPhaseTime = now;
      }
      break;
    }

    case BLINK_OPENING: {
      uint32_t dt = now - blinkPhaseTime;
      if (dt >= 90) {
        eyeOpenRatio = 1.0f;
        blinkState = BLINK_OPEN;
        // Schedule next natural blink (2.5 to 5.0 seconds)
        nextBlinkTime = now + 2500 + (esp_random() % 2500);
      } else {
        eyeOpenRatio = (float)dt / 90.0f;
      }
      break;
    }
  }
}

// ============================================================================
// Floating Particles Physics (Hearts / Sparkles / Zzz)
// ============================================================================
void updateParticles(float timeSec) {
  // Spawn new particle
  if (millis() > nextParticleSpawn) {
    nextParticleSpawn = millis() + 800 + (esp_random() % 800);

    for (int i = 0; i < MAX_PARTICLES; i++) {
      if (!particles[i].active) {
        particles[i].active = true;
        if (currentExpr == EXPR_SLEEPY) {
          particles[i].x = 88.0f + (esp_random() % 16);
          particles[i].y = 40.0f;
          particles[i].vx = 0.25f + ((float)(esp_random() % 20) / 100.0f);
          particles[i].vy = -0.5f;
          particles[i].type = 3; // zzz
        } else if (currentExpr == EXPR_SHY_LOVE || currentExpr == EXPR_JOY_ARCH) {
          particles[i].x = (esp_random() % 2 == 0) ? (esp_random() % 18 + 4) : (106 + esp_random() % 18);
          particles[i].y = 56.0f;
          particles[i].vx = ((float)(esp_random() % 40) - 20.0f) / 100.0f;
          particles[i].vy = -0.8f - ((float)(esp_random() % 40) / 100.0f);
          particles[i].type = (esp_random() % 3 == 0) ? 1 : 0; // heart or sparkle
        } else {
          // Playful idle sparkles
          particles[i].x = (esp_random() % 2 == 0) ? (esp_random() % 18 + 4) : (106 + esp_random() % 18);
          particles[i].y = 56.0f;
          particles[i].vx = ((float)(esp_random() % 30) - 15.0f) / 100.0f;
          particles[i].vy = -0.7f;
          particles[i].type = (esp_random() % 2 == 0) ? 1 : 2; // sparkle or star
        }
        break;
      }
    }
  }

  // Update particle positions
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;

      // Slight sinusoidal wobble
      particles[i].x += sin(timeSec * 4.0f + i) * 0.25f;

      if (particles[i].y < -8 || particles[i].x < -8 || particles[i].x > 136) {
        particles[i].active = false;
      }
    }
  }
}

// ============================================================================
// Render: Cute Wiggling Cat Ears
// ============================================================================
void drawCatEars(int cx, int cy) {
  // Left Ear base & tip
  int leftEarBaseX = cx - 42;
  int leftEarBaseY = cy - 18;
  int leftEarTipX  = cx - 34 - (int)earTwitchL;
  int leftEarTipY  = cy - 31 - (int)(earTwitchL * 1.5f);

  // Right Ear base & tip
  int rightEarBaseX = cx + 42;
  int rightEarBaseY = cy - 18;
  int rightEarTipX  = cx + 34 + (int)earTwitchR;
  int rightEarTipY  = cy - 31 - (int)(earTwitchR * 1.5f);

  // Left Outer Ear Triangle
  display.fillTriangle(leftEarBaseX - 6, leftEarBaseY, leftEarBaseX + 14, leftEarBaseY, leftEarTipX, leftEarTipY, SSD1306_WHITE);
  // Left Inner Ear Shadow (cuteness depth)
  display.fillTriangle(leftEarBaseX - 3, leftEarBaseY - 1, leftEarBaseX + 11, leftEarBaseY - 1, leftEarTipX, leftEarTipY + 3, SSD1306_BLACK);

  // Right Outer Ear Triangle
  display.fillTriangle(rightEarBaseX + 6, rightEarBaseY, rightEarBaseX - 14, rightEarBaseY, rightEarTipX, rightEarTipY, SSD1306_WHITE);
  // Right Inner Ear Shadow
  display.fillTriangle(rightEarBaseX + 3, rightEarBaseY - 1, rightEarBaseX - 11, rightEarBaseY - 1, rightEarTipX, rightEarTipY + 3, SSD1306_BLACK);
}

// ============================================================================
// Render: Giant Sparkly Chibi Anime Eyes
// ============================================================================
void drawSingleSparkleEye(int eyeCX, int eyeCY, int gazeX, int gazeY, float openRatio) {
  const int eyeW = 24;
  const int maxH = 28;
  int curH = (int)(maxH * openRatio);

  if (curH <= 3) {
    // Eye completely shut: Draw cute relaxed curve
    display.drawFastHLine(eyeCX - eyeW / 2, eyeCY, eyeW, SSD1306_WHITE);
    display.drawFastHLine(eyeCX - eyeW / 2 + 2, eyeCY + 1, eyeW - 4, SSD1306_WHITE);
    return;
  }

  int eyeTop = eyeCY - curH / 2;

  // 1. Outer Eye Capsule (Rounded Rectangle)
  display.fillRoundRect(eyeCX - eyeW / 2, eyeTop, eyeW, curH, 7, SSD1306_WHITE);

  // 2. Pupil & Iris (Black inner pupil with gaze tracking)
  int pupilW = 16;
  int pupilH = curH - 4;
  int pupilX = eyeCX - pupilW / 2 + gazeX;
  int pupilY = eyeTop + 2 + gazeY;

  // Boundary clamp for pupil inside sclera
  if (pupilX < eyeCX - eyeW / 2 + 2) pupilX = eyeCX - eyeW / 2 + 2;
  if (pupilX + pupilW > eyeCX + eyeW / 2 - 2) pupilX = eyeCX + eyeW / 2 - 2 - pupilW;
  if (pupilY < eyeTop + 1) pupilY = eyeTop + 1;
  if (pupilY + pupilH > eyeTop + curH - 1) pupilY = eyeTop + curH - 1 - pupilH;

  display.fillRoundRect(pupilX, pupilY, pupilW, pupilH, 5, SSD1306_BLACK);

  // 3. Dual Sparkle Highlights (Catchlights)
  if (openRatio > 0.45f) {
    // Big Upper Catchlight (Sparkle reflection)
    int hlBigX = pupilX + 3;
    int hlBigY = pupilY + 2;
    display.fillCircle(hlBigX, hlBigY, 2, SSD1306_WHITE);
    display.drawPixel(hlBigX + 1, hlBigY + 1, SSD1306_WHITE);

    // Small Secondary Lower Catchlight
    int hlSmallX = pupilX + pupilW - 4;
    int hlSmallY = pupilY + pupilH - 4;
    display.fillCircle(hlSmallX, hlSmallY, 1, SSD1306_WHITE);

    // Subtle bottom crescent iris glow
    display.drawFastHLine(pupilX + 4, pupilY + pupilH - 2, pupilW - 8, SSD1306_WHITE);
  }
}

void drawArchEye(int eyeCX, int eyeCY) {
  // Happy arch eye: ^ shape curved upward
  for (int dx = -11; dx <= 11; dx++) {
    int dy = (dx * dx) / 14;
    display.drawPixel(eyeCX + dx, eyeCY + dy - 4, SSD1306_WHITE);
    display.drawPixel(eyeCX + dx, eyeCY + dy - 3, SSD1306_WHITE); // bold
  }
  // Cute playful eyelash ticks
  display.drawLine(eyeCX - 12, eyeCY + 4, eyeCX - 15, eyeCY + 1, SSD1306_WHITE);
  display.drawLine(eyeCX + 12, eyeCY + 4, eyeCX + 15, eyeCY + 1, SSD1306_WHITE);
}

void drawChibiEyes(int cx, int cy, float timeSec) {
  int leftEyeCX  = cx - 24;
  int rightEyeCX = cx + 24;
  int eyeCY      = cy - 2;

  int gzX = (int)eyeGazeX;
  int gzY = (int)eyeGazeY;

  if (currentExpr == EXPR_JOY_ARCH) {
    // Joy Arch Eyes: ^ ^
    drawArchEye(leftEyeCX, eyeCY);
    drawArchEye(rightEyeCX, eyeCY);
    return;
  }

  if (currentExpr == EXPR_PLAYFUL_WINK) {
    // Left eye winks ^, Right eye sparkles wide
    drawArchEye(leftEyeCX, eyeCY);
    drawSingleSparkleEye(rightEyeCX, eyeCY, gzX, gzY, 1.0f);

    // Draw little star sparkle near winking eye
    display.drawBitmap(leftEyeCX - 14, eyeCY - 10, chibi_tiny_star, 5, 5, SSD1306_WHITE);
    return;
  }

  // Normal / Curious / Sleepy / Shy with dynamic blinking
  drawSingleSparkleEye(leftEyeCX, eyeCY, gzX, gzY, eyeOpenRatio);
  drawSingleSparkleEye(rightEyeCX, eyeCY, gzX, gzY, eyeOpenRatio);
}

// ============================================================================
// Render: Cute Blushing Cheeks (Japanese Anime Hatch Marks)
// ============================================================================
void drawBlush(int leftX, int rightX, int y) {
  // Left cheek 3 cute angled slashes ///
  for (int i = 0; i < 3; i++) {
    int sx = leftX + (i * 4);
    display.drawLine(sx, y + 3, sx + 2, y - 1, SSD1306_WHITE);
    display.drawLine(sx + 1, y + 3, sx + 3, y - 1, SSD1306_WHITE);
  }

  // Right cheek 3 cute angled slashes
  for (int i = 0; i < 3; i++) {
    int sx = rightX + (i * 4);
    display.drawLine(sx, y - 1, sx + 2, y + 3, SSD1306_WHITE);
    display.drawLine(sx + 1, y - 1, sx + 3, y + 3, SSD1306_WHITE);
  }
}

// ============================================================================
// Render: Expressive Chibi Mouths
// ============================================================================
void drawChibiMouth(int cx, int cy, float timeSec) {
  int mouthY = cy + 15;

  switch (currentExpr) {
    case EXPR_HAPPY_IDLE: {
      // Cute :3 Cat Mouth
      // Left curve
      display.drawPixel(cx - 7, mouthY - 1, SSD1306_WHITE);
      display.drawLine(cx - 6, mouthY, cx - 2, mouthY + 2, SSD1306_WHITE);
      display.drawLine(cx - 2, mouthY + 2, cx, mouthY, SSD1306_WHITE);

      // Right curve
      display.drawLine(cx, mouthY, cx + 2, mouthY + 2, SSD1306_WHITE);
      display.drawLine(cx + 2, mouthY + 2, cx + 6, mouthY, SSD1306_WHITE);
      display.drawPixel(cx + 7, mouthY - 1, SSD1306_WHITE);
      break;
    }

    case EXPR_JOY_ARCH: {
      // Big open happy smile :D with tongue
      int smileW = 14;
      display.drawLine(cx - smileW / 2, mouthY - 1, cx + smileW / 2, mouthY - 1, SSD1306_WHITE);
      // Open rounded chin
      display.fillRoundRect(cx - smileW / 2 + 1, mouthY, smileW - 2, 7, 3, SSD1306_WHITE);
      // Cute inner tongue
      display.fillCircle(cx, mouthY + 5, 2, SSD1306_BLACK);
      break;
    }

    case EXPR_PLAYFUL_WINK: {
      // Cheeky tongue poke mouth :P
      display.drawLine(cx - 5, mouthY, cx + 5, mouthY, SSD1306_WHITE);
      // Tongue sticking out to the right
      display.fillRoundRect(cx, mouthY + 1, 6, 6, 2, SSD1306_WHITE);
      display.drawFastHLine(cx + 1, mouthY + 3, 4, SSD1306_BLACK); // Tongue division line
      break;
    }

    case EXPR_CURIOUS: {
      // Small surprised / curious 'o' mouth
      display.drawRoundRect(cx - 3, mouthY, 6, 6, 2, SSD1306_WHITE);
      break;
    }

    case EXPR_SLEEPY: {
      // Tiny relaxed wavy mouth ~
      display.drawLine(cx - 5, mouthY + 1, cx - 2, mouthY, SSD1306_WHITE);
      display.drawLine(cx - 2, mouthY, cx + 2, mouthY + 2, SSD1306_WHITE);
      display.drawLine(cx + 2, mouthY + 2, cx + 5, mouthY + 1, SSD1306_WHITE);
      break;
    }

    case EXPR_SHY_LOVE: {
      // Small shy sweet curve
      display.drawLine(cx - 4, mouthY + 1, cx, mouthY + 3, SSD1306_WHITE);
      display.drawLine(cx, mouthY + 3, cx + 4, mouthY + 1, SSD1306_WHITE);
      break;
    }
  }
}

// ============================================================================
// Render: Floating Particles
// ============================================================================
void drawParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      int px = (int)particles[i].x;
      int py = (int)particles[i].y;

      switch (particles[i].type) {
        case 0: // Heart
          display.drawBitmap(px, py, chibi_heart, 8, 8, SSD1306_WHITE);
          break;
        case 1: // Sparkle
          display.drawBitmap(px, py, chibi_sparkle, 7, 7, SSD1306_WHITE);
          break;
        case 2: // Tiny Star
          display.drawBitmap(px, py, chibi_tiny_star, 5, 5, SSD1306_WHITE);
          break;
        case 3: // Zzz
          display.drawBitmap(px, py, chibi_zzz, 5, 6, SSD1306_WHITE);
          break;
      }
    }
  }
}

// ============================================================================
// Interactive Serial Commands
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.peek();
    if (c == '+' || c == '=') {
      Serial.read();
      if (displayContrast <= 235) displayContrast += 20;
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(displayContrast);
      Serial.printf("[BRIGHTNESS] Contrast: 0x%02X\n", displayContrast);
      continue;
    } else if (c == '-' || c == '_') {
      Serial.read();
      if (displayContrast >= 30) displayContrast -= 20;
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(displayContrast);
      Serial.printf("[BRIGHTNESS] Contrast: 0x%02X\n", displayContrast);
      continue;
    } else if (c == ' ' || c == 'm' || c == 'M') {
      Serial.read();
      currentExpr = (ChibiExpression)((currentExpr + 1) % 6);
      autoCycleExpressions = false;
      Serial.printf("[EXPR] Manual Switch to Expr: %d\n", currentExpr);
      continue;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toLowerCase();
    if (line.length() == 0) continue;

    if (line == "happy") {
      currentExpr = EXPR_HAPPY_IDLE;
      autoCycleExpressions = false;
    } else if (line == "joy") {
      currentExpr = EXPR_JOY_ARCH;
      autoCycleExpressions = false;
    } else if (line == "wink") {
      currentExpr = EXPR_PLAYFUL_WINK;
      autoCycleExpressions = false;
    } else if (line == "curious") {
      currentExpr = EXPR_CURIOUS;
      autoCycleExpressions = false;
    } else if (line == "sleep") {
      currentExpr = EXPR_SLEEPY;
      autoCycleExpressions = false;
    } else if (line == "shy" || line == "love") {
      currentExpr = EXPR_SHY_LOVE;
      autoCycleExpressions = false;
    } else if (line == "auto") {
      autoCycleExpressions = !autoCycleExpressions;
      Serial.printf("[AUTO] Expression auto-cycle: %s\n", autoCycleExpressions ? "ON" : "OFF");
    }
    exprStartTime = millis();
    Serial.printf("[CHIBI] Expression set to: %s\n", line.c_str());
  }
}
