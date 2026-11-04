#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "weather_bitmaps.h"

// ============================================================================
// Single-Screen Weather & Clock Station for Pune, India
// ESP32-C3 SuperMini + 0.96" SSD1306 I2C OLED (128x64)
// ============================================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_SDA       8
#define I2C_SCL       9
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

// Thread-safe weather metrics
portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
float currentTemp = 24.0f;
int currentHumidity = 58;
float currentWindSpeed = 10.0f;
int weatherCode = 0; // 0=Clear, 1-2=Partly cloudy, 3=Cloudy, 61=Rain, 95=Storm
String weatherCondition = "CLEAR SKY";
unsigned long lastWeatherSuccess = 0;

// Fallback base time (Nov 4, 2026, 23:32:00 IST) if NTP not yet synced
const time_t FALLBACK_BASE_EPOCH = 1793838720;
unsigned long bootMillis = 0;

// Pulse heartbeat animation
bool pulseTick = false;
unsigned long lastPulseTime = 0;

// FreeRTOS Background Weather Task Handle
TaskHandle_t weatherTaskHandle = NULL;

// Forward declarations
void renderUnifiedScreen(struct tm* t, bool ntpValid);
void weatherWorkerTask(void* pvParameters);
const unsigned char* getWeatherIcon(int code, bool isDay);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  bootMillis = millis();

  Serial.println(F("\n=========================================="));
  Serial.println(F(" SMART WEATHER & CLOCK HUD — PUNE, INDIA"));
  Serial.println(F("      ESP32-C3 SuperMini (160MHz)"));
  Serial.println(F("=========================================="));

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERROR] SSD1306 OLED initialization failed!"));
    while (1) delay(100);
  }

  // 1.0 MHz Fast Mode+ I2C bus clock for instant rendering
  Wire.setClock(1000000);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Quick Splash Screen
  display.drawBitmap(14, 18, ICON_SUN, 24, 24, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(44, 20);
  display.print(F("PUNE STATION"));
  display.setCursor(44, 32);
  display.print(F("CONNECTING..."));
  display.setCursor(44, 44);
  display.print(wifiSSID);
  display.display();
  delay(1000);

  // Spawn FreeRTOS background task for Wi-Fi & HTTP weather requests
  xTaskCreate(
    weatherWorkerTask,
    "WeatherWorker",
    8192,
    NULL,
    1,
    &weatherTaskHandle
  );

  Serial.printf("[WIFI] Target SSID: '%s'\n", wifiSSID.c_str());
}

// ============================================================================
// Main Loop (Single Fixed Unified Screen at Solid 60 FPS)
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

  // Render the single unified screen
  display.clearDisplay();
  renderUnifiedScreen(&localTime, ntpValid);
  display.display();

  // Handle any incoming Serial Wi-Fi changes if needed
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("wifi ")) {
      int spaceIdx = cmd.indexOf(' ', 5);
      if (spaceIdx > 5) {
        wifiSSID = cmd.substring(5, spaceIdx);
        wifiPassword = cmd.substring(spaceIdx + 1);
        Serial.printf("[WIFI] New credentials: '%s'\n", wifiSSID.c_str());
        WiFi.disconnect();
      }
    }
  }

  // Enforce locked 60 FPS frame rate
  unsigned long frameDuration = micros() - frameStart;
  const unsigned long TARGET_FRAME_US = 16666;
  if (frameDuration < TARGET_FRAME_US) {
    delayMicroseconds(TARGET_FRAME_US - frameDuration);
  }
}

// ============================================================================
// Unified Single-Screen Layout (128x64)
// ============================================================================
void renderUnifiedScreen(struct tm* t, bool ntpValid) {
  // --------------------------------------------------------------------------
  // Header: Location Pin + "PUNE, IN" + Wi-Fi Status
  // --------------------------------------------------------------------------
  display.drawBitmap(2, 2, BADGE_PIN, 7, 9, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(12, 3);
  display.print(cityName);

  if (wifiConnected) {
    display.drawBitmap(88, 2, BADGE_WIFI, 8, 8, SSD1306_WHITE);
    display.setCursor(98, 3);
    display.print(F("ONLINE"));
  } else {
    display.setCursor(80, 3);
    if (pulseTick) {
      display.print(F("WIFI..."));
    } else {
      display.print(F("       "));
    }
  }

  display.drawFastHLine(0, 13, SCREEN_WIDTH, SSD1306_WHITE);

  // --------------------------------------------------------------------------
  // Middle Section: Weather Icon (24x24), Temperature, Condition & Metrics
  // --------------------------------------------------------------------------
  bool isDay = (t->tm_hour >= 6 && t->tm_hour < 18);
  const unsigned char* icon = getWeatherIcon(weatherCode, isDay);
  display.drawBitmap(2, 17, icon, 24, 24, SSD1306_WHITE);

  // Large Temperature readout
  display.setTextSize(2);
  display.setCursor(30, 16);
  display.print((int)currentTemp);
  display.setTextSize(1);
  display.print((char)247);
  display.print(F("C"));

  // Condition Text
  display.setTextSize(1);
  display.setCursor(30, 33);
  display.print(weatherCondition.substring(0, 10));

  // Atmosphere Metrics Column (Right side)
  // 1. Humidity
  display.drawBitmap(92, 16, BADGE_DROP, 7, 8, SSD1306_WHITE);
  display.setCursor(102, 16);
  display.print(currentHumidity);
  display.print(F("%"));

  // 2. Wind
  display.drawBitmap(92, 28, BADGE_WIND, 8, 8, SSD1306_WHITE);
  display.setCursor(102, 28);
  display.print((int)currentWindSpeed);
  display.print(F("k"));

  display.drawFastHLine(0, 47, SCREEN_WIDTH, SSD1306_WHITE);

  // --------------------------------------------------------------------------
  // Footer: Date & Live Digital Clock (HH:MM:SS)
  // --------------------------------------------------------------------------
  const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

  display.setCursor(2, 52);
  display.print(days[t->tm_wday]);
  display.print(F(" "));
  if (t->tm_mday < 10) display.print('0');
  display.print(t->tm_mday);
  display.print(F(" "));
  display.print(months[t->tm_mon]);

  // Digital Clock with blinking colon
  display.setCursor(76, 52);
  if (t->tm_hour < 10) display.print('0');
  display.print(t->tm_hour);
  display.print((t->tm_sec % 2 == 0) ? ':' : ' ');
  if (t->tm_min < 10) display.print('0');
  display.print(t->tm_min);
  display.print(':');
  if (t->tm_sec < 10) display.print('0');
  display.print(t->tm_sec);
}

// ============================================================================
// Weather Icon Resolver
// ============================================================================
const unsigned char* getWeatherIcon(int code, bool isDay) {
  if (code == 0) {
    return isDay ? ICON_SUN : ICON_MOON;
  } else if (code == 1 || code == 2) {
    return ICON_SUN_CLOUD;
  } else if (code == 3 || code == 45 || code == 48) {
    return ICON_CLOUD;
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    return ICON_RAIN;
  } else if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
    return ICON_SNOW;
  } else if (code >= 95) {
    return ICON_THUNDER;
  }
  return ICON_SUN_CLOUD;
}

// ============================================================================
// FreeRTOS Background Weather Task (Handles Wi-Fi & Open-Meteo in background)
// ============================================================================
void weatherWorkerTask(void* pvParameters) {
  unsigned long lastFetchAttempt = 0;

  while (true) {
    // 1. Ensure Wi-Fi is connected
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
        Serial.printf("[WIFI] Connected successfully! IP: %s\n", WiFi.localIP().toString().c_str());
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
        triggerWeatherFetch = true;
      } else {
        Serial.println(F("[WIFI] Connection attempt timed out. Retrying in 10s..."));
        vTaskDelay(pdMS_TO_TICKS(10000));
        continue;
      }
    } else {
      wifiConnected = true;
    }

    // 2. Fetch Live Pune Weather from Open-Meteo
    if (WiFi.status() == WL_CONNECTED) {
      if (triggerWeatherFetch || (millis() - lastFetchAttempt > 600000)) { // 10 minutes
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
              else if (weatherCode >= 95) weatherCondition = "STORM";

              portEXIT_CRITICAL(&weatherMux);

              lastWeatherSuccess = millis();
              Serial.printf("[PUNE WEATHER OK] Temp: %.1fC, Humidity: %d%%, Wind: %.1f km/h, Condition: %s\n",
                            currentTemp, currentHumidity, currentWindSpeed, weatherCondition.c_str());
            }
          } else {
            Serial.printf("[HTTP] Error code: %d\n", code);
          }
          http.end();
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
