#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pikachu_bitmaps.h"

// ============================================================================
// Pikachu Laying on Screen — Cozy Sleeping Desktop Companion
// For ESP32-C3 SuperMini + 0.96" OLED (SSD1306, 128x64)
// ============================================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Contrast & Brightness
uint8_t displayContrast = 0x3F; // Soft, comfortable brightness

// ----------------------------------------------------------------------------
// Animation State Machine
// ----------------------------------------------------------------------------
enum PikaState {
  STATE_SLEEPING,  // Deep, peaceful sleep with breathing and floating Zzz
  STATE_PEEKING,   // Curious one-eye or both eyes open, looking at user
  STATE_DREAMING,  // Dreaming of thunder: cheek electric sparks!
  STATE_PET_HAPPY  // Just petted: wide awake smile with floating hearts
};

PikaState currentState = STATE_SLEEPING;
uint32_t stateStartTime = 0;
uint32_t nextActionTime = 0;

// Tail physics
bool tailFlick = false;
uint32_t tailFlickEnd = 0;
uint32_t nextTailFlick = 0;

// Ear twitch physics
int earTwitchY = 0;
uint32_t nextEarTwitch = 0;

// Cheek Electric Sparks
bool cheekSparksActive = false;
uint32_t cheekSparksEnd = 0;

// Floating "Zzz" Sleep Particles
struct SleepZ {
  float x;
  float y;
  float vx;
  float vy;
  uint8_t size; // 0=small, 1=mid, 2=big
  bool active;
};
const int MAX_Z = 4;
SleepZ sleepZs[MAX_Z];
uint32_t nextZSpawn = 0;

// Night Sky Stars (ambient twinkling)
struct Star {
  int8_t x, y;
  uint8_t phase;
};
const int NUM_STARS = 6;
Star stars[NUM_STARS] = {
  {8, 6, 0}, {26, 12, 2}, {45, 4, 4},
  {85, 8, 1}, {108, 14, 3}, {120, 5, 5}
};

// ----------------------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------------------
void updateBreathing(float timeSec);
void updateParticles(float timeSec);
void updatePikaState(float timeSec);
void renderNightSky(float timeSec);
void renderPikachu(float timeSec);
void renderCheekSparks(int pikaX, int pikaY);
void renderParticles();
void handleSerialCommands();

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n========================================================");
  Serial.println("  Pikachu Laying On Screen — ESP32-C3 Cozy Companion    ");
  Serial.println("========================================================");

  // Initialize I2C (ESP32-C3 SuperMini: SDA=8, SCL=9)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[WARN] Retrying I2C on alternate pins 9,8...");
    Wire.begin(9, 8);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  }

  // Explicitly power ON OLED with comfortable contrast
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.dim(false);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(displayContrast);

  display.clearDisplay();
  display.display();

  stateStartTime = millis();
  nextActionTime = millis() + 8000;
  nextTailFlick = millis() + 3000;
  nextEarTwitch = millis() + 4500;
  nextZSpawn = millis() + 1000;

  for (int i = 0; i < MAX_Z; i++) {
    sleepZs[i].active = false;
  }

  Serial.println("[OK] Pikachu is resting cozily on screen!");
  Serial.println("Commands: pet, sleep, spark, tail, wake, +, -");
}

// ============================================================================
// Main Animation Loop (~33 FPS)
// ============================================================================
void loop() {
  float timeSec = (float)millis() / 1000.0f;

  handleSerialCommands();
  updatePikaState(timeSec);
  updateParticles(timeSec);

  // Clear display buffer
  display.clearDisplay();

  // 1. Ambient Background (Twinkling stars & cozy moon)
  renderNightSky(timeSec);

  // 2. Ground Rug Line / Floor
  display.drawFastHLine(0, 62, 128, SSD1306_WHITE);
  display.drawBitmap(6, 58, grass_tuft, 8, 5, SSD1306_WHITE);
  display.drawBitmap(114, 58, grass_tuft, 8, 5, SSD1306_WHITE);

  // 3. Laying Pikachu
  renderPikachu(timeSec);

  // 4. Floating Zzz & Hearts
  renderParticles();

  // Push frame to OLED
  display.display();

  delay(30); // ~33 FPS silky smooth
}

// ============================================================================
// State & Life Cycle Updates
// ============================================================================
void updatePikaState(float timeSec) {
  uint32_t now = millis();

  // Tail flick timer
  if (now > nextTailFlick) {
    tailFlick = true;
    tailFlickEnd = now + 400 + (esp_random() % 300);
    nextTailFlick = now + 2500 + (esp_random() % 3500);
  }
  if (tailFlick && now > tailFlickEnd) {
    tailFlick = false;
  }

  // Periodic behavioral transitions
  if (now > nextActionTime) {
    if (currentState == STATE_SLEEPING) {
      // 30% chance to peek, 20% chance to dream sparks, 50% keep sleeping
      int r = esp_random() % 10;
      if (r < 3) {
        currentState = STATE_PEEKING;
        nextActionTime = now + 3500;
      } else if (r < 5) {
        currentState = STATE_DREAMING;
        cheekSparksActive = true;
        cheekSparksEnd = now + 2500;
        nextActionTime = now + 4000;
      } else {
        nextActionTime = now + 6000 + (esp_random() % 4000);
      }
    } else {
      // Return to peaceful sleep
      currentState = STATE_SLEEPING;
      cheekSparksActive = false;
      nextActionTime = now + 6000 + (esp_random() % 4000);
    }
  }

  if (cheekSparksActive && now > cheekSparksEnd) {
    cheekSparksActive = false;
  }
}

// ============================================================================
// Sleep Particles (Zzz) & Heart Updates
// ============================================================================
void updateParticles(float timeSec) {
  uint32_t now = millis();

  // Spawn new Zzz from head area
  if (currentState == STATE_SLEEPING || currentState == STATE_DREAMING) {
    if (now > nextZSpawn) {
      nextZSpawn = now + 1200 + (esp_random() % 800);
      for (int i = 0; i < MAX_Z; i++) {
        if (!sleepZs[i].active) {
          sleepZs[i].active = true;
          // Spawn near Pikachu's nose / mouth (x ~ 80, y ~ 40)
          sleepZs[i].x = 76.0f + (esp_random() % 6);
          sleepZs[i].y = 40.0f;
          sleepZs[i].vx = -0.35f - ((float)(esp_random() % 20) / 100.0f); // drift left
          sleepZs[i].vy = -0.6f - ((float)(esp_random() % 30) / 100.0f);  // drift up
          sleepZs[i].size = 0; // starts small
          break;
        }
      }
    }
  }

  // Update existing particles
  for (int i = 0; i < MAX_Z; i++) {
    if (sleepZs[i].active) {
      sleepZs[i].x += sleepZs[i].vx;
      sleepZs[i].y += sleepZs[i].vy;

      // Slight sinusoidal wobble as it floats
      sleepZs[i].x += sin(timeSec * 3.0f + i) * 0.3f;

      // Grow size as it ascends
      if (sleepZs[i].y < 28.0f) sleepZs[i].size = 1;
      if (sleepZs[i].y < 16.0f) sleepZs[i].size = 2;

      // Despawn when out of bounds
      if (sleepZs[i].y < -4.0f || sleepZs[i].x < 0.0f) {
        sleepZs[i].active = false;
      }
    }
  }
}

// ============================================================================
// Render: Ambient Night Sky (Crescent Moon & Twinkling Stars)
// ============================================================================
void renderNightSky(float timeSec) {
  // Little Crescent Moon in top-left corner
  display.fillCircle(10, 8, 5, SSD1306_WHITE);
  display.fillCircle(12, 7, 4, SSD1306_BLACK);

  // Status caption
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 2);
  if (currentState == STATE_SLEEPING) {
    display.print(F("PIKA SNOOZING..."));
  } else if (currentState == STATE_PEEKING) {
    display.print(F("PIKA PEEKS! ^_^"));
  } else if (currentState == STATE_DREAMING) {
    display.print(F("ELECTRIC DREAM ⚡"));
  } else if (currentState == STATE_PET_HAPPY) {
    display.print(F("PIKACHU <3 <3"));
  }

  // Twinkling stars
  for (int i = 0; i < NUM_STARS; i++) {
    float flicker = sin(timeSec * 4.0f + stars[i].phase);
    if (flicker > 0.2f) {
      display.drawPixel(stars[i].x, stars[i].y, SSD1306_WHITE);
      if (flicker > 0.8f) {
        // Cross twinkle
        display.drawPixel(stars[i].x - 1, stars[i].y, SSD1306_WHITE);
        display.drawPixel(stars[i].x + 1, stars[i].y, SSD1306_WHITE);
        display.drawPixel(stars[i].x, stars[i].y - 1, SSD1306_WHITE);
        display.drawPixel(stars[i].x, stars[i].y + 1, SSD1306_WHITE);
      }
    }
  }
}

// ============================================================================
// Render: Laying Pikachu Sprite & Animation
// ============================================================================
void renderPikachu(float timeSec) {
  // Breathing rhythm: 3.5s cycle
  float breath = sin(timeSec * 1.8f);
  bool breathUp = (breath > 0.2f);

  // Position: Centered at bottom (x=20, y=24)
  int pikaX = 20;
  int pikaY = 24;

  const unsigned char* pikaFrame;

  if (currentState == STATE_PEEKING || currentState == STATE_PET_HAPPY) {
    pikaFrame = pika_awake;
  } else if (tailFlick) {
    pikaFrame = pika_tail_up;
  } else if (breathUp) {
    pikaFrame = pika_sleep_2; // Inhaling: torso lifts 2px
  } else {
    pikaFrame = pika_sleep_1; // Exhaling: torso relaxed
  }

  display.drawBitmap(pikaX, pikaY, pikaFrame, PIKA_WIDTH, PIKA_HEIGHT, SSD1306_WHITE);

  // Render Cheek Sparks if dreaming
  if (cheekSparksActive || currentState == STATE_DREAMING) {
    renderCheekSparks(pikaX, pikaY);
  }

  // If petted, draw floating hearts
  if (currentState == STATE_PET_HAPPY) {
    int heartY = 16 + (int)(sin(timeSec * 5.0f) * 3.0f);
    display.drawBitmap(102, heartY, pika_heart, 8, 8, SSD1306_WHITE);
    display.drawBitmap(14, heartY + 4, pika_heart, 8, 8, SSD1306_WHITE);
  }
}

// ============================================================================
// Render: Cheek Electric Sparks
// ============================================================================
void renderCheekSparks(int pikaX, int pikaY) {
  // Pikachu's cheeks are around:
  // Left cheek: pikaX + 53, pikaY + 22
  // Right cheek: pikaX + 73, pikaY + 22
  int lx = pikaX + 53;
  int rx = pikaX + 73;
  int cy = pikaY + 20;

  // Flash tiny lightning bolts
  if ((millis() / 120) % 2 == 0) {
    display.drawBitmap(lx - 8, cy - 6, spark_bolt, 7, 10, SSD1306_WHITE);
    display.drawBitmap(rx + 4, cy - 6, spark_bolt, 7, 10, SSD1306_WHITE);
  } else {
    // Radiating spark lines
    display.drawLine(lx - 2, cy, lx - 5, cy - 3, SSD1306_WHITE);
    display.drawLine(lx - 2, cy + 2, lx - 5, cy + 4, SSD1306_WHITE);
    display.drawLine(rx + 4, cy, rx + 7, cy - 3, SSD1306_WHITE);
    display.drawLine(rx + 4, cy + 2, rx + 7, cy + 4, SSD1306_WHITE);
  }
}

// ============================================================================
// Render: Floating Sleep Zzz Particles
// ============================================================================
void renderParticles() {
  for (int i = 0; i < MAX_Z; i++) {
    if (sleepZs[i].active) {
      int px = (int)sleepZs[i].x;
      int py = (int)sleepZs[i].y;

      if (sleepZs[i].size == 0) {
        display.drawBitmap(px, py, sleep_small_z, 4, 4, SSD1306_WHITE);
      } else if (sleepZs[i].size == 1) {
        display.drawBitmap(px, py, sleep_mid_z, 5, 6, SSD1306_WHITE);
      } else {
        display.drawBitmap(px, py, sleep_big_z, 7, 8, SSD1306_WHITE);
      }
    }
  }
}

// ============================================================================
// Interactive Serial Commands (115200 baud)
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
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toLowerCase();
    if (line.length() == 0) continue;

    if (line == "pet" || line == "wake") {
      currentState = STATE_PET_HAPPY;
      tailFlick = true;
      tailFlickEnd = millis() + 2000;
      nextActionTime = millis() + 4500;
      Serial.println("[PIKACHU] *Happy Pika noises* <3 Pikachu woke up happily!");
    } else if (line == "sleep" || line == "nap") {
      currentState = STATE_SLEEPING;
      cheekSparksActive = false;
      nextActionTime = millis() + 8000;
      Serial.println("[PIKACHU] Pikachu curled back down to sleep... Zzz");
    } else if (line == "spark" || line == "thunder") {
      currentState = STATE_DREAMING;
      cheekSparksActive = true;
      cheekSparksEnd = millis() + 3500;
      nextActionTime = millis() + 5000;
      Serial.println("[PIKACHU] ⚡ Sparks crackling from red cheeks! Pika-chuuuu!");
    } else if (line == "tail") {
      tailFlick = true;
      tailFlickEnd = millis() + 1500;
      Serial.println("[PIKACHU] Lightning tail wagging!");
    }
  }
}

