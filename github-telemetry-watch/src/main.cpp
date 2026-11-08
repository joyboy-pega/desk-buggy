#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "hud_icons.h"

// ============================================================================
// Developer Telemetry Watch & GitHub Stats HUD (No Anime)
// Tracks: NTP Time, Pune Location, Date, Core Temp, and Live GitHub Commits
// Target: ESP32-WROOM-32 + 1.8" ST7735 Color TFT (160x128, Rotation 3)
// ============================================================================

// ----------------------------------------------------------------------------
// Master Pinout for ESP32-WROOM-32 (Standard 30-pin)
// ----------------------------------------------------------------------------
#define TFT_CS    5   // Chip Select (GPIO 5)
#define TFT_DC    2   // Data/Command A0 (GPIO 2)
#define TFT_RST   4   // Hardware Reset (GPIO 4)
#define TFT_MOSI 23   // Hardware VSPI MOSI / SDA (GPIO 23)
#define TFT_SCLK 18   // Hardware VSPI SCLK / SCK (GPIO 18)
#define TFT_BL   32   // Backlight PWM pin

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 160x128 Double-Buffered Canvas for ZERO-Flicker 60 FPS Rendering
GFXcanvas16* canvas = nullptr;
const int SCREEN_W = 160;
const int SCREEN_H = 128;

// ----------------------------------------------------------------------------
// Color Palette (16-bit RGB565)
// ----------------------------------------------------------------------------
#define COLOR_BG          0x0821 // Deep Obsidian Black
#define COLOR_CARD_BG     0x10A3 // Slate Midnight Navy
#define COLOR_CARD_BORDER 0x2966 // Card Outline
#define COLOR_CLOCK_GOLD  0xFDE0 // Warm Amber Gold
#define COLOR_CYAN_ACCENT 0x3FFA // Neon Cyber Cyan
#define COLOR_GITHUB_GRN  0x2FE4 // Bright GitHub Emerald Green
#define COLOR_TEXT_WHITE  0xFFFF // Crisp White
#define COLOR_TEXT_MUTED  0x94B2 // Slate Silver
#define COLOR_RED_PIN     0xF986 // Crimson Red
#define COLOR_HEATMAP_LO  0x1245 // Faint Dark Green
#define COLOR_HEATMAP_MID 0x1AE9 // Medium Green
#define COLOR_HEATMAP_HI  0x2FE4 // Bright Neon Green

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
int curHours = 22;
int curMinutes = 0;
int curSeconds = 0;
int curDay = 8;
int curMonth = 11;
int curYear = 2026;
int curDayOfWeek = 0; // Sun=0

float currentCoreTemp = 38.2f;
unsigned long lastTickMillis = 0;
unsigned long bootMillis = 0;

// GitHub User & Commits Telemetry
const char* GITHUB_USER = "joyboy-pega";
volatile int totalGithubCommits = 873; // Live baseline for joyboy-pega
volatile bool githubFetchDone = false;
unsigned long lastGithubFetch = 0;

const char* DAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Simulated Contribution Heatmap Matrix (4 rows x 8 columns)
uint8_t heatmapMatrix[4][8] = {
  {1, 2, 3, 2, 3, 3, 2, 3},
  {2, 1, 3, 3, 2, 3, 3, 2},
  {3, 3, 2, 3, 1, 2, 3, 3},
  {1, 2, 3, 3, 3, 2, 3, 3}
};

// ----------------------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------------------
void advanceClock();
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
  Serial.println("  GitHub Dev Telemetry Watch HUD (No Anime Version)     ");
  Serial.println("========================================================");

  // Setup Backlight PWM
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, displayBrightness);

  // High-Speed SPI (27 MHz)
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(27000000);

  // Initialize ST7735 (1.8" BlackTab, 180° Rotated: rotation=3)
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(screenRotation);
  tft.fillScreen(COLOR_BG);

  // Allocate 160x128 double-buffered canvas (40KB RAM)
  canvas = new GFXcanvas16(SCREEN_W, SCREEN_H);
  if (canvas) {
    Serial.println("[OK] 160x128 Double-Buffered Canvas Active (Zero Flicker!)");
  }

  // Launch Background Network Worker (Wi-Fi + NTP + GitHub Fetch)
  xTaskCreate(
    backgroundNetworkWorker,
    "NetWorker",
    8192,
    NULL,
    1,
    NULL
  );

  Serial.println("[OK] GitHub Telemetry Watch Active!");
  Serial.println("Hotkeys: '+' / '-' for brightness, 'flip' to rotate 180, 'commits <n>' to mock");
}

// ============================================================================
// Main 30 FPS Render Loop
// ============================================================================
void loop() {
  handleSerialCommands();

  // 1-second clock tick & internal sensors
  if (millis() - lastTickMillis >= 1000) {
    lastTickMillis = millis();
    advanceClock();
    updateTemperature();
  }

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
  float raw = temperatureRead();
  if (raw > 10.0f && raw < 110.0f) {
    currentCoreTemp = currentCoreTemp * 0.85f + raw * 0.15f;
  }
}

// ============================================================================
// HUD Renderer (Full Width 160x128 Double-Buffered)
// ============================================================================
void renderHUD() {
  if (!canvas) return;

  uint16_t cBg       = scaleBrightness(COLOR_BG, displayBrightness);
  uint16_t cCard     = scaleBrightness(COLOR_CARD_BG, displayBrightness);
  uint16_t cBorder   = scaleBrightness(COLOR_CARD_BORDER, displayBrightness);
  uint16_t cClock    = scaleBrightness(COLOR_CLOCK_GOLD, displayBrightness);
  uint16_t cCyan     = scaleBrightness(COLOR_CYAN_ACCENT, displayBrightness);
  uint16_t cGreen    = scaleBrightness(COLOR_GITHUB_GRN, displayBrightness);
  uint16_t cWhite    = scaleBrightness(COLOR_TEXT_WHITE, displayBrightness);
  uint16_t cMuted    = scaleBrightness(COLOR_TEXT_MUTED, displayBrightness);
  uint16_t cRed      = scaleBrightness(COLOR_RED_PIN, displayBrightness);

  canvas->fillScreen(cBg);

  // --------------------------------------------------------------------------
  // 1. Top Header Bar (y=2 to 15, h=14)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(2, 2, 156, 14, 3, cCard);
  canvas->drawRoundRect(2, 2, 156, 14, 3, cBorder);

  // Location Pin & Name
  canvas->drawBitmap(6, 5, pin_icon, 7, 9, cRed);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(16, 5);
  canvas->print(LOCATION_NAME);

  // Profile Handle
  canvas->setTextColor(cCyan);
  canvas->setCursor(68, 5);
  canvas->print(F("@joyboy-pega"));

  // Wi-Fi Status Icon
  if (wifiConnected) {
    canvas->drawBitmap(145, 5, wifi_icon, 8, 8, cGreen);
  } else {
    canvas->setTextColor(cMuted);
    canvas->setCursor(140, 5);
    canvas->print(F("OFF"));
  }

  // --------------------------------------------------------------------------
  // 2. Large Digital Watch Card (y=18 to 54, h=37)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(2, 18, 156, 37, 4, cCard);
  canvas->drawRoundRect(2, 18, 156, 37, 4, cBorder);

  // Neon Cyan accent bracket
  canvas->drawFastVLine(4, 22, 28, cCyan);

  // Double-Height Digital Clock (HH:MM:SS)
  canvas->setTextSize(2);
  canvas->setTextColor(cClock);
  canvas->setCursor(12, 22);
  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", curHours, curMinutes, curSeconds);
  canvas->print(timeBuf);

  // Date Ribbon
  canvas->setTextSize(1);
  canvas->setTextColor(cCyan);
  canvas->setCursor(12, 41);
  char dateBuf[26];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s %d",
           DAY_NAMES[curDayOfWeek], curDay, MONTH_NAMES[(curMonth - 1) % 12], curYear);
  canvas->print(dateBuf);

  // 12H AM/PM tag on right
  canvas->setTextSize(1);
  canvas->setTextColor(cMuted);
  canvas->setCursor(120, 24);
  canvas->print((curHours >= 12) ? F("PM") : F("AM"));

  // --------------------------------------------------------------------------
  // 3. GitHub Commits Card (y=57 to 95, h=39)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(2, 57, 156, 39, 4, cCard);
  canvas->drawRoundRect(2, 57, 156, 39, 4, cBorder);

  // Octocat Icon & Label
  canvas->drawBitmap(6, 61, octocat_icon, 12, 12, cWhite);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(22, 63);
  canvas->print(F("GITHUB COMMITS 2026"));

  // Giant Total Commits Number
  canvas->setTextSize(2);
  canvas->setTextColor(cGreen);
  canvas->setCursor(6, 76);
  canvas->printf("%d", totalGithubCommits);

  // "this year" caption
  canvas->setTextSize(1);
  canvas->setTextColor(cMuted);
  canvas->setCursor(54, 82);
  canvas->print(F("in the last year"));

  // Mini GitHub Contribution Heatmap Tiles on the right side
  int heatX = 110;
  int heatY = 62;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 8; c++) {
      uint16_t tileColor;
      uint8_t lvl = heatmapMatrix[r][c];
      if (lvl == 1) tileColor = scaleBrightness(COLOR_HEATMAP_LO, displayBrightness);
      else if (lvl == 2) tileColor = scaleBrightness(COLOR_HEATMAP_MID, displayBrightness);
      else tileColor = scaleBrightness(COLOR_HEATMAP_HI, displayBrightness);

      canvas->fillRect(heatX + (c * 5), heatY + (r * 6), 4, 4, tileColor);
    }
  }

  // --------------------------------------------------------------------------
  // 4. Telemetry Vitals Bar (y=98 to 126, h=28)
  // --------------------------------------------------------------------------
  canvas->fillRoundRect(2, 98, 156, 28, 4, cCard);
  canvas->drawRoundRect(2, 98, 156, 28, 4, cBorder);

  // Thermometer & Silicon Core Temp
  canvas->drawBitmap(6, 103, thermo_icon, 6, 11, cRed);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(15, 105);
  canvas->printf("%.1fC", currentCoreTemp);

  // CPU & RAM Vitals
  canvas->setTextColor(cMuted);
  canvas->setCursor(58, 105);
  canvas->printf("CPU: 240MHz");

  canvas->setCursor(58, 115);
  canvas->printf("RAM: %luK", ESP.getFreeHeap() / 1024);

  // Uptime Counter
  uint32_t upSec = (millis() - bootMillis) / 1000;
  canvas->setTextColor(cCyan);
  canvas->setCursor(118, 105);
  canvas->printf("%lum %lus", upSec / 60, upSec % 60);

  canvas->setTextColor(cMuted);
  canvas->setCursor(118, 115);
  canvas->print(F("UPTIME"));

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
      Serial.printf("[HUD] Commits manually set to: %d\n", totalGithubCommits);
    } else if (line.equalsIgnoreCase("status")) {
      Serial.printf("[HUD] Wi-Fi: %s | Time: %02d:%02d:%02d | Commits: %d | Core Temp: %.1fC\n",
                    wifiConnected ? "CONNECTED" : "OFFLINE",
                    curHours, curMinutes, curSeconds,
                    totalGithubCommits, currentCoreTemp);
    }
  }
}

// ============================================================================
// Background FreeRTOS Worker (Wi-Fi + NTP + GitHub Fetch)
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
      Serial.println("[GITHUB] Querying user contributions for @joyboy-pega...");

      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient https;
      https.setTimeout(8000);

      String url = "https://github.com/users/joyboy-pega/contributions";
      if (https.begin(client, url)) {
        https.setUserAgent("Mozilla/5.0 (ESP32 Developer HUD)");
        int httpCode = https.GET();
        if (httpCode == HTTP_CODE_OK) {
          String body = https.getString();
          int idx = body.indexOf("contributions");
          if (idx > 0) {
            int numEnd = idx - 1;
            while (numEnd > 0 && (body[numEnd] == ' ' || body[numEnd] == '\n' || body[numEnd] == '\r' || body[numEnd] == '\t')) {
              numEnd--;
            }
            int numStart = numEnd;
            while (numStart > 0 && ((body[numStart] >= '0' && body[numStart] <= '9') || body[numStart] == ',')) {
              numStart--;
            }
            numStart++;
            String numStr = body.substring(numStart, numEnd + 1);
            numStr.replace(",", "");
            int count = numStr.toInt();
            if (count > 0) {
              totalGithubCommits = count;
              githubFetchDone = true;
              Serial.printf("[GITHUB] Extracted Total Commits: %d\n", totalGithubCommits);
            }
          }
        }
        https.end();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

