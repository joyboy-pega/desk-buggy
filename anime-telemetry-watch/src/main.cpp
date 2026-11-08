#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "anime_bitmaps.h"
#include "hud_icons.h"

// ============================================================================
// Pixel Anime Telemetry Watch & GitHub Stats HUD
// For ESP32-WROOM-32 + 1.8" ST7735 Color TFT (160x128, Rotation 3)
// ============================================================================

// ----------------------------------------------------------------------------
// Pinout for ESP32-WROOM-32 (Standard 30-pin)
// ----------------------------------------------------------------------------
#define TFT_CS    5   // Chip Select (GPIO 5)
#define TFT_DC    2   // Data/Command A0 (GPIO 2)
#define TFT_RST   4   // Hardware Reset (GPIO 4)
#define TFT_MOSI 23   // VSPI MOSI / SDA (GPIO 23)
#define TFT_SCLK 18   // VSPI SCLK / SCK (GPIO 18)
#define TFT_BL   32   // Backlight PWM pin

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 160x128 Double-Buffered Canvas for ZERO-Flicker 60 FPS Rendering
GFXcanvas16* canvas = nullptr;
const int SCREEN_W = 160;
const int SCREEN_H = 128;

// ----------------------------------------------------------------------------
// 16-bit RGB565 Cyberpunk / Anime Color Palette
// ----------------------------------------------------------------------------
#define COLOR_BG          0x0821 // Deep Midnight Obsidian
#define COLOR_PANEL_BG    0x10A3 // Slate Cyber Navy
#define COLOR_PANEL_EDGE  0x2966 // Subtle Panel Border
#define COLOR_CYAN_ACCENT 0x3FFA // Neon Cyber Cyan
#define COLOR_PINK_ACCENT 0xF9B6 // Neon Anime Pink
#define COLOR_GOLD_CLOCK  0xFDE0 // Vivid Amber Gold
#define COLOR_GREEN_STAT  0x2FE4 // Bright Emerald Green
#define COLOR_TEXT_WHITE  0xFFFF // Crisp White
#define COLOR_TEXT_MUTED  0x94B2 // Slate Silver
#define COLOR_RED_PIN     0xF986 // Crimson Red

uint8_t displayBrightness = 255; // 0 to 255
uint8_t screenRotation = 3;      // 180° rotated landscape

// ----------------------------------------------------------------------------
// Wi-Fi Credentials & Geolocation (Pune, India)
// ----------------------------------------------------------------------------
const char* wifiSSID = "Arigatoo1";
const char* wifiPassword = "12345678";
volatile bool wifiConnected = false;
const long gmtOffsetSec = 19800; // IST UTC+5:30 (Pune, India)
const int daylightOffsetSec = 0;
const char* LOCATION_NAME = "PUNE, IN";

// ----------------------------------------------------------------------------
// Clock & Telemetry Variables
// ----------------------------------------------------------------------------
int curHours = 21;
int curMinutes = 52;
int curSeconds = 0;
int curDay = 8;
int curMonth = 11;
int curYear = 2026;
int curDayOfWeek = 0; // Sun=0

float currentCoreTemp = 38.2f;
unsigned long lastTickMillis = 0;
unsigned long bootMillis = 0;

// GitHub Telemetry
const char* GITHUB_REPO = "joyboy-pega/desk-buggy";
volatile int totalGithubCommits = 36; // Baseline count
volatile bool githubFetchDone = false;
unsigned long lastGithubFetch = 0;

// Anime Companion Animation State
int animeAnimFrame = 0; // 0=normal, 1=breathe, 2=blink
bool isBlinking = false;
uint32_t nextBlinkTime = 0;
uint32_t blinkEndTime = 0;

const char* DAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// ----------------------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------------------
void advanceClock();
void updateAnimeAnimation(float timeSec);
void updateTemperature();
void renderHUD();
void handleSerialCommands();
void backgroundNetworkWorker(void* pvParameters);
uint16_t scaleBrightness(uint16_t color, uint8_t brightness);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  bootMillis = millis();
  delay(500);

  Serial.println("\n\n========================================================");
  Serial.println("  Pixel Anime Telemetry Watch & GitHub HUD Station      ");
  Serial.println("========================================================");

  // Backlight PWM
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, displayBrightness);

  // High-Speed SPI (27 MHz)
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(27000000);

  // Initialize ST7735 (1.8" BlackTab, 180° Rotated: rotation=3)
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(screenRotation);
  tft.fillScreen(COLOR_BG);

  // Allocate 160x128 double buffer canvas (40KB)
  canvas = new GFXcanvas16(SCREEN_W, SCREEN_H);
  if (canvas) {
    Serial.println("[OK] 160x128 Double-Buffered Canvas Active (Zero Flicker!)");
  }

  nextBlinkTime = millis() + 3000;

  // Launch Background Network Task (Wi-Fi + NTP + GitHub Fetch)
  xTaskCreate(
    backgroundNetworkWorker,
    "NetWorker",
    8192,
    NULL,
    1,
    NULL
  );

  Serial.println("[OK] Telemetry Watch Running!");
  Serial.println("Hotkeys: '+' / '-' for brightness, 'flip' to rotate 180");
}

// ============================================================================
// Main 30 FPS Render Loop
// ============================================================================
void loop() {
  float timeSec = (float)millis() / 1000.0f;

  handleSerialCommands();

  // 1-second clock tick & internal sensors
  if (millis() - lastTickMillis >= 1000) {
    lastTickMillis = millis();
    advanceClock();
    updateTemperature();
  }

  // Update anime companion animation
  updateAnimeAnimation(timeSec);

  // Render complete HUD
  renderHUD();

  delay(33); // ~30 FPS silky smooth cadence
}

// ============================================================================
// Clock & Internal Temperature
// ============================================================================
void advanceClock() {
  time_t now = time(nullptr);
  if (now > 100000000) {
    // Synchronized with NTP
    struct tm t;
    localtime_r(&now, &t);
    curHours = t.tm_hour;
    curMinutes = t.tm_min;
    curSeconds = t.tm_sec;
    curDay = t.tm_mday;
    curMonth = t.tm_mon + 1;
    curYear = t.tm_year + 1900;
    curDayOfWeek = t.tm_wday;
    return;
  }

  // Fallback internal clock
  curSeconds++;
  if (curSeconds >= 60) {
    curSeconds = 0;
    curMinutes++;
    if (curMinutes >= 60) {
      curMinutes = 0;
      curHours = (curHours + 1) % 24;
    }
  }
}

void updateTemperature() {
  // Read on-chip silicon core temperature
  float raw = temperatureRead();
  if (raw > 10.0f && raw < 110.0f) {
    currentCoreTemp = currentCoreTemp * 0.85f + raw * 0.15f;
  }
}

// ============================================================================
// Anime Companion Physics & Animation
// ============================================================================
void updateAnimeAnimation(float timeSec) {
  uint32_t now = millis();

  // Natural Blinking State
  if (!isBlinking && now > nextBlinkTime) {
    isBlinking = true;
    blinkEndTime = now + 140; // Blink duration 140ms
  }
  if (isBlinking && now > blinkEndTime) {
    isBlinking = false;
    nextBlinkTime = now + 2500 + (esp_random() % 3500); // Next blink in 2.5-6s
  }

  if (isBlinking) {
    animeAnimFrame = 2; // Blink frame
  } else {
    // Breathing cycle: 3-second gentle sine wave
    float breath = sin(timeSec * 2.1f);
    animeAnimFrame = (breath > 0.2f) ? 1 : 0;
  }
}

// ============================================================================
// HUD Renderer (Double-Buffered 160x128)
// ============================================================================
void renderHUD() {
  if (!canvas) return;

  uint16_t cBg       = scaleBrightness(COLOR_BG, displayBrightness);
  uint16_t cPanel    = scaleBrightness(COLOR_PANEL_BG, displayBrightness);
  uint16_t cEdge     = scaleBrightness(COLOR_PANEL_EDGE, displayBrightness);
  uint16_t cCyan     = scaleBrightness(COLOR_CYAN_ACCENT, displayBrightness);
  uint16_t cPink     = scaleBrightness(COLOR_PINK_ACCENT, displayBrightness);
  uint16_t cGold     = scaleBrightness(COLOR_GOLD_CLOCK, displayBrightness);
  uint16_t cGreen    = scaleBrightness(COLOR_GREEN_STAT, displayBrightness);
  uint16_t cWhite    = scaleBrightness(COLOR_TEXT_WHITE, displayBrightness);
  uint16_t cMuted    = scaleBrightness(COLOR_TEXT_MUTED, displayBrightness);
  uint16_t cRed      = scaleBrightness(COLOR_RED_PIN, displayBrightness);

  canvas->fillScreen(cBg);

  // --------------------------------------------------------------------------
  // Left Side: Anime Character Card (x=2, y=2, w=48, h=124)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(2, 2, 48, 124, 4, cPanel);
  canvas->drawRoundRect(2, 2, 48, 124, 4, cEdge);

  // Holographic Corner Accents
  canvas->drawFastHLine(4, 4, 6, cCyan);
  canvas->drawFastVLine(4, 4, 6, cCyan);
  canvas->drawFastHLine(42, 4, 6, cCyan);
  canvas->drawFastVLine(47, 4, 6, cCyan);

  // Draw 16-bit RGB565 Pixel Art Anime Sprite (40x56) at (6, 12)
  const uint16_t* animeBmp;
  if (animeAnimFrame == 2) animeBmp = anime_idle_blink;
  else if (animeAnimFrame == 1) animeBmp = anime_idle_2;
  else animeBmp = anime_idle_1;

  canvas->drawRGBBitmap(6, 12, animeBmp, ANIME_WIDTH, ANIME_HEIGHT);

  // Status Badge Under Anime Sprite
  canvas->fillRoundRect(5, 72, 42, 13, 3, cBg);
  canvas->drawRoundRect(5, 72, 42, 13, 3, cEdge);
  // Status LED dot
  canvas->fillCircle(11, 78, 2, cGreen);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(16, 75);
  canvas->print(wifiConnected ? F("SYNC") : F("IDLE"));

  // Core Temp Box under Status
  canvas->fillRoundRect(5, 88, 42, 16, 3, cBg);
  canvas->drawRoundRect(5, 88, 42, 16, 3, cEdge);
  canvas->drawBitmap(8, 91, thermo_icon, 6, 11, cPink);
  canvas->setTextColor(cWhite);
  canvas->setCursor(16, 92);
  canvas->printf("%.1fC", currentCoreTemp);

  // Mini decorative bottom bar
  canvas->drawFastHLine(8, 114, 36, cEdge);
  canvas->drawFastHLine(14, 118, 24, cPink);

  // --------------------------------------------------------------------------
  // Right Side: Top Status Header (x=53, y=2, w=105, h=14)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(53, 2, 105, 14, 3, cPanel);
  canvas->drawRoundRect(53, 2, 105, 14, 3, cEdge);

  // Location Pin & Name
  canvas->drawBitmap(57, 5, pin_icon, 7, 9, cRed);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(67, 5);
  canvas->print(LOCATION_NAME);

  // Wi-Fi Icon & Signal
  if (wifiConnected) {
    canvas->drawBitmap(147, 5, wifi_icon, 8, 8, cCyan);
  } else {
    canvas->setTextColor(cMuted);
    canvas->setCursor(135, 5);
    canvas->print(F("OFF"));
  }

  // --------------------------------------------------------------------------
  // Right Side: Digital Clock & Date Card (x=53, y=19, w=105, h=33)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(53, 19, 105, 33, 3, cPanel);
  canvas->drawRoundRect(53, 19, 105, 33, 3, cEdge);

  // Big Digital Time (HH:MM:SS)
  canvas->setTextSize(2);
  canvas->setTextColor(cGold);
  canvas->setCursor(57, 23);
  char timeBuf[10];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", curHours, curMinutes, curSeconds);
  canvas->print(timeBuf);

  // Date Ribbon
  canvas->setTextSize(1);
  canvas->setTextColor(cCyan);
  canvas->setCursor(57, 41);
  char dateBuf[20];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %s %02d",
           DAY_NAMES[curDayOfWeek], MONTH_NAMES[(curMonth - 1) % 12], curDay);
  canvas->print(dateBuf);

  // --------------------------------------------------------------------------
  // Right Side: GitHub Commits Card (x=53, y=55, w=105, h=35)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(53, 55, 105, 35, 3, cPanel);
  canvas->drawRoundRect(53, 55, 105, 35, 3, cEdge);

  // Octocat Icon + Title
  canvas->drawBitmap(57, 59, octocat_icon, 12, 12, cWhite);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(72, 61);
  canvas->print(F("GITHUB COMMITS"));

  // Commits Number Badge
  canvas->setTextSize(1);
  canvas->setTextColor(cGreen);
  canvas->setCursor(57, 75);
  canvas->printf("TOTAL: %d+", totalGithubCommits);

  // Commit Activity Bar
  int barW = 34;
  canvas->drawRect(120, 75, barW, 9, cEdge);
  int fillW = map(constrain(totalGithubCommits, 0, 100), 0, 100, 2, barW - 2);
  canvas->fillRect(121, 76, fillW, 7, cGreen);

  // --------------------------------------------------------------------------
  // Right Side: Hardware Vitals Card (x=53, y=93, w=105, h=33)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(53, 93, 105, 33, 3, cPanel);
  canvas->drawRoundRect(53, 93, 105, 33, 3, cEdge);

  canvas->setTextSize(1);
  canvas->setTextColor(cMuted);
  canvas->setCursor(57, 97);
  canvas->printf("CPU: 240MHz");

  canvas->setCursor(57, 107);
  canvas->printf("RAM: %luK Free", ESP.getFreeHeap() / 1024);

  // Uptime in corner
  uint32_t upSec = (millis() - bootMillis) / 1000;
  canvas->setTextColor(cCyan);
  canvas->setCursor(57, 117);
  canvas->printf("UPTIME: %lum %lus", upSec / 60, upSec % 60);

  // Push completed canvas buffer over 27 MHz SPI
  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), SCREEN_W, SCREEN_H);
}

// ----------------------------------------------------------------------------
// Color Brightness Scaling
// ----------------------------------------------------------------------------
uint16_t scaleBrightness(uint16_t color, uint8_t brightness) {
  if (brightness >= 255) return color;
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
  return (r << 11) | (g << 5) | b;
}

// ============================================================================
// Interactive Serial Commands (115200 baud)
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.peek();
    if (c == '+' || c == '=') {
      Serial.read();
      if (displayBrightness <= 235) displayBrightness += 20; else displayBrightness = 255;
      analogWrite(TFT_BL, displayBrightness);
      Serial.printf("[HUD] Brightness: %d (%d%%)\n", displayBrightness, (displayBrightness * 100) / 255);
      continue;
    } else if (c == '-' || c == '_') {
      Serial.read();
      if (displayBrightness >= 35) displayBrightness -= 20; else displayBrightness = 15;
      analogWrite(TFT_BL, displayBrightness);
      Serial.printf("[HUD] Brightness: %d (%d%%)\n", displayBrightness, (displayBrightness * 100) / 255);
      continue;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.equalsIgnoreCase("flip")) {
      screenRotation = (screenRotation + 2) % 4;
      tft.setRotation(screenRotation);
      Serial.printf("[HUD] Flipped 180°. Rotation: %d\n", screenRotation);
    } else if (line.startsWith("commits ")) {
      totalGithubCommits = line.substring(8).toInt();
      Serial.printf("[HUD] Commits set to: %d\n", totalGithubCommits);
    } else if (line.equalsIgnoreCase("status")) {
      Serial.printf("[HUD] Wi-Fi: %s | Clock: %02d:%02d:%02d | Commits: %d | Temp: %.1fC\n",
                    wifiConnected ? "CONNECTED" : "OFFLINE",
                    curHours, curMinutes, curSeconds,
                    totalGithubCommits, currentCoreTemp);
    }
  }
}

// ============================================================================
// Background FreeRTOS Worker (Wi-Fi + NTP + GitHub API Fetch)
// ============================================================================
void backgroundNetworkWorker(void* pvParameters) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      Serial.printf("[NET WORKER] Connecting to Wi-Fi '%s'...\n", wifiSSID);
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifiSSID, wifiPassword);

      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("[NET WORKER] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        // Sync time with NTP (IST UTC+5:30)
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
      } else {
        Serial.println("[NET WORKER] Connection failed. Retrying in 10s...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
    } else {
      wifiConnected = true;
    }

    // Fetch GitHub commit stats every 10 minutes
    if (wifiConnected && (millis() - lastGithubFetch >= 600000 || !githubFetchDone)) {
      lastGithubFetch = millis();
      Serial.println("[GITHUB] Fetching total repository commits...");

      WiFiClientSecure client;
      client.setInsecure(); // No certificate check for speed
      HTTPClient https;
      https.setTimeout(8000);

      String url = "https://api.github.com/repos/joyboy-pega/desk-buggy/commits?per_page=1";
      if (https.begin(client, url)) {
        https.setUserAgent("ESP32-Telemetry-Watch");
        const char* headerKeys[] = {"Link"};
        https.collectHeaders(headerKeys, 1);

        int httpCode = https.GET();
        if (httpCode == HTTP_CODE_OK) {
          if (https.hasHeader("Link")) {
            String linkHeader = https.header("Link");
            int lastPageIdx = linkHeader.lastIndexOf("&page=");
            if (lastPageIdx != -1) {
              int endIdx = linkHeader.indexOf('>', lastPageIdx);
              if (endIdx != -1) {
                int count = linkHeader.substring(lastPageIdx + 6, endIdx).toInt();
                if (count > 0) {
                  totalGithubCommits = count;
                  githubFetchDone = true;
                  Serial.printf("[GITHUB] Live Commits Updated: %d\n", totalGithubCommits);
                }
              }
            }
          }
        }
        https.end();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

