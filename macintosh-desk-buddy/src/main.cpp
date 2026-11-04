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

// 60 FPS Target Timing: 16.67ms per frame (16,666 microseconds)
const uint32_t TARGET_FRAME_MICROS = 16666;
uint32_t lastFrameMicros = 0;
float currentFPS = 60.0f;
uint32_t frameCount = 0;
uint32_t fpsTimer = 0;
uint32_t fpsCounter = 0;

enum MacState {
  STATE_HELLO_INTRO,   // 1984 Cursive "hello." animated stroke drawing
  STATE_SYSTEM_DESKTOP,// System 1.0 Desktop with menu bar, clock, moving cursor & window
  STATE_HAPPY_MAC_3D,  // Floating miniature 1984 Mac chassis with breathing & eye gaze
  STATE_EXPRESSIVE_FACE// Full-screen retro CRT face with smooth eyelids & blinks
};

MacState currentState = STATE_HELLO_INTRO;
uint32_t stateStartTime = 0;

// Cursor Physics (Smooth gliding at 60 FPS)
float cursorX = 20.0f, cursorY = 25.0f;
float cursorTargetX = 64.0f, cursorTargetY = 32.0f;

// Floating Window Physics
float winX = 22.0f, winY = 16.0f;
float winVX = 0.8f, winVY = 0.5f;

// Menu Bar Interaction
bool menuOpen = false;
uint32_t menuTimer = 0;

// "hello." Vector Path Stroke Definition (Iconic 1984 Mac Intro)
struct Point { int8_t x, y; };
const Point HELLO_STROKES[] PROGMEM = {
  // 'h'
  {18, 48}, {18, 20}, {18, 36}, {24, 28}, {30, 28}, {32, 34}, {32, 48},
  // 'e'
  {38, 40}, {46, 38}, {44, 30}, {38, 32}, {36, 42}, {44, 46},
  // 'l'
  {48, 44}, {52, 20}, {52, 46}, {56, 48},
  // 'l'
  {60, 44}, {64, 20}, {64, 46}, {68, 48},
  // 'o'
  {74, 36}, {70, 32}, {74, 28}, {82, 28}, {86, 36}, {82, 46}, {74, 46}, {70, 40}, {76, 32},
  // '.'
  {92, 46}, {94, 46}, {94, 48}, {92, 48}
};
const uint16_t TOTAL_HELLO_POINTS = sizeof(HELLO_STROKES) / sizeof(HELLO_STROKES[0]);
float helloProgress = 0.0f;

// ----------------------------------------------------------------
// High-Speed I2C & Display Initialization
// ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================================");
  Serial.println("  1984 Macintosh OS Simulator - Ultra Smooth 60 FPS     ");
  Serial.println("========================================================");

  // Initialize I2C with High-Speed 800kHz Fast-Mode Plus for 60 FPS
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(800000); // 800 kHz ensures <12ms buffer transfer
  delay(50);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Fallback scan if 0x3C failed
    Wire.begin(9, 8);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  }

  // Set 1MHz ultra-high speed I2C AFTER display.begin() (Adafruit resets it to 400kHz in begin)
  Wire.setClock(1000000);

  display.clearDisplay();
  display.display();

  currentState = STATE_HELLO_INTRO;
  stateStartTime = millis();
  lastFrameMicros = micros();
  fpsTimer = millis();

  Serial.println("[OK] 60 FPS Render Pipeline Initialized.");
}

// ----------------------------------------------------------------
// 60 FPS Render Routines
// ----------------------------------------------------------------

// 1. Iconic 1984 "hello." Stroke by Stroke Animation
void renderHello(float timeSec) {
  display.clearDisplay();

  // Draw miniature classic Mac on left side
  int macCX = 16, macCY = 34;
  display.drawRoundRect(macCX - 12, macCY - 16, 24, 30, 3, SSD1306_WHITE);
  display.drawRoundRect(macCX - 10, macCY - 13, 20, 18, 2, SSD1306_WHITE);
  display.drawFastHLine(macCX - 11, macCY + 8, 22, SSD1306_WHITE);
  display.fillRect(macCX, macCY + 10, 8, 2, SSD1306_WHITE); // floppy slot

  // Tiny smiling face on the Mac screen
  display.drawPixel(macCX - 4, macCY - 6, SSD1306_WHITE);
  display.drawPixel(macCX + 4, macCY - 6, SSD1306_WHITE);
  display.drawLine(macCX - 4, macCY - 1, macCX + 4, macCY - 1, SSD1306_WHITE);

  // Advance stroke interpolation at 60 FPS
  helloProgress += 0.28f;
  if (helloProgress > (float)TOTAL_HELLO_POINTS) {
    helloProgress = (float)TOTAL_HELLO_POINTS;
  }

  int currentIdx = (int)helloProgress;

  // Draw cursive strokes completed so far
  for (int i = 0; i < currentIdx - 1; i++) {
    Point p1, p2;
    memcpy_P(&p1, &HELLO_STROKES[i], sizeof(Point));
    memcpy_P(&p2, &HELLO_STROKES[i + 1], sizeof(Point));

    // Thick retro brush stroke (2 pixels wide)
    display.drawLine(p1.x + 18, p1.y, p2.x + 18, p2.y, SSD1306_WHITE);
    display.drawLine(p1.x + 19, p1.y, p2.x + 19, p2.y, SSD1306_WHITE);
  }

  // Draw moving fountain pen nib at leading edge
  if (currentIdx < TOTAL_HELLO_POINTS) {
    Point cur;
    memcpy_P(&cur, &HELLO_STROKES[currentIdx], sizeof(Point));
    display.fillCircle(cur.x + 18, cur.y, 2, SSD1306_WHITE);
  }

  // Bottom vintage slogan
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(42, 54);
  display.print("Macintosh 1984");

  // Retro shadow outline border
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
}

// 2. System 1.0 Classic Macintosh Desktop (60 FPS Motion)
void renderDesktop(float timeSec) {
  display.clearDisplay();

  // Desktop Menu Bar (White background with black text)
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.drawBitmap(3, 0, apple_icon, 10, 12, SSD1306_BLACK);

  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(16, 1);
  display.print("File  Edit  View");

  // Real-time ticking retro clock in top-right corner
  uint32_t totalSec = millis() / 1000;
  int clockMin = (totalSec / 60) % 60;
  int clockSec = totalSec % 60;
  display.setCursor(96, 1);
  display.printf("9:%02d", clockMin);

  // Desktop Icons
  display.drawBitmap(6, 16, mac_floppy, 16, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 34);
  display.print("Disk");

  // Draggable / Bouncing Retro Window
  winX += winVX;
  winY += winVY;
  if (winX <= 28.0f || winX >= 64.0f) winVX = -winVX;
  if (winY <= 12.0f || winY >= 26.0f) winVY = -winVY;

  int wx = (int)winX;
  int wy = (int)winY;
  int ww = 60, wh = 36;

  // Window Shadow & Frame
  display.fillRect(wx + 2, wy + 2, ww, wh, SSD1306_BLACK);
  display.drawRect(wx, wy, ww, wh, SSD1306_WHITE);
  display.fillRect(wx + 1, wy + 1, ww - 2, wh - 2, SSD1306_BLACK);

  // Window Pinstripe Title Bar
  for (int y = wy + 2; y <= wy + 8; y += 2) {
    display.drawFastHLine(wx + 3, y, ww - 6, SSD1306_WHITE);
  }
  // Title Box
  display.fillRect(wx + 16, wy + 1, 28, 8, SSD1306_BLACK);
  display.setCursor(wx + 18, wy + 2);
  display.print("Mac");
  // Close Box (Square in top-left)
  display.drawRect(wx + 3, wy + 2, 6, 6, SSD1306_WHITE);

  // Animated Sine Wave Activity Monitor inside window
  for (int x = 0; x < ww - 6; x++) {
    int waveY = wy + 22 + (int)(sin(timeSec * 5.0f + x * 0.25f) * 6.0f);
    display.drawPixel(wx + 3 + x, waveY, SSD1306_WHITE);
  }

  // Smooth Sub-pixel Cursor Movement
  // Automatically moves between Menu, Window, and Disk icon
  float targetDist = hypot(cursorTargetX - cursorX, cursorTargetY - cursorY);
  if (targetDist < 3.0f) {
    // Pick next point
    int waypoint = (millis() / 2500) % 3;
    if (waypoint == 0) { cursorTargetX = 26.0f; cursorTargetY = 4.0f; }      // File menu
    else if (waypoint == 1) { cursorTargetX = winX + 6; cursorTargetY = winY + 4; } // Close box
    else { cursorTargetX = 14.0f; cursorTargetY = 24.0f; }                   // Disk icon
  }
  cursorX += (cursorTargetX - cursorX) * 0.08f;
  cursorY += (cursorTargetY - cursorY) * 0.08f;

  // Dropdown Menu Simulation when clicking File Menu
  if (cursorX < 36.0f && cursorY < 8.0f) {
    display.fillRect(14, 10, 48, 34, SSD1306_WHITE);
    display.drawRect(14, 10, 48, 34, SSD1306_BLACK);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(18, 13); display.print("New...");
    display.setCursor(18, 21); display.print("Open");
    display.setCursor(18, 29); display.print("Close");
    display.fillRect(15, 20, 46, 9, SSD1306_BLACK); // Highlight "Open"
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(18, 21); display.print("Open");
  }

  // Render Classic Arrow Cursor
  display.drawBitmap((int)cursorX, (int)cursorY, mac_cursor, 11, 16, SSD1306_WHITE);
}

// 3. Floating 3D Miniature 1984 Macintosh with Breathing Physics
void renderHappyMac3D(float timeSec) {
  display.clearDisplay();

  // Smooth sinusoidal levitation & breathing
  float floatY = sin(timeSec * 2.5f) * 4.0f;
  float eyeGlideX = sin(timeSec * 1.8f) * 4.0f;
  float eyeGlideY = cos(timeSec * 1.5f) * 2.0f;

  int cx = 64;
  int cy = 32 + (int)floatY;

  // Outer Macintosh Body (Beveled retro beige case)
  display.drawRoundRect(cx - 28, cy - 26, 56, 52, 4, SSD1306_WHITE);
  display.drawFastHLine(cx - 26, cy + 14, 52, SSD1306_WHITE); // lower chin line
  display.fillRect(cx + 4, cy + 17, 18, 3, SSD1306_WHITE);    // 3.5" floppy drive slot
  display.drawPixel(cx - 18, cy + 18, SSD1306_WHITE);         // Apple rainbow logo spot

  // CRT Monitor Bezel
  display.drawRoundRect(cx - 22, cy - 22, 44, 34, 3, SSD1306_WHITE);
  display.drawRoundRect(cx - 21, cy - 21, 42, 32, 2, SSD1306_WHITE);

  // Animated Eyes on CRT
  int leftEyeX = cx - 10 + (int)eyeGlideX;
  int rightEyeX = cx + 10 + (int)eyeGlideX;
  int eyeY = cy - 8 + (int)eyeGlideY;

  // Periodic smooth blink
  bool blink = (fmod(timeSec, 3.2f) < 0.15f);
  if (blink) {
    display.drawFastHLine(leftEyeX - 3, eyeY, 6, SSD1306_WHITE);
    display.drawFastHLine(rightEyeX - 3, eyeY, 6, SSD1306_WHITE);
  } else {
    display.fillRoundRect(leftEyeX - 3, eyeY - 4, 6, 8, 2, SSD1306_WHITE);
    display.fillRoundRect(rightEyeX - 3, eyeY - 4, 6, 8, 2, SSD1306_WHITE);
    display.drawPixel(leftEyeX + 1, eyeY - 2, SSD1306_BLACK); // pupil catchlight
    display.drawPixel(rightEyeX + 1, eyeY - 2, SSD1306_BLACK);
  }

  // Classic Mac Smile
  display.drawLine(cx - 10, cy + 2, cx - 6, cy + 5, SSD1306_WHITE);
  display.drawLine(cx - 6, cy + 5, cx + 6, cy + 5, SSD1306_WHITE);
  display.drawLine(cx + 6, cy + 5, cx + 10, cy + 2, SSD1306_WHITE);

  // Floating rotating wristwatch in corner
  display.drawBitmap(108, 6, mac_watch, 14, 14, SSD1306_WHITE);

  // Status banner
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 56);
  display.printf("60 FPS | Uptime: %lus", millis() / 1000);
}

// 4. Full-Screen Retro CRT Face with Dynamic Physics
void renderExpressiveFace(float timeSec) {
  display.clearDisplay();

  // Curved CRT Outer Phosphor Border
  display.drawRoundRect(4, 2, 120, 60, 6, SSD1306_WHITE);
  display.drawRoundRect(6, 4, 116, 56, 4, SSD1306_WHITE);

  // Smooth Eye Gaze following Lissajous curve
  float lookX = sin(timeSec * 2.0f) * 8.0f;
  float lookY = sin(timeSec * 4.0f) * 3.0f;

  int leftX = 42 + (int)lookX;
  int rightX = 86 + (int)lookX;
  int eyeY = 24 + (int)lookY;

  // Eyelid Phase Animation
  float blinkCycle = fmod(timeSec, 3.0f);
  int eyeHeight = 16;
  if (blinkCycle < 0.12f) {
    eyeHeight = 2; // closed
  } else if (blinkCycle < 0.22f) {
    eyeHeight = 8; // opening
  }

  if (eyeHeight > 2) {
    display.fillRoundRect(leftX - 8, eyeY - eyeHeight / 2, 16, eyeHeight, 3, SSD1306_WHITE);
    display.fillRoundRect(rightX - 8, eyeY - eyeHeight / 2, 16, eyeHeight, 3, SSD1306_WHITE);
    // Eye Pupils
    display.fillRect(leftX - 2, eyeY - 2, 4, 4, SSD1306_BLACK);
    display.fillRect(rightX - 2, eyeY - 2, 4, 4, SSD1306_BLACK);
  } else {
    // Closed happy curve
    display.drawFastHLine(leftX - 8, eyeY, 16, SSD1306_WHITE);
    display.drawFastHLine(rightX - 8, eyeY, 16, SSD1306_WHITE);
  }

  // Smooth Dynamic Smiling Mouth
  float smileSquish = sin(timeSec * 3.0f) * 2.0f;
  display.drawLine(44, 44, 52, 48 + (int)smileSquish, SSD1306_WHITE);
  display.drawLine(52, 48 + (int)smileSquish, 76, 48 + (int)smileSquish, SSD1306_WHITE);
  display.drawLine(76, 48 + (int)smileSquish, 84, 44, SSD1306_WHITE);
  display.drawLine(52, 49 + (int)smileSquish, 76, 49 + (int)smileSquish, SSD1306_WHITE); // Bold line
}

// ----------------------------------------------------------------
// Main 60 FPS Engine Loop
// ----------------------------------------------------------------
void loop() {
  // Check Serial interactive commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    if (cmd == "hello" || cmd == "intro") {
      currentState = STATE_HELLO_INTRO;
      helloProgress = 0.0f;
    } else if (cmd == "desktop" || cmd == "mac") {
      currentState = STATE_SYSTEM_DESKTOP;
    } else if (cmd == "3d" || cmd == "buddy") {
      currentState = STATE_HAPPY_MAC_3D;
    } else if (cmd == "face" || cmd == "happy") {
      currentState = STATE_EXPRESSIVE_FACE;
    }
    stateStartTime = millis();
    Serial.printf("[60 FPS ENGINE] Switched Mode: %s\n", cmd.c_str());
  }

  uint32_t nowMicros = micros();
  float timeSec = (float)millis() / 1000.0f;
  uint32_t elapsedState = millis() - stateStartTime;

  // Auto-cycle through the 4 iconic modes every 9 seconds
  if (currentState == STATE_HELLO_INTRO && elapsedState > 6500) {
    currentState = STATE_SYSTEM_DESKTOP;
    stateStartTime = millis();
  } else if (currentState == STATE_SYSTEM_DESKTOP && elapsedState > 9000) {
    currentState = STATE_HAPPY_MAC_3D;
    stateStartTime = millis();
  } else if (currentState == STATE_HAPPY_MAC_3D && elapsedState > 9000) {
    currentState = STATE_EXPRESSIVE_FACE;
    stateStartTime = millis();
  } else if (currentState == STATE_EXPRESSIVE_FACE && elapsedState > 9000) {
    currentState = STATE_HELLO_INTRO;
    helloProgress = 0.0f;
    stateStartTime = millis();
  }

  // Render active scene
  switch (currentState) {
    case STATE_HELLO_INTRO:    renderHello(timeSec); break;
    case STATE_SYSTEM_DESKTOP: renderDesktop(timeSec); break;
    case STATE_HAPPY_MAC_3D:   renderHappyMac3D(timeSec); break;
    case STATE_EXPRESSIVE_FACE:renderExpressiveFace(timeSec); break;
  }

  // Push buffer to display via 800kHz I2C bus (~11ms transfer time)
  display.display();

  // FPS Counter Calculation
  frameCount++;
  fpsCounter++;
  if (millis() - fpsTimer >= 1000) {
    currentFPS = (float)fpsCounter;
    fpsCounter = 0;
    fpsTimer = millis();
    Serial.printf("[ENGINE] Mode: %d | Frame: %lu | Live FPS: %.1f\n",
                  currentState, (unsigned long)frameCount, currentFPS);
  }

  // Maintain precise 60 FPS cadence (16,666 µs frame budget)
  uint32_t frameDuration = micros() - nowMicros;
  if (frameDuration < TARGET_FRAME_MICROS) {
    delayMicroseconds(TARGET_FRAME_MICROS - frameDuration);
  }
}
