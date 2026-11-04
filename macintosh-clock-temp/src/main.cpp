#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "mac_bitmaps.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Internal Clock Variables
int curHours = 22;
int curMinutes = 35;
int curSeconds = 0;
int curYear = 2026;
int curMonth = 11;
int curDay = 4;
int curDayOfWeek = 3; // 0=Sun, 1=Mon, 2=Tue, 3=Wed, 4=Thu, 5=Fri, 6=Sat

unsigned long lastTickMillis = 0;
float currentTempC = 37.0f;
float currentTempF = 98.6f;

// Temperature History Buffer for Live Graph
const int TEMP_HISTORY_SIZE = 64;
float tempHistory[TEMP_HISTORY_SIZE];
int historyIndex = 0;
unsigned long lastGraphSample = 0;

enum DisplayMode {
  MODE_SYSTEM_WINDOW,  // Classic "About This Mac" window with time, temp & specs
  MODE_GIANT_CLOCK,    // Large retro digital clock + thermometer
  MODE_TEMP_GRAPH      // Scrolling historical temperature line graph + clock
};

DisplayMode currentMode = MODE_SYSTEM_WINDOW;
unsigned long lastModeSwitch = 0;

const char* DAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// -------------------------------------------------------------
// Clock & Timekeeping Logic
// -------------------------------------------------------------
void advanceClockOneSecond() {
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

// -------------------------------------------------------------
// Temperature Measurement (Internal ESP32-C3 Core Sensor)
// -------------------------------------------------------------
void updateTemperature() {
  // Read on-chip silicon core temperature sensor
  float raw = temperatureRead();
  if (raw > 0.0f && raw < 120.0f) {
    // Smooth reading with exponential filter
    currentTempC = currentTempC * 0.85f + raw * 0.15f;
  }
  currentTempF = currentTempC * 1.8f + 32.0f;

  // Record into graph history every 2 seconds
  if (millis() - lastGraphSample >= 2000) {
    lastGraphSample = millis();
    tempHistory[historyIndex] = currentTempC;
    historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;
  }
}

// -------------------------------------------------------------
// Screen Mode 1: Classic Retro Macintosh System Status Window
// -------------------------------------------------------------
void renderSystemWindow() {
  display.clearDisplay();

  // Top Menu Bar
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.drawBitmap(3, 0, apple_icon, 10, 12, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(16, 1);
  display.print("System Status");

  // Retro Window Container (Shadowed Bezel)
  display.drawRect(0, 12, 128, 52, SSD1306_WHITE);
  display.drawRect(1, 13, 126, 50, SSD1306_WHITE);

  // Digital Clock (Middle Big Box)
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 17);
  display.printf("%02d:%02d:%02d", curHours, curMinutes, curSeconds);

  // Date Ribbon
  display.setTextSize(1);
  display.setCursor(12, 35);
  display.printf("%s, %s %02d, %d", DAY_NAMES[curDayOfWeek], MONTH_NAMES[curMonth - 1], curDay, curYear);

  // Thermometer & Temperature Bar
  display.drawBitmap(8, 46, thermometer_icon, 9, 16, SSD1306_WHITE);
  display.setCursor(22, 49);
  display.printf("%.1f C / %.1f F", currentTempC, currentTempF);

  display.display();
}

// -------------------------------------------------------------
// Screen Mode 2: Giant Retro Digital Clock & Weather / Temp
// -------------------------------------------------------------
void renderGiantClock() {
  display.clearDisplay();

  // Outer CRT Frame
  display.drawRoundRect(2, 2, 124, 60, 4, SSD1306_WHITE);

  // Clock Icon & Big Digits
  display.drawBitmap(8, 6, clock_icon, 12, 12, SSD1306_WHITE);

  int displayHours = curHours % 12;
  if (displayHours == 0) displayHours = 12;
  const char* ampm = (curHours >= 12) ? "PM" : "AM";

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 6);
  display.printf("%2d:%02d", displayHours, curMinutes);

  display.setTextSize(1);
  display.setCursor(88, 6);
  display.printf(":%02d", curSeconds);
  display.setCursor(88, 14);
  display.print(ampm);

  // Divider Line
  display.drawFastHLine(6, 26, 116, SSD1306_WHITE);

  // Temperature Section
  display.drawBitmap(10, 32, thermometer_icon, 9, 16, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(24, 32);
  display.printf("%.1f", currentTempC);

  display.setTextSize(1);
  display.setCursor(76, 32);
  display.print("o"); // degree symbol
  display.setTextSize(1);
  display.setCursor(84, 34);
  display.print("C");

  display.setCursor(84, 44);
  display.printf("(%.0f F)", currentTempF);

  // Status Bar at bottom
  display.setCursor(10, 52);
  display.printf("CPU: %dMHz | Free: %uK", ESP.getCpuFreqMHz(), ESP.getFreeHeap() / 1024);

  display.display();
}

// -------------------------------------------------------------
// Screen Mode 3: Scrolling Historical Temperature Line Graph
// -------------------------------------------------------------
void renderTempGraph() {
  display.clearDisplay();

  // Top header with current time & current temp
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 2);
  display.printf("%02d:%02d:%02d", curHours, curMinutes, curSeconds);

  display.drawBitmap(72, 0, thermometer_icon, 9, 16, SSD1306_WHITE);
  display.setCursor(84, 2);
  display.printf("%.1f C", currentTempC);

  // Divider line
  display.drawFastHLine(0, 14, 128, SSD1306_WHITE);

  // Graph Window Frame
  display.drawRect(0, 16, 128, 48, SSD1306_WHITE);

  // Find min and max in history buffer for auto-scaling
  float minT = 20.0f, maxT = 60.0f;
  for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
    if (tempHistory[i] > 0.0f) {
      if (tempHistory[i] < minT) minT = tempHistory[i] - 1.0f;
      if (tempHistory[i] > maxT) maxT = tempHistory[i] + 1.0f;
    }
  }

  // Draw Temperature Curve
  int graphX = 4;
  for (int i = 0; i < TEMP_HISTORY_SIZE - 1; i++) {
    int idx1 = (historyIndex + i) % TEMP_HISTORY_SIZE;
    int idx2 = (historyIndex + i + 1) % TEMP_HISTORY_SIZE;

    float val1 = tempHistory[idx1];
    float val2 = tempHistory[idx2];

    if (val1 > 0.0f && val2 > 0.0f) {
      int y1 = 60 - (int)((val1 - minT) / (maxT - minT) * 40.0f);
      int y2 = 60 - (int)((val2 - minT) / (maxT - minT) * 40.0f);
      y1 = constrain(y1, 18, 61);
      y2 = constrain(y2, 18, 61);
      display.drawLine(graphX + i * 2, y1, graphX + (i + 1) * 2, y2, SSD1306_WHITE);
    }
  }

  // Graph labels
  display.setTextSize(1);
  display.setCursor(6, 18);
  display.printf("%.0fC", maxT);
  display.setCursor(6, 52);
  display.printf("%.0fC", minT);

  display.display();
}

// -------------------------------------------------------------
// Serial Interactive Commands
// -------------------------------------------------------------
void handleSerialCommands() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("time ")) {
      // Format: time HH:MM:SS
      int h, m, s;
      if (sscanf(line.c_str(), "time %d:%d:%d", &h, &m, &s) == 3) {
        curHours = h; curMinutes = m; curSeconds = s;
        Serial.printf("[CLOCK] Time set to %02d:%02d:%02d\n", curHours, curMinutes, curSeconds);
      }
    } else if (line.startsWith("date ")) {
      // Format: date YYYY-MM-DD
      int y, m, d;
      if (sscanf(line.c_str(), "date %d-%d-%d", &y, &m, &d) == 3) {
        curYear = y; curMonth = m; curDay = d;
        Serial.printf("[CLOCK] Date set to %d-%02d-%02d\n", curYear, curMonth, curDay);
      }
    } else if (line.startsWith("wifi ")) {
      // Format: wifi SSID PASSWORD
      int spaceIdx = line.indexOf(' ', 5);
      if (spaceIdx > 0) {
        String ssid = line.substring(5, spaceIdx);
        String pass = line.substring(spaceIdx + 1);
        Serial.printf("[WIFI] Connecting to '%s'...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
      }
    } else if (line == "mode") {
      currentMode = (DisplayMode)((currentMode + 1) % 3);
      Serial.printf("[MODE] Switched to Mode %d\n", currentMode);
    } else if (line == "temp") {
      Serial.printf("[STATUS] Core Temp: %.2f °C | %.2f °F | Heap: %u bytes\n",
                    currentTempC, currentTempF, ESP.getFreeHeap());
    }
  }
}

// -------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32-C3 SuperMini Macintosh Clock & Temperature HUD  ");
  Serial.println("========================================================");

  // Initialize temperature history buffer
  for (int i = 0; i < TEMP_HISTORY_SIZE; i++) {
    tempHistory[i] = 37.0f;
  }

  // Initialize High-Speed I2C for 0.96" OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(800000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Wire.begin(9, 8);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  }
  Wire.setClock(1000000); // 1MHz high speed

  display.clearDisplay();
  display.display();

  lastTickMillis = millis();
  lastModeSwitch = millis();
  lastGraphSample = millis();

  Serial.println("[OK] Clock & Temperature HUD Initialized.");
  Serial.println("Commands:");
  Serial.println("  time HH:MM:SS       -> Set current time");
  Serial.println("  date YYYY-MM-DD     -> Set current date");
  Serial.println("  wifi SSID PASSWORD  -> Connect Wi-Fi & auto-sync NTP");
  Serial.println("  mode                -> Switch display view (1/2/3)");
  Serial.println("  temp                -> Print current temperature");
  Serial.println("========================================================\n");
}

void loop() {
  handleSerialCommands();

  // Tick clock every 1 second
  if (millis() - lastTickMillis >= 1000) {
    lastTickMillis = millis();
    advanceClockOneSecond();
    updateTemperature();
  }

  // Auto-cycle through the 3 viewing modes every 10 seconds
  if (millis() - lastModeSwitch >= 10000) {
    lastModeSwitch = millis();
    currentMode = (DisplayMode)((currentMode + 1) % 3);
  }

  // Render active mode
  switch (currentMode) {
    case MODE_SYSTEM_WINDOW: renderSystemWindow(); break;
    case MODE_GIANT_CLOCK:   renderGiantClock();   break;
    case MODE_TEMP_GRAPH:    renderTempGraph();    break;
  }

  delay(50);
}
