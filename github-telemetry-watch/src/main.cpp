#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "hud_icons.h"

// ============================================================================
// Developer Telemetry Watch & GitHub Stats HUD (Ultra-Responsive Dual-Core)
// MCU: ESP32-WROOM-32 (Dual Core 240MHz) | Display: 1.8" ST7735 (160x128)
// Real-time: NTP Time, Pune, IN, Silicon Temp, Live GitHub Commits (@joyboy-pega)
// Built-in Web Portal + Hotkey Serial Control + Async Network Task on Core 0
// ============================================================================

// ----------------------------------------------------------------------------
// Master Pinout for ESP32-WROOM-32 (Standard 30-pin)
// ----------------------------------------------------------------------------
#define TFT_CS    5   // Chip Select (GPIO 5)
#define TFT_DC    2   // Data/Command A0 (GPIO 2)
#define TFT_RST   4   // Hardware Reset (GPIO 4)
#define TFT_MOSI 23   // Hardware VSPI MOSI / SDA (GPIO 23)
#define TFT_SCLK 18   // Hardware VSPI SCLK / SCK (GPIO 18)
#define TFT_BL   32   // Backlight PWM pin (GPIO 32)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 160x128 Double-Buffered Canvas for ZERO-Flicker Rendering
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
// Wi-Fi Credentials & Built-in Web Server
// ----------------------------------------------------------------------------
const char* wifiSSID = "Arigatoo1";
const char* wifiPassword = "12345678";
volatile bool wifiConnected = false;
String localIPStr = "Connecting...";
WebServer server(80);

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
volatile bool uiNeedsRedraw = true;

// GitHub User & Commits Telemetry
const char* GITHUB_USER = "joyboy-pega";
volatile int totalGithubCommits = 894; // Baseline for joyboy-pega (894 in 2026)
volatile bool githubFetchDone = false;
volatile bool triggerManualFetch = false;
unsigned long lastGithubFetch = 0;

const char* DAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Contribution Heatmap Matrix (4 rows x 8 columns)
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
void setupWebServer();
void backgroundNetworkWorker(void* pvParameters);
uint16_t scaleBrightness(uint16_t color, uint8_t brightness);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20); // Fast non-blocking timeout
  bootMillis = millis();
  delay(200);

  Serial.println("\n\n========================================================");
  Serial.println("  GitHub Dev Telemetry Watch HUD (Ultra-Responsive)     ");
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
    Serial.println("[OK] 160x128 Double-Buffered Canvas Active");
  }

  // Setup Web Server routes
  setupWebServer();

  // Launch Background Network Worker PINNED TO CORE 0
  // Core 0 handles Wi-Fi, NTP, Web Requests, and GitHub HTTPS Streaming
  // Core 1 runs the Display Render Loop and Hardware I/O with ZERO latency!
  xTaskCreatePinnedToCore(
    backgroundNetworkWorker,
    "NetWorker",
    8192,
    NULL,
    1,
    NULL,
    0 // Core 0
  );

  Serial.println("[OK] Telemetry Watch Active on Dual-Core ESP32!");
  Serial.println("Serial Hotkeys: '+' / '-' brightness | 'flip' rotate 180 | 'commits <n>' | 'refresh'");
  
  uiNeedsRedraw = true;
  renderHUD();
}

// ============================================================================
// Main Loop (Core 1: Dedicated to Display, Sensor, & Instant Response)
// ============================================================================
void loop() {
  // 1. Process instant serial commands
  handleSerialCommands();

  // 2. 1-second clock tick & internal silicon temperature
  unsigned long now = millis();
  if (now - lastTickMillis >= 1000) {
    lastTickMillis = now;
    advanceClock();
    updateTemperature();
    uiNeedsRedraw = true;
  }

  // 3. Render HUD immediately whenever state changes
  if (uiNeedsRedraw) {
    uiNeedsRedraw = false;
    renderHUD();
  }

  // 4. Ultra-short sleep to yield to FreeRTOS scheduler without lag
  delay(10);
}

// ============================================================================
// Clock & Silicon Sensor
// ============================================================================
void advanceClock() {
  time_t t_now = time(nullptr);
  if (t_now > 100000000) {
    struct tm t;
    localtime_r(&t_now, &t);
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
// HUD Renderer (Full Width 160x128 Double-Buffered, Zero-Flicker)
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
  canvas->drawBitmap(5, 5, pin_icon, 7, 9, cRed);
  canvas->setTextSize(1);
  canvas->setTextColor(cWhite);
  canvas->setCursor(15, 5);
  canvas->print(LOCATION_NAME);

  // Center: alternate between @joyboy-pega and IP address
  canvas->setTextColor(cCyan);
  canvas->setCursor(68, 5);
  if ((curSeconds / 4) % 2 == 0 || !wifiConnected) {
    canvas->print(F("@joyboy-pega"));
  } else {
    // Show short IP
    canvas->print(WiFi.localIP().toString());
  }

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
// Interactive Serial Commands (115200 baud, Non-blocking)
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.peek();
    if (c == '+' || c == '=') {
      Serial.read();
      if (displayBrightness <= 235) displayBrightness += 20; else displayBrightness = 255;
      analogWrite(TFT_BL, displayBrightness);
      uiNeedsRedraw = true;
      Serial.printf("[HUD] Brightness: %d (%d%%)\n", displayBrightness, (displayBrightness * 100) / 255);
      continue;
    } else if (c == '-' || c == '_') {
      Serial.read();
      if (displayBrightness >= 35) displayBrightness -= 20; else displayBrightness = 15;
      analogWrite(TFT_BL, displayBrightness);
      uiNeedsRedraw = true;
      Serial.printf("[HUD] Brightness: %d (%d%%)\n", displayBrightness, (displayBrightness * 100) / 255);
      continue;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.equalsIgnoreCase("flip")) {
      screenRotation = (screenRotation + 2) % 4;
      tft.setRotation(screenRotation);
      uiNeedsRedraw = true;
      Serial.printf("[HUD] Flipped 180°. Rotation: %d\n", screenRotation);
    } else if (line.startsWith("commits ")) {
      totalGithubCommits = line.substring(8).toInt();
      uiNeedsRedraw = true;
      Serial.printf("[HUD] Commits manually set to: %d\n", totalGithubCommits);
    } else if (line.equalsIgnoreCase("refresh")) {
      triggerManualFetch = true;
      Serial.println("[HUD] GitHub sync requested!");
    } else if (line.equalsIgnoreCase("status")) {
      Serial.printf("[HUD] Wi-Fi: %s (%s) | Time: %02d:%02d:%02d | Commits: %d | Core Temp: %.1fC | Heap: %luK\n",
                    wifiConnected ? "CONNECTED" : "OFFLINE",
                    localIPStr.c_str(),
                    curHours, curMinutes, curSeconds,
                    totalGithubCommits, currentCoreTemp, ESP.getFreeHeap() / 1024);
    }
  }
}

// ============================================================================
// HTML Web Control Dashboard (Served on http://<ESP32_IP>)
// ============================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>GitHub Telemetry Watch HUD</title>
  <style>
    :root {
      --bg: #090d16;
      --card-bg: #121a29;
      --border: #233554;
      --cyan: #3ffaed;
      --green: #2fe47c;
      --gold: #fde047;
      --text: #e2e8f0;
      --text-muted: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace; }
    body { background: var(--bg); color: var(--text); padding: 20px; display: flex; justify-content: center; }
    .container { width: 100%; max-width: 480px; }
    header { text-align: center; margin-bottom: 24px; }
    header h1 { font-size: 1.5rem; color: var(--cyan); letter-spacing: 1px; display: flex; align-items: center; justify-content: center; gap: 8px; }
    header p { color: var(--text-muted); font-size: 0.85rem; margin-top: 4px; }
    .card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 18px; margin-bottom: 16px; box-shadow: 0 4px 20px rgba(0,0,0,0.4); }
    .card h2 { font-size: 0.9rem; text-transform: uppercase; color: var(--text-muted); margin-bottom: 12px; letter-spacing: 0.5px; }
    .stat-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; font-size: 0.95rem; }
    .stat-val { font-weight: bold; font-family: monospace; }
    .stat-val.gold { color: var(--gold); }
    .stat-val.green { color: var(--green); font-size: 1.4rem; }
    .stat-val.cyan { color: var(--cyan); }
    .control-group { margin-bottom: 16px; }
    label { display: block; font-size: 0.85rem; color: var(--text-muted); margin-bottom: 6px; }
    input[type=range] { width: 100%; accent-color: var(--cyan); }
    .btn-row { display: flex; gap: 10px; margin-top: 14px; }
    button { flex: 1; padding: 10px; border-radius: 8px; border: 1px solid var(--cyan); background: transparent; color: var(--cyan); font-weight: bold; cursor: pointer; transition: 0.2s; }
    button:hover { background: var(--cyan); color: #000; }
    button.primary { background: var(--green); border-color: var(--green); color: #000; }
    button.primary:hover { opacity: 0.9; }
    .form-inline { display: flex; gap: 8px; margin-top: 8px; }
    .form-inline input { flex: 1; background: #0c121e; border: 1px solid var(--border); color: #fff; padding: 8px 12px; border-radius: 6px; font-size: 0.95rem; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>⚡ Telemetry Watch HUD</h1>
      <p>Target: @joyboy-pega | Pune, IN | 1.8" ST7735</p>
    </header>

    <div class="card">
      <h2>Live Telemetry</h2>
      <div class="stat-row">
        <span>Time (IST):</span>
        <span id="time" class="stat-val gold">--:--:--</span>
      </div>
      <div class="stat-row">
        <span>GitHub Commits:</span>
        <span id="commits" class="stat-val green">894</span>
      </div>
      <div class="stat-row">
        <span>Silicon Temp:</span>
        <span id="temp" class="stat-val cyan">--.-°C</span>
      </div>
      <div class="stat-row">
        <span>Free RAM:</span>
        <span id="ram" class="stat-val">-- KB</span>
      </div>
    </div>

    <div class="card">
      <h2>Display Controls</h2>
      <div class="control-group">
        <label>Brightness: <span id="brightVal">100%</span></label>
        <input type="range" id="brightSlider" min="15" max="255" value="255" oninput="setBrightness(this.value)">
      </div>
      <div class="btn-row">
        <button onclick="flipScreen()">🔄 Rotate 180°</button>
        <button class="primary" onclick="refreshGithub()">🚀 Refresh GitHub</button>
      </div>
    </div>

    <div class="card">
      <h2>Override Commits Count</h2>
      <div class="form-inline">
        <input type="number" id="manualCommits" placeholder="e.g. 894" value="894">
        <button onclick="setCommits()">Set</button>
      </div>
    </div>
  </div>

  <script>
    async function updateStats() {
      try {
        const res = await fetch('/api/stats');
        const data = await res.json();
        document.getElementById('time').innerText = data.time;
        document.getElementById('commits').innerText = data.commits;
        document.getElementById('temp').innerText = data.temp + '°C';
        document.getElementById('ram').innerText = data.ram + ' KB';
        document.getElementById('brightVal').innerText = Math.round((data.brightness / 255) * 100) + '%';
        document.getElementById('brightSlider').value = data.brightness;
      } catch (e) {}
    }

    async function setBrightness(val) {
      document.getElementById('brightVal').innerText = Math.round((val / 255) * 100) + '%';
      await fetch('/api/brightness?val=' + val);
    }

    async function flipScreen() {
      await fetch('/api/flip');
    }

    async function refreshGithub() {
      await fetch('/api/refresh');
      alert('GitHub sync triggered! Re-fetching stats...');
    }

    async function setCommits() {
      const val = document.getElementById('manualCommits').value;
      if (val) {
        await fetch('/api/commits?val=' + val);
        updateStats();
      }
    }

    setInterval(updateStats, 1000);
    updateStats();
  </script>
</body>
</html>
)rawliteral";

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/stats", HTTP_GET, []() {
    char buf[128];
    snprintf(buf, sizeof(buf),
      "{\"time\":\"%02d:%02d:%02d\",\"commits\":%d,\"temp\":%.1f,\"ram\":%lu,\"brightness\":%d}",
      curHours, curMinutes, curSeconds, totalGithubCommits, currentCoreTemp, ESP.getFreeHeap() / 1024, displayBrightness);
    server.send(200, "application/json", buf);
  });

  server.on("/api/brightness", HTTP_GET, []() {
    if (server.hasArg("val")) {
      displayBrightness = server.arg("val").toInt();
      analogWrite(TFT_BL, displayBrightness);
      uiNeedsRedraw = true;
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/flip", HTTP_GET, []() {
    screenRotation = (screenRotation + 2) % 4;
    tft.setRotation(screenRotation);
    uiNeedsRedraw = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/refresh", HTTP_GET, []() {
    triggerManualFetch = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/commits", HTTP_GET, []() {
    if (server.hasArg("val")) {
      totalGithubCommits = server.arg("val").toInt();
      uiNeedsRedraw = true;
    }
    server.send(200, "text/plain", "OK");
  });
}

// ============================================================================
// Streaming GitHub Fetcher (Consumes < 1KB RAM, Never Exhausts Heap!)
// ============================================================================
void streamGithubContributions() {
  Serial.println("[GITHUB] Streaming contributions for @joyboy-pega...");

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10); // 10s timeout
  
  HTTPClient https;
  https.setTimeout(8000);

  String url = "https://github.com/users/joyboy-pega/contributions";
  if (!https.begin(client, url)) {
    Serial.println("[GITHUB] Connection begin failed!");
    return;
  }

  https.setUserAgent("Mozilla/5.0 (ESP32 Developer HUD)");
  int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = https.getStreamPtr();
    bool foundActivity = false;
    unsigned long startRead = millis();

    // Stream lines without loading the whole 230KB file
    while (stream->connected() && (stream->available() || (millis() - startRead < 4000))) {
      if (stream->available()) {
        String line = stream->readStringUntil('\n');
        line.trim();

        if (line.indexOf("js-contribution-activity-description") >= 0) {
          foundActivity = true;
          continue;
        }

        if (foundActivity && line.length() > 0) {
          // Expected line: "894" or "894 contributions"
          int count = line.toInt();
          if (count > 0) {
            totalGithubCommits = count;
            githubFetchDone = true;
            uiNeedsRedraw = true;
            Serial.printf("[GITHUB] Extracted Total Commits: %d\n", totalGithubCommits);
            break;
          }
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
  } else {
    Serial.printf("[GITHUB] HTTP Code: %d\n", httpCode);
  }

  https.end();
}

// ============================================================================
// Background FreeRTOS Worker (PINNED TO CORE 0)
// Manages Wi-Fi, NTP Sync, Web Server, and Async GitHub Fetching
// ============================================================================
void backgroundNetworkWorker(void* pvParameters) {
  while (true) {
    // 1. Manage Wi-Fi Connection
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      localIPStr = "Connecting...";
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
        localIPStr = WiFi.localIP().toString();
        Serial.printf("[NET WORKER] Connected! IP: %s\n", localIPStr.c_str());
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
        server.begin();
        Serial.println("[NET WORKER] Web Server active on port 80");
        uiNeedsRedraw = true;
      } else {
        Serial.println("[NET WORKER] Retrying Wi-Fi in 10s...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
    } else {
      wifiConnected = true;
    }

    // 2. Handle incoming web clients
    for (int i = 0; i < 20; i++) {
      server.handleClient();
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 3. Fetch GitHub stats every 10 minutes or when requested
    if (wifiConnected && (millis() - lastGithubFetch >= 600000 || !githubFetchDone || triggerManualFetch)) {
      triggerManualFetch = false;
      lastGithubFetch = millis();
      streamGithubContributions();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
