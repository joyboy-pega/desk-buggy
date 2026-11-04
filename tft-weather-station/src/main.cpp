#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// ============================================================================
// Full-Color 1.8" TFT ST7735 Weather & Clock Station (ESP32-C3 SuperMini)
// Auto-powers down 0.96" OLED on I2C bus
// ============================================================================

// ----------------------------------------------------------------------------
// Pin Definitions for 1.8" ST7735 SPI Display on ESP32-C3 SuperMini
// ----------------------------------------------------------------------------
#define TFT_CS    7  // Chip Select
#define TFT_DC    2  // Data / Command (A0)
#define TFT_RST   1  // Reset
#define TFT_MOSI  6  // SPI MOSI (SDA)
#define TFT_SCLK  4  // SPI Clock (SCK)

// I2C Pins for 0.96" OLED (Used to turn OLED OFF)
#define OLED_SDA  8
#define OLED_SCL  9
#define OLED_ADDR 0x3C

// Color Palette (16-bit RGB565)
#define COLOR_BG          0x0000 // Deep Black
#define COLOR_CARD_BG     0x0842 // Dark Midnight Blue / Slate
#define COLOR_HEADER_BG   0x0010 // Deep Navy
#define COLOR_ACCENT_GOLD 0xFDE0 // Vivid Gold / Yellow
#define COLOR_SUN_GOLD    0xFDC0 // Golden Sun
#define COLOR_SUN_ORANGE  0xFA20 // Sun Flare Orange
#define COLOR_MOON_CYAN   0x57FF // Cool Moon Glow
#define COLOR_TEMP_AMBER  0xFE20 // Warm Temperature Amber
#define COLOR_TEXT_WHITE  0xFFFF // Crisp White
#define COLOR_TEXT_MUTED  0x9CD3 // Soft Slate Gray
#define COLOR_GREEN_OK    0x07E0 // Bright Emerald Green
#define COLOR_BLUE_WATER  0x34DF // Lush Sky Blue
#define COLOR_TEAL_WIND   0x067A // Teal Breeze
#define COLOR_RED_PIN     0xF986 // Crimson Red Pin
#define COLOR_DIVIDER     0x2124 // Subtle Divider

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ----------------------------------------------------------------------------
// Wi-Fi Credentials & Geolocation (Pune, Maharashtra, India)
// ----------------------------------------------------------------------------
String wifiSSID = "Arigatoo1";
String wifiPassword = "12345678";
volatile bool wifiConnected = false;
volatile bool triggerWeatherFetch = true;

const char* cityName = "PUNE, IN";
const float latitude = 18.5204f;
const float longitude = 73.8567f;
const long gmtOffsetSec = 19800; // IST UTC+5:30 (19800 sec)
const int daylightOffsetSec = 0;

// Shared Weather Metrics
portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
float currentTemp = 24.0f;
int currentHumidity = 62;
float currentWindSpeed = 8.5f;
int weatherCode = 0; // 0=Clear, 1-2=Partly cloudy, 3=Cloudy, 61=Rain, 95=Storm
String weatherCondition = "CLEAR SKY";
float minTemp = 17.0f;
float maxTemp = 30.0f;
unsigned long lastWeatherSuccess = 0;

// Fallback Epoch time (Nov 4, 2026, 23:54:00 IST)
const time_t FALLBACK_BASE_EPOCH = 1793840040;
unsigned long bootMillis = 0;

// Pulse heartbeat animation
bool pulseTick = false;
unsigned long lastPulseTime = 0;

// FreeRTOS Background Weather Task Handle
TaskHandle_t weatherTaskHandle = NULL;

// Forward Declarations
void shutdownOLED();
void renderFullColorDashboard(struct tm* t, bool ntpValid);
void drawWeatherIllustration(int x, int y, int code, bool isDay);
void drawSunIllustration(int cx, int cy);
void drawMoonIllustration(int cx, int cy);
void drawCloudIllustration(int cx, int cy);
void drawRainIllustration(int cx, int cy);
void weatherWorkerTask(void* pvParameters);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  bootMillis = millis();

  Serial.println(F("\n=========================================="));
  Serial.println(F(" 1.8\" TFT COLOR WEATHER & CLOCK STATION"));
  Serial.println(F("      ESP32-C3 SuperMini (PUNE, INDIA)"));
  Serial.println(F("=========================================="));

  // 1. Immediately shut down / power down 0.96" OLED display on I2C bus
  shutdownOLED();

  // 2. Initialize 1.8" ST7735 Full Color SPI Display (128x160)
  Serial.println(F("[TFT] Initializing ST7735 Color Display (128x160)..."));
  tft.initR(INITR_BLACKTAB); // Standard 1.8" TFT ST7735 BlackTab
  tft.setRotation(0);        // Portrait 128x160
  tft.fillScreen(COLOR_BG);

  // Splash Screen on TFT
  tft.fillRect(10, 40, 108, 80, COLOR_CARD_BG);
  tft.drawRect(10, 40, 108, 80, COLOR_ACCENT_GOLD);
  tft.setTextColor(COLOR_ACCENT_GOLD);
  tft.setTextSize(1);
  tft.setCursor(20, 52);
  tft.print(F("PUNE STATION"));
  tft.setTextColor(COLOR_TEXT_WHITE);
  tft.setCursor(20, 68);
  tft.print(F("1.8\" COLOR TFT"));
  tft.setTextColor(COLOR_GREEN_OK);
  tft.setCursor(20, 84);
  tft.print(F("OLED -> POWERED OFF"));
  tft.setTextColor(COLOR_TEXT_MUTED);
  tft.setCursor(20, 100);
  tft.print(F("CONNECTING..."));
  delay(1200);
  tft.fillScreen(COLOR_BG);

  // 3. Spawn FreeRTOS background task for Wi-Fi & Open-Meteo HTTP fetch
  xTaskCreate(
    weatherWorkerTask,
    "WeatherWorker",
    8192,
    NULL,
    1,
    &weatherTaskHandle
  );

  Serial.println(F("[OK] Full-Color TFT Weather Station Initialized!"));
}

// ============================================================================
// Main UI Loop (Render 1.8" Color TFT at smooth 30 FPS)
// ============================================================================
void loop() {
  unsigned long frameStart = micros();

  // Instant non-blocking time retrieval
  time_t nowEpoch = time(nullptr);
  bool ntpValid = (nowEpoch > 100000000);
  struct tm localTime;

  if (ntpValid) {
    localtime_r(&nowEpoch, &localTime);
  } else {
    time_t simulated = FALLBACK_BASE_EPOCH + ((millis() - bootMillis) / 1000);
    localtime_r(&simulated, &localTime);
  }

  // Heartbeat pulse every 500ms
  if (millis() - lastPulseTime >= 500) {
    pulseTick = !pulseTick;
    lastPulseTime = millis();
  }

  // Render Full-Color TFT Dashboard (128x160)
  renderFullColorDashboard(&localTime, ntpValid);

  // Enforce locked ~30 FPS frame rate
  unsigned long frameDuration = micros() - frameStart;
  const unsigned long TARGET_FRAME_US = 33333;
  if (frameDuration < TARGET_FRAME_US) {
    delayMicroseconds(TARGET_FRAME_US - frameDuration);
  }
}

// ============================================================================
// 1.8" Full-Color Dashboard Renderer (128 x 160)
// ============================================================================
void renderFullColorDashboard(struct tm* t, bool ntpValid) {
  // --------------------------------------------------------------------------
  // Header: Location Pin, City, Wi-Fi Badge (Y=0 to 18)
  // --------------------------------------------------------------------------
  tft.fillRect(0, 0, 128, 18, COLOR_HEADER_BG);

  // Red Location Pin icon (drawn with geometric primitives)
  tft.fillCircle(6, 8, 3, COLOR_RED_PIN);
  tft.fillTriangle(3, 8, 9, 8, 6, 14, COLOR_RED_PIN);
  tft.fillCircle(6, 8, 1, COLOR_HEADER_BG);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_WHITE);
  tft.setCursor(14, 5);
  tft.print(cityName);

  // Wi-Fi Status Indicator
  if (wifiConnected) {
    tft.fillCircle(84, 8, 3, COLOR_GREEN_OK);
    tft.setTextColor(COLOR_GREEN_OK);
    tft.setCursor(92, 5);
    tft.print(F("ONLINE"));
  } else {
    if (pulseTick) {
      tft.fillCircle(84, 8, 3, COLOR_SUN_ORANGE);
      tft.setTextColor(COLOR_SUN_ORANGE);
      tft.setCursor(92, 5);
      tft.print(F("WIFI..."));
    } else {
      tft.fillRect(80, 2, 48, 14, COLOR_HEADER_BG);
    }
  }

  // Golden separator line
  tft.drawFastHLine(0, 18, 128, COLOR_ACCENT_GOLD);

  // --------------------------------------------------------------------------
  // Section 1: Weather Illustration & Temperature Card (Y=21 to 68)
  // --------------------------------------------------------------------------
  tft.fillRoundRect(2, 21, 124, 48, 4, COLOR_CARD_BG);
  tft.drawRoundRect(2, 21, 124, 48, 4, COLOR_DIVIDER);

  bool isDay = (t->tm_hour >= 6 && t->tm_hour < 18);
  drawWeatherIllustration(4, 25, weatherCode, isDay);

  // Large Vivid Temperature Readout
  tft.setTextSize(3);
  tft.setTextColor(COLOR_TEMP_AMBER);
  tft.setCursor(54, 26);
  tft.print((int)currentTemp);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_ACCENT_GOLD);
  tft.print((char)247);
  tft.setTextSize(2);
  tft.print(F("C"));

  // Weather Condition Sub-Text
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_WHITE);
  tft.setCursor(54, 52);
  tft.print(weatherCondition.substring(0, 10));

  // --------------------------------------------------------------------------
  // Section 2: Atmosphere Vitals Grid (Y=72 to 110)
  // --------------------------------------------------------------------------
  // Left Box: Humidity (Y=72, X=2, W=60, H=38)
  tft.fillRoundRect(2, 72, 60, 38, 4, COLOR_CARD_BG);
  tft.drawRoundRect(2, 72, 60, 38, 4, COLOR_DIVIDER);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_BLUE_WATER);
  tft.setCursor(6, 76);
  tft.print(F("HUMIDITY"));

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT_WHITE);
  tft.setCursor(6, 88);
  tft.print(currentHumidity);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_BLUE_WATER);
  tft.print(F("%"));

  // Blue Humidity Mini-Bar
  tft.drawRect(6, 102, 50, 4, COLOR_DIVIDER);
  int humBarW = (currentHumidity * 48) / 100;
  if (humBarW > 48) humBarW = 48;
  tft.fillRect(7, 103, humBarW, 2, COLOR_BLUE_WATER);

  // Right Box: Wind & Core Temp (Y=72, X=66, W=60, H=38)
  tft.fillRoundRect(66, 72, 60, 38, 4, COLOR_CARD_BG);
  tft.drawRoundRect(66, 72, 60, 38, 4, COLOR_DIVIDER);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEAL_WIND);
  tft.setCursor(70, 76);
  tft.print(F("WIND SPD"));

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT_WHITE);
  tft.setCursor(70, 88);
  tft.print((int)currentWindSpeed);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEAL_WIND);
  tft.print(F("km"));

  // Temperature Range
  tft.setCursor(70, 101);
  tft.setTextColor(COLOR_TEXT_MUTED);
  tft.printf("%d%c-%d%c", (int)minTemp, (char)247, (int)maxTemp, (char)247);

  // --------------------------------------------------------------------------
  // Section 3: Digital Clock & Date Hub (Y=114 to 158)
  // --------------------------------------------------------------------------
  tft.fillRoundRect(2, 114, 124, 44, 4, COLOR_CARD_BG);
  tft.drawRoundRect(2, 114, 124, 44, 4, COLOR_ACCENT_GOLD);

  // Big Glowing Digital Clock
  tft.setTextSize(2);
  tft.setTextColor(COLOR_ACCENT_GOLD);
  tft.setCursor(18, 120);

  if (t->tm_hour < 10) tft.print('0');
  tft.print(t->tm_hour);
  tft.setTextColor(pulseTick ? COLOR_ACCENT_GOLD : COLOR_TEXT_MUTED);
  tft.print(':');
  tft.setTextColor(COLOR_ACCENT_GOLD);
  if (t->tm_min < 10) tft.print('0');
  tft.print(t->tm_min);
  tft.setTextColor(pulseTick ? COLOR_ACCENT_GOLD : COLOR_TEXT_MUTED);
  tft.print(':');
  tft.setTextColor(COLOR_ACCENT_GOLD);
  if (t->tm_sec < 10) tft.print('0');
  tft.print(t->tm_sec);

  // Full Calendar Date String (e.g. "WED, 04 NOV 2026")
  const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_WHITE);
  tft.setCursor(14, 142);
  tft.printf("%s, %02d %s %d", days[t->tm_wday], t->tm_mday, months[t->tm_mon], t->tm_year + 1900);
}

// ============================================================================
// Procedural Weather Graphics in Vivid 16-Bit Color
// ============================================================================
void drawWeatherIllustration(int x, int y, int code, bool isDay) {
  int cx = x + 22;
  int cy = y + 22;

  if (code == 0) {
    if (isDay) drawSunIllustration(cx, cy);
    else drawMoonIllustration(cx, cy);
  } else if (code == 1 || code == 2) {
    // Partly Cloudy: Sun behind cloud
    tft.fillCircle(cx + 6, cy - 6, 8, COLOR_SUN_GOLD);
    drawCloudIllustration(cx - 2, cy + 2);
  } else if (code == 3 || code == 45 || code == 48) {
    // Overcast
    drawCloudIllustration(cx, cy);
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    // Rain
    drawCloudIllustration(cx, cy - 4);
    drawRainIllustration(cx, cy);
  } else {
    // Fallback
    if (isDay) drawSunIllustration(cx, cy);
    else drawMoonIllustration(cx, cy);
  }
}

void drawSunIllustration(int cx, int cy) {
  // Radiant Sun with 8 rays
  tft.fillCircle(cx, cy, 11, COLOR_SUN_GOLD);
  tft.drawCircle(cx, cy, 11, COLOR_SUN_ORANGE);
  // Rays
  tft.drawFastVLine(cx, cy - 18, 5, COLOR_SUN_ORANGE);
  tft.drawFastVLine(cx, cy + 13, 5, COLOR_SUN_ORANGE);
  tft.drawFastHLine(cx - 18, cy, 5, COLOR_SUN_ORANGE);
  tft.drawFastHLine(cx + 13, cy, 5, COLOR_SUN_ORANGE);
  tft.drawLine(cx - 13, cy - 13, cx - 9, cy - 9, COLOR_SUN_ORANGE);
  tft.drawLine(cx + 9, cy + 9, cx + 13, cy + 13, COLOR_SUN_ORANGE);
  tft.drawLine(cx + 9, cy - 9, cx + 13, cy - 13, COLOR_SUN_ORANGE);
  tft.drawLine(cx - 13, cy + 13, cx - 9, cy + 9, COLOR_SUN_ORANGE);
}

void drawMoonIllustration(int cx, int cy) {
  // Glowing Crescent Moon + Stars
  tft.fillCircle(cx, cy, 12, COLOR_MOON_CYAN);
  tft.fillCircle(cx + 5, cy - 3, 10, COLOR_CARD_BG);
  // Twinkle stars
  tft.drawPixel(cx - 10, cy - 10, COLOR_ACCENT_GOLD);
  tft.drawPixel(cx + 12, cy + 6, COLOR_ACCENT_GOLD);
  tft.drawPixel(cx - 12, cy + 8, COLOR_TEXT_WHITE);
}

void drawCloudIllustration(int cx, int cy) {
  // Fluffy rounded cloud
  tft.fillCircle(cx - 6, cy + 2, 7, COLOR_TEXT_MUTED);
  tft.fillCircle(cx + 6, cy + 2, 8, COLOR_TEXT_MUTED);
  tft.fillCircle(cx, cy - 3, 9, COLOR_TEXT_WHITE);
  tft.fillRoundRect(cx - 12, cy + 2, 26, 8, 3, COLOR_TEXT_WHITE);
}

void drawRainIllustration(int cx, int cy) {
  // Diagonal raindrops
  tft.drawLine(cx - 8, cy + 12, cx - 11, cy + 17, COLOR_BLUE_WATER);
  tft.drawLine(cx - 2, cy + 12, cx - 5, cy + 17, COLOR_BLUE_WATER);
  tft.drawLine(cx + 4, cy + 12, cx + 1, cy + 17, COLOR_BLUE_WATER);
  tft.drawLine(cx + 10, cy + 12, cx + 7, cy + 17, COLOR_BLUE_WATER);
}

// ============================================================================
// Turn OLED Display Completely OFF
// ============================================================================
void shutdownOLED() {
  Wire.begin(OLED_SDA, OLED_SCL);
  Adafruit_SSD1306 oled(128, 64, &Wire, -1);
  if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oled.clearDisplay();
    oled.display();
    oled.ssd1306_command(SSD1306_DISPLAYOFF); // Power down OLED panel
    Serial.println(F("[OLED] Successfully turned OFF (Display powered down)."));
  } else {
    Serial.println(F("[OLED] No OLED active on I2C bus (already off)."));
  }
}

// ============================================================================
// FreeRTOS Background Weather Task (Wi-Fi + Open-Meteo)
// ============================================================================
void weatherWorkerTask(void* pvParameters) {
  unsigned long lastFetchAttempt = 0;

  while (true) {
    // 1. Connect Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      Serial.printf("[WIFI] Connecting to '%s'...\n", wifiSSID.c_str());
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

      int waitSec = 0;
      while (WiFi.status() != WL_CONNECTED && waitSec < 15) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        waitSec++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
        triggerWeatherFetch = true;
      } else {
        Serial.println(F("[WIFI] Connection timeout. Retrying in 10s..."));
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
    } else {
      wifiConnected = true;
    }

    // 2. Fetch Live Pune Weather
    if (WiFi.status() == WL_CONNECTED) {
      if (triggerWeatherFetch || (millis() - lastFetchAttempt > 600000)) { // 10 min
        triggerWeatherFetch = false;
        lastFetchAttempt = millis();

        HTTPClient http;
        String url = "https://api.open-meteo.com/v1/forecast?latitude=18.5204&longitude=73.8567"
                     "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=auto";

        WiFiClientSecure client;
        client.setInsecure();

        if (http.begin(client, url)) {
          http.setTimeout(5000);
          int code = http.GET();
          if (code == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload);

            if (!err) {
              portENTER_CRITICAL(&weatherMux);
              currentTemp = doc["current"]["temperature_2m"] | currentTemp;
              currentHumidity = doc["current"]["relative_humidity_2m"] | currentHumidity;
              currentWindSpeed = doc["current"]["wind_speed_10m"] | currentWindSpeed;
              weatherCode = doc["current"]["weather_code"] | weatherCode;

              if (weatherCode == 0) weatherCondition = "CLEAR SKY";
              else if (weatherCode == 1 || weatherCode == 2) weatherCondition = "PARTLY CLD";
              else if (weatherCode == 3) weatherCondition = "OVERCAST";
              else if (weatherCode == 45 || weatherCode == 48) weatherCondition = "FOGGY";
              else if (weatherCode >= 51 && weatherCode <= 67) weatherCondition = "RAIN";
              else if (weatherCode >= 71 && weatherCode <= 77) weatherCondition = "SNOW";
              else if (weatherCode >= 95) weatherCondition = "THUNDERSTORM";

              portEXIT_CRITICAL(&weatherMux);

              lastWeatherSuccess = millis();
              Serial.printf("[PUNE TFT WEATHER] Temp: %.1fC, Humidity: %d%%, Wind: %.1f km/h, %s\n",
                            currentTemp, currentHumidity, currentWindSpeed, weatherCondition.c_str());
            }
          }
          http.end();
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

