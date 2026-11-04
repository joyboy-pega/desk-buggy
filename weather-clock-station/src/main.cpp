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
// Smart Weather & Clock Station for ESP32-C3 SuperMini + 0.96" OLED
// ============================================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_SDA       8
#define I2C_SCL       9
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----------------------------------------------------------------------------
// Configuration & State
// ----------------------------------------------------------------------------
String wifiSSID = "";
String wifiPassword = "";
bool wifiConnected = false;
unsigned long lastWifiAttempt = 0;

// Geolocation & City
String cityName = "PUNE";
float latitude = 18.5204f;
float longitude = 73.8567f;
long gmtOffsetSec = 19800; // IST +5:30 (19800 sec)
int daylightOffsetSec = 0;

// Weather Metrics
float currentTemp = 23.5f;
int currentHumidity = 58;
float currentWindSpeed = 9.8f;
int weatherCode = 0; // Clear Sky
String weatherCondition = "CLEAR SKY";
float minTemp = 17.0f;
float maxTemp = 30.5f;
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_INTERVAL_MS = 600000; // 10 minutes

// Temperature unit: 'C' or 'F'
char tempUnit = 'C';

// Internal Core Temp
float coreTemp = 36.0f;

// Standalone Simulated Clock (if NTP not connected)
unsigned long baseEpoch = 1793836800; // Fallback time: Nov 4, 2026, 23:00:00
unsigned long baseMillis = 0;

// UI Display Mode
// 1 = Main Weather & Clock Card
// 2 = Detailed Environment Metrics
// 3 = Celestial Sun/Moon Sky Arc
int uiMode = 1;
unsigned long lastModeAutoCycle = 0;
bool autoCycleModes = true;

// ----------------------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------------------
void connectWiFi(const String& ssid, const String& pass);
void fetchWeatherData();
void updateClock();
void renderMainDashboard();
void renderMetricsCard();
void renderCelestialCard();
void handleSerialCommands();
const unsigned char* getWeatherIcon(int code, bool isDay);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 1000);

  Serial.println(F("\n=========================================="));
  Serial.println(F("     SMART WEATHER & CLOCK STATION"));
  Serial.println(F("      ESP32-C3 SuperMini (160MHz)"));
  Serial.println(F("=========================================="));

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERROR] SSD1306 OLED allocation failed!"));
    while (1) delay(100);
  }

  // Optimize I2C bus clock to 800 kHz for crisp 60 FPS transitions
  Wire.setClock(800000);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Splash Screen
  display.drawBitmap(12, 16, ICON_SUN_CLOUD, 24, 24, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(44, 18);
  display.print(F("WEATHER HUD"));
  display.setCursor(44, 30);
  display.print(F("STATION v2.0"));
  display.setCursor(44, 42);
  display.print(F("INITIALIZING"));
  display.display();
  delay(1200);

  baseMillis = millis();

  Serial.println(F("[OK] Weather HUD Ready!"));
  Serial.println(F("Commands:"));
  Serial.println(F("  'wifi <SSID> <PASSWORD>' : Connect to local Wi-Fi"));
  Serial.println(F("  'city <tokyo|london|ny|sf|blr|paris>' : Select city preset"));
  Serial.println(F("  'loc <LAT> <LON> <CITY>' : Set custom GPS coordinates"));
  Serial.println(F("  'unit <C|F>' : Toggle Celsius / Fahrenheit"));
  Serial.println(F("  'mode' : Cycle display dashboard views"));
  Serial.println(F("  'auto' : Toggle auto-cycling cards"));
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  handleSerialCommands();

  // Periodic weather update if Wi-Fi connected
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      Serial.println(F("[WIFI] Connected! Syncing NTP & Weather..."));
      configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
      fetchWeatherData();
    } else if (millis() - lastWeatherFetch > WEATHER_INTERVAL_MS) {
      fetchWeatherData();
    }
  } else {
    wifiConnected = false;
  }

  // Smooth core temp reading
  coreTemp = coreTemp * 0.95f + temperatureRead() * 0.05f;

  // Auto-cycle dashboard views every 8 seconds if enabled
  if (autoCycleModes && millis() - lastModeAutoCycle > 8000) {
    uiMode = (uiMode % 3) + 1;
    lastModeAutoCycle = millis();
  }

  display.clearDisplay();

  if (uiMode == 1) {
    renderMainDashboard();
  } else if (uiMode == 2) {
    renderMetricsCard();
  } else if (uiMode == 3) {
    renderCelestialCard();
  }

  display.display();
  delay(30); // ~33 FPS UI refresh rate
}

// ============================================================================
// UI Screen 1: Main Weather & Clock Dashboard
// ============================================================================
void renderMainDashboard() {
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 50);

  int hour = 23, minute = 0, second = 0;
  int mday = 4, month = 10, year = 2026, wday = 3;

  if (timeValid) {
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
    mday = timeinfo.tm_mday;
    month = timeinfo.tm_mon;
    year = timeinfo.tm_year + 1900;
    wday = timeinfo.tm_wday;
  } else {
    // Offline simulated clock
    unsigned long elapsedSec = (millis() - baseMillis) / 1000;
    unsigned long currentEpoch = baseEpoch + elapsedSec;
    second = currentEpoch % 60;
    minute = (currentEpoch / 60) % 60;
    hour = (currentEpoch / 3600) % 24;
  }

  // Top Status Bar: City Name & Wi-Fi badge
  display.drawBitmap(2, 2, BADGE_PIN, 7, 9, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(12, 3);
  display.print(cityName);

  if (wifiConnected) {
    display.drawBitmap(118, 2, BADGE_WIFI, 8, 8, SSD1306_WHITE);
  } else {
    display.setCursor(102, 3);
    display.print(F("DEMO"));
  }

  display.drawFastHLine(0, 13, SCREEN_WIDTH, SSD1306_WHITE);

  // Left Side: Weather Icon (24x24) & Temperature
  bool isDay = (hour >= 6 && hour < 18);
  const unsigned char* weatherIcon = getWeatherIcon(weatherCode, isDay);
  display.drawBitmap(4, 18, weatherIcon, 24, 24, SSD1306_WHITE);

  // Large Temperature readout
  display.setTextSize(2);
  display.setCursor(34, 18);
  float dispTemp = (tempUnit == 'F') ? (currentTemp * 1.8f + 32.0f) : currentTemp;
  display.print((int)dispTemp);
  display.setTextSize(1);
  display.print((char)247); // Degree symbol
  display.print(tempUnit);

  // Weather Condition Text
  display.setTextSize(1);
  display.setCursor(34, 34);
  display.print(weatherCondition.substring(0, 14));

  display.drawFastHLine(0, 46, SCREEN_WIDTH, SSD1306_WHITE);

  // Bottom Banner: Digital Clock & Date
  display.setTextSize(1);
  display.setCursor(4, 52);
  const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  display.print(days[wday]);
  display.print(F(" "));
  if (mday < 10) display.print('0');
  display.print(mday);

  // Clock centered-right
  display.setCursor(72, 52);
  if (hour < 10) display.print('0');
  display.print(hour);
  display.print((second % 2 == 0) ? ':' : ' ');
  if (minute < 10) display.print('0');
  display.print(minute);
  display.print(':');
  if (second < 10) display.print('0');
  display.print(second);
}

// ============================================================================
// UI Screen 2: Detailed Environment Metrics Card
// ============================================================================
void renderMetricsCard() {
  // Title Header
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(F("ATMOSPHERE & VITALS"));
  display.drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);

  // Metric 1: Humidity
  display.drawBitmap(4, 18, BADGE_DROP, 7, 8, SSD1306_WHITE);
  display.setCursor(16, 18);
  display.print(F("HUMIDITY: "));
  display.print(currentHumidity);
  display.print(F("%"));

  // Humidity gauge bar
  display.drawRect(86, 18, 38, 7, SSD1306_WHITE);
  int humWidth = (currentHumidity * 34) / 100;
  if (humWidth > 34) humWidth = 34;
  display.fillRect(88, 20, humWidth, 3, SSD1306_WHITE);

  // Metric 2: Wind Speed
  display.drawBitmap(4, 32, BADGE_WIND, 8, 8, SSD1306_WHITE);
  display.setCursor(16, 32);
  display.print(F("WIND: "));
  display.print(currentWindSpeed, 1);
  display.print(F(" km/h"));

  // Metric 3: Silicon Core Temp
  display.drawBitmap(4, 46, BADGE_THERMO, 7, 8, SSD1306_WHITE);
  display.setCursor(16, 46);
  display.print(F("CHIP CORE: "));
  display.print(coreTemp, 1);
  display.print((char)247);
  display.print(F("C"));

  // Bottom Range Bar (Min/Max Temp)
  display.drawFastHLine(0, 56, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(4, 57);
  display.print(F("LO:"));
  display.print((int)minTemp);
  display.print((char)247);
  display.print(F("  HI:"));
  display.print((int)maxTemp);
  display.print((char)247);
  display.setCursor(80, 57);
  display.print(F("160MHz C3"));
}

// ============================================================================
// UI Screen 3: Celestial Sun/Moon Sky Arc
// ============================================================================
void renderCelestialCard() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(F("CELESTIAL SKY ARC"));
  display.drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);

  // Draw Horizon Arc
  display.drawCircle(64, 75, 48, SSD1306_WHITE);
  display.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);

  // Calculate sun/moon position along arc based on time
  struct tm timeinfo;
  int hour = 12, min = 0;
  if (getLocalTime(&timeinfo, 20)) {
    hour = timeinfo.tm_hour;
    min = timeinfo.tm_min;
  }
  float dayProgress = (hour * 60 + min) / 1440.0f; // 0.0 to 1.0

  float angle = 3.14159f - (dayProgress * 3.14159f);
  int celestialX = 64 + (int)(cos(angle) * 44.0f);
  int celestialY = 75 - (int)(sin(angle) * 44.0f);

  if (celestialY < 50) {
    if (hour >= 6 && hour < 18) {
      // Draw Sun
      display.fillCircle(celestialX, celestialY, 4, SSD1306_WHITE);
    } else {
      // Draw Moon
      display.drawCircle(celestialX, celestialY, 4, SSD1306_WHITE);
      display.drawPixel(celestialX + 1, celestialY - 1, SSD1306_WHITE);
    }
  }

  // Sunrise / Sunset Labels
  display.setCursor(4, 55);
  display.print(F("DAWN 06:00"));
  display.setCursor(68, 55);
  display.print(F("DUSK 18:30"));
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
// Open-Meteo REST API Client
// ============================================================================
void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println(F("[API] Fetching Open-Meteo forecast..."));
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
               "&longitude=" + String(longitude, 4) +
               "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m" +
               "&daily=temperature_2m_max,temperature_2m_min&timezone=auto";

  WiFiClientSecure client;
  client.setInsecure(); // No certificate verification for fast lightweight embedded fetch

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload);

      if (!err) {
        currentTemp = doc["current"]["temperature_2m"] | currentTemp;
        currentHumidity = doc["current"]["relative_humidity_2m"] | currentHumidity;
        currentWindSpeed = doc["current"]["wind_speed_10m"] | currentWindSpeed;
        weatherCode = doc["current"]["weather_code"] | weatherCode;

        if (doc["daily"]["temperature_2m_min"].size() > 0) {
          minTemp = doc["daily"]["temperature_2m_min"][0];
          maxTemp = doc["daily"]["temperature_2m_max"][0];
        }

        // Map condition text
        if (weatherCode == 0) weatherCondition = "CLEAR SKY";
        else if (weatherCode == 1 || weatherCode == 2) weatherCondition = "PARTLY CLOUDY";
        else if (weatherCode == 3) weatherCondition = "OVERCAST";
        else if (weatherCode == 45 || weatherCode == 48) weatherCondition = "FOGGY";
        else if (weatherCode >= 51 && weatherCode <= 67) weatherCondition = "RAIN / SHOWERS";
        else if (weatherCode >= 71 && weatherCode <= 77) weatherCondition = "SNOWFALL";
        else if (weatherCode >= 95) weatherCondition = "THUNDERSTORM";

        lastWeatherFetch = millis();
        Serial.printf("[API OK] %s: %.1fC, Hum: %d%%, Code: %d (%s)\n",
                      cityName.c_str(), currentTemp, currentHumidity, weatherCode, weatherCondition.c_str());
      } else {
        Serial.printf("[API JSON Error] %s\n", err.c_str());
      }
    } else {
      Serial.printf("[API HTTP Error] Code: %d\n", httpCode);
    }
    http.end();
  }
}

// ============================================================================
// Interactive Serial Command Handler
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.startsWith("wifi ")) {
      int spaceIdx = line.indexOf(' ', 5);
      if (spaceIdx > 5) {
        wifiSSID = line.substring(5, spaceIdx);
        wifiPassword = line.substring(spaceIdx + 1);
        Serial.printf("[WIFI] Connecting to '%s'...\n", wifiSSID.c_str());
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
      }
    } else if (line.startsWith("city ")) {
      String c = line.substring(5);
      c.toLowerCase();
      if (c == "pune") {
        cityName = "PUNE"; latitude = 18.5204f; longitude = 73.8567f; gmtOffsetSec = 19800;
      } else if (c == "tokyo") {
        cityName = "TOKYO"; latitude = 35.6895f; longitude = 139.6917f; gmtOffsetSec = 32400;
      } else if (c == "london") {
        cityName = "LONDON"; latitude = 51.5074f; longitude = -0.1278f; gmtOffsetSec = 0;
      } else if (c == "ny" || c == "newyork") {
        cityName = "NEW YORK"; latitude = 40.7128f; longitude = -74.0060f; gmtOffsetSec = -18000;
      } else if (c == "sf") {
        cityName = "SAN FRANCISCO"; latitude = 37.7749f; longitude = -122.4194f; gmtOffsetSec = -28800;
      } else if (c == "blr" || c == "bengaluru") {
        cityName = "BENGALURU"; latitude = 12.9716f; longitude = 77.5946f; gmtOffsetSec = 19800;
      } else if (c == "paris") {
        cityName = "PARIS"; latitude = 48.8566f; longitude = 2.3522f; gmtOffsetSec = 3600;
      }
      Serial.printf("[CITY] Updated to %s (Lat: %.2f, Lon: %.2f)\n", cityName.c_str(), latitude, longitude);
      if (WiFi.status() == WL_CONNECTED) {
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
        fetchWeatherData();
      }
    } else if (line.startsWith("loc ")) {
      // loc LAT LON NAME
      int s1 = line.indexOf(' ', 4);
      int s2 = line.indexOf(' ', s1 + 1);
      if (s1 > 4 && s2 > s1) {
        latitude = line.substring(4, s1).toFloat();
        longitude = line.substring(s1 + 1, s2).toFloat();
        cityName = line.substring(s2 + 1);
        cityName.toUpperCase();
        Serial.printf("[LOC] Custom location set: %s (%.4f, %.4f)\n", cityName.c_str(), latitude, longitude);
        if (WiFi.status() == WL_CONNECTED) fetchWeatherData();
      }
    } else if (line.equalsIgnoreCase("mode")) {
      uiMode = (uiMode % 3) + 1;
      autoCycleModes = false;
      Serial.printf("[UI] Manually switched to View %d\n", uiMode);
    } else if (line.equalsIgnoreCase("auto")) {
      autoCycleModes = !autoCycleModes;
      Serial.printf("[UI] Auto-Cycle: %s\n", autoCycleModes ? "ON" : "OFF");
    } else if (line.startsWith("unit ")) {
      char u = line.charAt(5);
      if (u == 'c' || u == 'C') tempUnit = 'C';
      if (u == 'f' || u == 'F') tempUnit = 'F';
      Serial.printf("[UNIT] Temperature unit set to %c\n", tempUnit);
    }
  }
}

