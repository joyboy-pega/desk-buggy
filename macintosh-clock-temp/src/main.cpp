#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "mac_bitmaps.h"

// ============================================================================
// Telemetry Watch & Clock Station for ESP32-C3 SuperMini + 0.96" OLED (SSD1306)
// ============================================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi Credentials & Background Connectivity
String wifiSSID = "Arigatoo1";
String wifiPassword = "12345678";
volatile bool wifiConnected = false;
const long gmtOffsetSec = 19800; // IST UTC+5:30 (Pune, India)
const int daylightOffsetSec = 0;

// Internal Clock Variables (Fallback base time)
int curHours = 13;
int curMinutes = 15;
int curSeconds = 0;
int curYear = 2026;
int curMonth = 11;
int curDay = 8;
int curDayOfWeek = 0; // 0=Sun, 1=Mon...

unsigned long lastTickMillis = 0;
unsigned long bootMillis = 0;
float currentTempC = 36.5f;
float currentTempF = 97.7f;
uint8_t displayContrast = 0x38; // Decreased brightness, clean & comfortable

// Temperature Telemetry Buffer for Live Scrolling Graph
const int TEMP_HISTORY_SIZE = 64;
float tempHistory[TEMP_HISTORY_SIZE];
int historyIndex = 0;
unsigned long lastGraphSample = 0;

// Walk & Fitness Telemetry
unsigned long totalSteps = 6420;
float distanceKm = 4.82f;
int caloriesKcal = 318;
int walkPaceMin = 5;
int walkPaceSec = 14;
int runnerAnimFrame = 0;
bool isWalking = true;
unsigned long lastWalkStepTime = 0;

enum DisplayMode {
  MODE_SYSTEM_WINDOW,  // 1. Classic "System Status" Macintosh window (Single All-in-One HUD)
  MODE_GIANT_CLOCK,    // 2. Large retro digital clock + CPU & RAM telemetry
  MODE_TEMP_GRAPH,     // 3. Live 64-sample telemetry waveform graph
  MODE_WALK_TELEMETRY  // 4. Walk & Activity Telemetry HUD (Steps, Distance, Pace)
};

DisplayMode currentMode = MODE_SYSTEM_WINDOW;
unsigned long lastModeSwitch = 0;
bool autoCycleModes = false; // Keep only one UI, do not toggle automatically

const char* DAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// FreeRTOS Wi-Fi Background Task
TaskHandle_t wifiTaskHandle = NULL;

// -------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------
void advanceClockOneSecond();
void updateTemperature();
void updateWalkTelemetry();
void renderSystemWindow();
void renderGiantClock();
void renderTempGraph();
void renderWalkTelemetry();
void handleSerialCommands();
void wifiBackgroundWorker(void* pvParameters);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  bootMillis = millis();

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32-C3 SuperMini Telemetry Watch & Clock HUD        ");
  Serial.println("========================================================");

  // Initialize temperature history buffer
  for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
    tempHistory[i] = 36.5f;
  }

  // 1. Initialize High-Speed Standard 400kHz I2C for 0.96" OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400 kHz rock-solid standard clock

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[WARN] Retrying I2C on alternate pins...");
    Wire.begin(9, 8);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  }

  // 2. Explicitly power ON OLED panel with comfortable contrast
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.dim(false);
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(displayContrast); // Comfortable, reduced brightness

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Splash Screen
  display.drawBitmap(12, 14, apple_icon, 10, 12, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(30, 16);
  display.print(F("TELEMETRY HUD"));
  display.setCursor(30, 28);
  display.print(F("WATCH & CLOCK"));
  display.setCursor(30, 42);
  display.print(F("STARTING UP..."));
  display.display();
  delay(1000);

  lastTickMillis = millis();
  lastModeSwitch = millis();
  lastGraphSample = millis();

  // 3. Spawn background FreeRTOS Wi-Fi task (keeps UI rendering silky smooth)
  xTaskCreate(
    wifiBackgroundWorker,
    "WiFiWorker",
    4096,
    NULL,
    1,
    &wifiTaskHandle
  );

  Serial.println("[OK] Telemetry Watch & Clock Engine Running!");
  Serial.println("Hotkeys in Serial Monitor (115200 baud):");
  Serial.println("  ' ' or 'm' -> Switch screen mode (1: System / 2: Giant / 3: Graph / 4: Walk)");
  Serial.println("  'w'        -> Simulate walking steps");
  Serial.println("  'a'        -> Toggle auto-cycling screens");
  Serial.println("  'temp'     -> Print live hardware telemetry");
}

// ============================================================================
// Main UI Loop (Silky Smooth 30 FPS)
// ============================================================================
void loop() {
  handleSerialCommands();

  // 1. One second clock & telemetry update
  if (millis() - lastTickMillis >= 1000) {
    lastTickMillis = millis();
    advanceClockOneSecond();
    updateTemperature();
    updateWalkTelemetry();
  }

  // 2. Auto-cycle through the 4 viewing modes every 8 seconds if enabled
  if (autoCycleModes && millis() - lastModeSwitch >= 8000) {
    lastModeSwitch = millis();
    currentMode = (DisplayMode)((currentMode + 1) % 4);
  }

  // 3. Render active telemetry screen
  display.clearDisplay();

  switch (currentMode) {
    case MODE_SYSTEM_WINDOW: renderSystemWindow(); break;
    case MODE_GIANT_CLOCK:   renderGiantClock();   break;
    case MODE_TEMP_GRAPH:    renderTempGraph();    break;
    case MODE_WALK_TELEMETRY: renderWalkTelemetry(); break;
  }

  display.display();
  delay(33); // ~30 FPS
}

// ============================================================================
// Clock & Timekeeping Logic
// ============================================================================
void advanceClockOneSecond() {
  time_t now = time(nullptr);
  if (now > 100000000) {
    // Synced with NTP
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

  // Standalone RTC
  curSeconds++;
  if (curSeconds >= 60) {
    curSeconds = 0;
    curMinutes++;
    if (curMinutes >= 60) {
      curMinutes = 0;
      curHours++;
      if (curHours >= 24) {
        curHours = 0;
        curDay++;
        curDayOfWeek = (curDayOfWeek + 1) % 7;
      }
    }
  }
}

// ============================================================================
// Hardware Core Temperature Telemetry
// ============================================================================
void updateTemperature() {
  float raw = temperatureRead();
  if (raw > 0.0f && raw < 120.0f) {
    currentTempC = currentTempC * 0.85f + raw * 0.15f;
  }
  currentTempF = currentTempC * 1.8f + 32.0f;

  if (millis() - lastGraphSample >= 1500) {
    lastGraphSample = millis();
    tempHistory[historyIndex] = currentTempC;
    historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
  }
}

// ============================================================================
// Walk & Fitness Telemetry Logic
// ============================================================================
void updateWalkTelemetry() {
  if (isWalking) {
    totalSteps += (esp_random() % 3) + 1; // 1-3 steps per second cadence
    distanceKm = totalSteps * 0.00075f;
    caloriesKcal = (int)(totalSteps * 0.045f);
    runnerAnimFrame = (runnerAnimFrame + 1) % 2;
  }
}

// ============================================================================
// Screen Mode 1: Classic Retro Macintosh System Status Window (All-in-One HUD)
// ============================================================================
void renderSystemWindow() {
  // Top Menu Bar
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.drawBitmap(3, 0, apple_icon, 10, 12, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(16, 1);
  display.print(F("Pune Telemetry"));

  // Wi-Fi Indicator in menu bar
  if (wifiConnected) {
    display.drawBitmap(116, 1, wifi_icon, 8, 8, SSD1306_BLACK);
  } else {
    display.setCursor(76, 1);
    display.print(F("WIFI WAIT"));
  }

  // Macintosh Window Box
  display.drawRect(2, 12, 124, 50, SSD1306_WHITE);
  display.drawFastHLine(3, 22, 122, SSD1306_WHITE);

  // Window Title Tab
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(6, 14);
  display.print(F("CLOCK & VITALS"));

  // Digital Clock
  display.setTextSize(2);
  display.setCursor(8, 25);
  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", curHours, curMinutes, curSeconds);
  display.print(timeBuf);

  // Date Ribbon
  display.setTextSize(1);
  display.setCursor(8, 41);
  char dateBuf[20];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %s %02d",
           DAY_NAMES[curDayOfWeek], MONTH_NAMES[(curMonth - 1) % 12], curDay);
  display.print(dateBuf);

  // Temperature & Thermometer Icon
  display.drawBitmap(6, 49, thermometer_icon, 9, 16, SSD1306_WHITE);
  display.setCursor(17, 52);
  display.printf("%.1fC", currentTempC);

  // Activity Walk / Steps Icon & Counter
  const unsigned char* runnerBmp = (runnerAnimFrame == 0) ? runner_walk1 : runner_walk2;
  display.drawBitmap(68, 51, runnerBmp, 12, 12, SSD1306_WHITE);
  display.setCursor(83, 52);
  display.printf("%lustp", totalSteps);
}

// ============================================================================
// Screen Mode 2: Giant Digital Watch & Core Vitals
// ============================================================================
void renderGiantClock() {
  // Top Status Bar
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.printf("%s %02d %s %d", DAY_NAMES[curDayOfWeek], curDay, MONTH_NAMES[(curMonth - 1) % 12], curYear);

  display.setCursor(102, 2);
  display.print((curHours >= 12) ? F("PM") : F("AM"));

  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // Big Double-Height Digital Clock
  int displayHours = curHours % 12;
  if (displayHours == 0) displayHours = 12;

  display.setTextSize(2);
  display.setCursor(4, 16);
  char bigTimeBuf[10];
  snprintf(bigTimeBuf, sizeof(bigTimeBuf), "%02d:%02d:%02d", displayHours, curMinutes, curSeconds);
  display.print(bigTimeBuf);

  display.drawFastHLine(0, 34, 128, SSD1306_WHITE);

  // Core Vitals Telemetry
  // 1. Thermometer + Core Temp
  display.drawBitmap(2, 38, thermometer_icon, 9, 16, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(14, 40);
  display.printf("%.1f C (%.0f F)", currentTempC, currentTempF);

  // 2. CPU Freq & Free SRAM
  display.drawBitmap(2, 51, chip_icon, 12, 12, SSD1306_WHITE);
  display.setCursor(16, 53);
  display.printf("160MHz | RAM %dK", ESP.getFreeHeap() / 1024);
}

// ============================================================================
// Screen Mode 3: Live Real-Time Telemetry Graph
// ============================================================================
void renderTempGraph() {
  // Mini Clock in Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print(F("TEMP TELEMETRY"));

  display.setCursor(80, 2);
  char miniTimeBuf[10];
  snprintf(miniTimeBuf, sizeof(miniTimeBuf), "%02d:%02d:%02d", curHours, curMinutes, curSeconds);
  display.print(miniTimeBuf);

  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // Find min and max in telemetry buffer for auto-scaling
  float minT = 100.0f, maxT = 0.0f;
  for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
    if (tempHistory[i] < minT) minT = tempHistory[i];
    if (tempHistory[i] > maxT) maxT = tempHistory[i];
  }
  if (maxT - minT < 2.0f) {
    maxT += 1.0f;
    minT -= 1.0f;
  }

  // Draw Graph Axes Box
  const int graphX = 2;
  const int graphY = 14;
  const int graphW = 124;
  const int graphH = 36;
  display.drawRect(graphX, graphY, graphW, graphH, SSD1306_WHITE);

  // Plot 64 points (scaled to 120 pixels wide)
  int prevPlotX = -1, prevPlotY = -1;
  for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
    int idx = (historyIndex + i) % TEMP_HISTORY_SIZE;
    float val = tempHistory[idx];

    int px = graphX + 2 + (i * (graphW - 4)) / (TEMP_HISTORY_SIZE - 1);
    int py = graphY + graphH - 2 - (int)(((val - minT) / (maxT - minT)) * (graphH - 4));

    if (py < graphY + 1) py = graphY + 1;
    if (py > graphY + graphH - 2) py = graphY + graphH - 2;

    if (prevPlotX != -1) {
      display.drawLine(prevPlotX, prevPlotY, px, py, SSD1306_WHITE);
    }
    prevPlotX = px;
    prevPlotY = py;
  }

  // Bottom Stats
  display.setCursor(2, 54);
  display.printf("LO:%.1f HI:%.1f NOW:%.1fC", minT, maxT, currentTempC);
}

// ============================================================================
// Screen Mode 4: Walk & Activity Telemetry HUD
// ============================================================================
void renderWalkTelemetry() {
  // Header: Walk Telemetry + Digital Clock
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print(F("WALK TELEMETRY"));

  display.setCursor(80, 2);
  display.printf("%02d:%02d:%02d", curHours, curMinutes, curSeconds);

  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // Animated Runner Stride Icon
  const unsigned char* runnerBmp = (runnerAnimFrame == 0) ? runner_walk1 : runner_walk2;
  display.drawBitmap(4, 16, runnerBmp, 12, 12, SSD1306_WHITE);

  // Large Steps Counter
  display.setTextSize(2);
  display.setCursor(20, 14);
  display.print(totalSteps);

  display.setTextSize(1);
  display.setCursor(20, 30);
  display.print(F("STEPS TODAY"));

  display.drawFastHLine(0, 40, 128, SSD1306_WHITE);

  // Lower Vitals Grid: Distance, Pace, Calories
  display.setCursor(2, 43);
  display.printf("DIST: %.2f km", distanceKm);

  display.setCursor(2, 53);
  display.printf("PACE: %d'%02d\"/km", walkPaceMin, walkPaceSec);

  display.setCursor(82, 43);
  display.printf("%d kcal", caloriesKcal);

  display.setCursor(82, 53);
  display.printf("%.1f C", currentTempC);
}

// ============================================================================
// Interactive Serial Command Handler
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.peek();
    if (c == ' ' || c == 'm' || c == 'M') {
      Serial.read();
      currentMode = (DisplayMode)((currentMode + 1) % 4);
      autoCycleModes = false;
      Serial.printf("[MODE] Switched to Mode %d\n", currentMode + 1);
      continue;
    } else if (c == 'w' || c == 'W') {
      Serial.read();
      totalSteps += 50;
      distanceKm = totalSteps * 0.00075f;
      caloriesKcal = (int)(totalSteps * 0.045f);
      Serial.printf("[WALK] +50 Steps! Total: %lu (%.2f km)\n", totalSteps, distanceKm);
      continue;
    } else if (c == 'a' || c == 'A') {
      Serial.read();
      autoCycleModes = !autoCycleModes;
      Serial.printf("[AUTO] Auto-cycle: %s\n", autoCycleModes ? "ON" : "OFF");
      continue;
    } else if (c == '+' || c == '=') {
      Serial.read();
      if (displayContrast <= 235) displayContrast += 20;
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(displayContrast);
      Serial.printf("[BRIGHTNESS] Contrast increased to 0x%02X\n", displayContrast);
      continue;
    } else if (c == '-' || c == '_') {
      Serial.read();
      if (displayContrast >= 30) displayContrast -= 20;
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(displayContrast);
      Serial.printf("[BRIGHTNESS] Contrast decreased to 0x%02X\n", displayContrast);
      continue;
    } else if (c >= '1' && c <= '4') {
      Serial.read();
      currentMode = (DisplayMode)(c - '1');
      autoCycleModes = false;
      Serial.printf("[MODE] Jumped to Mode %d\n", currentMode + 1);
      continue;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.startsWith("time ")) {
      int h, m, s;
      if (sscanf(line.c_str() + 5, "%d:%d:%d", &h, &m, &s) == 3) {
        curHours = h; curMinutes = m; curSeconds = s;
        Serial.printf("[TIME] Clock set to %02d:%02d:%02d\n", curHours, curMinutes, curSeconds);
      }
    } else if (line.startsWith("wifi ")) {
      int spaceIdx = line.indexOf(' ', 5);
      if (spaceIdx > 5) {
        wifiSSID = line.substring(5, spaceIdx);
        wifiPassword = line.substring(spaceIdx + 1);
        Serial.printf("[WIFI] Set credentials for '%s'\n", wifiSSID.c_str());
        WiFi.disconnect();
      }
    } else if (line.equalsIgnoreCase("temp")) {
      Serial.printf("[TELEMETRY] Core Temp: %.2f C (%.2f F) | Free Heap: %d B\n",
                    currentTempC, currentTempF, ESP.getFreeHeap());
    }
  }
}

// ============================================================================
// FreeRTOS Wi-Fi Background Worker (Auto-connects & syncs NTP)
// ============================================================================
void wifiBackgroundWorker(void* pvParameters) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      Serial.printf("[WIFI WORKER] Connecting to '%s'...\n", wifiSSID.c_str());
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("[WIFI WORKER] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
      } else {
        Serial.println(F("[WIFI WORKER] Connection failed. Retrying in 10s..."));
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
    } else {
      wifiConnected = true;
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
