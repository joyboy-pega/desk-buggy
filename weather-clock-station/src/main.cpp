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
// Ultra-Responsive Smart Weather & Clock Station (ESP32-C3 SuperMini)
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
volatile bool wifiConnected = false;
volatile bool triggerWeatherFetch = false;

// Geolocation & City (Default: Pune, Maharashtra, India)
String cityName = "PUNE";
float latitude = 18.5204f;
float longitude = 73.8567f;
long gmtOffsetSec = 19800; // IST UTC+5:30 (19800 sec)
int daylightOffsetSec = 0;

// Weather Metrics (Thread-safe shared state)
portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
float currentTemp = 23.5f;
int currentHumidity = 58;
float currentWindSpeed = 9.8f;
int weatherCode = 0; // Clear Sky
String weatherCondition = "CLEAR SKY";
float minTemp = 17.0f;
float maxTemp = 30.5f;
unsigned long lastWeatherSuccess = 0;

// Temperature unit: 'C' or 'F'
char tempUnit = 'C';

// Internal Core Temp
float coreTemp = 36.0f;

// Fallback Epoch time: Nov 4, 2026, 23:20:00 (IST)
const time_t FALLBACK_BASE_EPOCH = 1793838000;
unsigned long bootMillis = 0;

// UI Display Mode & Smooth Horizontal Slide Animation
int currentCard = 1;      // 1=Main, 2=Vitals, 3=Celestial
int targetCard = 1;
float slideOffset = 0.0f; // Animated offset in pixels (-128 to 128)
bool isTransitioning = false;
unsigned long lastAutoCycle = 0;
bool autoCycleEnabled = true;

// FPS Counter & Responsiveness Heartbeat
unsigned long lastFpsTime = 0;
int frameCount = 0;
int liveFps = 60;
bool heartbeatDot = false;

// Non-blocking Serial buffer
char serialInputBuffer[96];
uint8_t serialBufferIdx = 0;

// FreeRTOS Background Weather Task Handle
TaskHandle_t weatherTaskHandle = NULL;

// ----------------------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------------------
void drawCard(int cardIdx, int xOffset);
void renderMainDashboard(int xOffset, struct tm* t, bool ntpValid);
void renderMetricsCard(int xOffset);
void renderCelestialCard(int xOffset, struct tm* t);
void weatherWorkerTask(void* pvParameters);
void handleSerialCommandsNonBlocking();
void processCommand(const String& cmd);
void switchCard(int newCard);
const unsigned char* getWeatherIcon(int code, bool isDay);

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  bootMillis = millis();

  // Non-blocking serial timeout
  Serial.setTimeout(10);

  Serial.println(F("\n=========================================="));
  Serial.println(F(" ULTRA-RESPONSIVE WEATHER & CLOCK STATION"));
  Serial.println(F("       ESP32-C3 RISC-V 160MHz (PUNE)"));
  Serial.println(F("=========================================="));

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERROR] SSD1306 OLED initialization failed!"));
    while (1) delay(100);
  }

  // Maximize I2C bus to 1.0 MHz (Fast Mode Plus) for instant ~9ms frame transfers
  Wire.setClock(1000000);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Splash Screen
  display.drawBitmap(12, 16, ICON_SUN_CLOUD, 24, 24, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(44, 18);
  display.print(F("PUNE STATION"));
  display.setCursor(44, 30);
  display.print(F("FAST 60 FPS UI"));
  display.setCursor(44, 42);
  display.print(F("READY & LIVE"));
  display.display();
  delay(800);

  // Spawn FreeRTOS background task for Wi-Fi & HTTP weather requests
  // Keeps network I/O completely off the main UI rendering thread!
  xTaskCreate(
    weatherWorkerTask,
    "WeatherWorker",
    8192,
    NULL,
    1,
    &weatherTaskHandle
  );

  Serial.println(F("[OK] UI Rendering Engine Initialized at 60 FPS!"));
  Serial.println(F("Instant Key Shortcuts:"));
  Serial.println(F("  ' ' or 'm' : Next Dashboard Card"));
  Serial.println(F("  '1' / '2' / '3' : Jump directly to Card 1, 2, or 3"));
  Serial.println(F("  'u'        : Toggle Celsius / Fahrenheit"));
  Serial.println(F("  'a'        : Toggle Auto-Cycling Cards"));
  Serial.println(F("  'p'        : Select Pune preset"));
  Serial.println(F("  'wifi <SSID> <PASS>' : Connect Wi-Fi"));
  Serial.println(F("  'city <tokyo|london|ny|sf|blr|paris|pune>' : Select City"));
}

// ============================================================================
// Main UI Render Loop (Locked at Silky Smooth 60 FPS)
// ============================================================================
void loop() {
  unsigned long frameStart = micros();

  // 1. Process serial commands with zero blocking
  handleSerialCommandsNonBlocking();

  // 2. Instant Non-Blocking Time Retrieval
  time_t nowEpoch = time(nullptr);
  bool ntpValid = (nowEpoch > 100000000);
  struct tm localTimeInfo;

  if (ntpValid) {
    localtime_r(&nowEpoch, &localTimeInfo);
  } else {
    // Immediate fallback relative to boot time (zero-delay)
    time_t simulated = FALLBACK_BASE_EPOCH + ((millis() - bootMillis) / 1000);
    localtime_r(&simulated, &localTimeInfo);
  }

  // 3. Smooth EMA Core Temperature (updated dynamically)
  coreTemp = coreTemp * 0.98f + temperatureRead() * 0.02f;

  // 4. Auto-cycle timer
  if (autoCycleEnabled && !isTransitioning && millis() - lastAutoCycle > 7000) {
    switchCard((currentCard % 3) + 1);
    lastAutoCycle = millis();
  }

  // 5. Smooth Spring / Easing Slide Animation between Cards
  if (isTransitioning) {
    float step = (0.0f - slideOffset) * 0.35f;
    if (std::abs(step) < 0.8f) {
      slideOffset = 0.0f;
      isTransitioning = false;
    } else {
      slideOffset += step;
    }
  }

  // 6. Render Current UI Frame
  display.clearDisplay();

  int intOffset = (int)slideOffset;
  drawCard(currentCard, intOffset);

  // If sliding, also draw the outgoing/incoming card adjacent
  if (isTransitioning && intOffset != 0) {
    int prevCard = (intOffset > 0) ? ((currentCard == 1) ? 3 : currentCard - 1)
                                   : ((currentCard == 3) ? 1 : currentCard + 1);
    int adjacentX = (intOffset > 0) ? (intOffset - SCREEN_WIDTH) : (intOffset + SCREEN_WIDTH);
    drawCard(prevCard, adjacentX);
  }

  display.display();

  // 7. Live FPS Benchmark Calculation
  frameCount++;
  if (millis() - lastFpsTime >= 1000) {
    liveFps = frameCount;
    frameCount = 0;
    lastFpsTime = millis();
    heartbeatDot = !heartbeatDot;
  }

  // Enforce locked 60 FPS frame timing (16.6 ms per frame)
  unsigned long frameDuration = micros() - frameStart;
  const unsigned long TARGET_FRAME_US = 16666;
  if (frameDuration < TARGET_FRAME_US) {
    delayMicroseconds(TARGET_FRAME_US - frameDuration);
  }
}

// ============================================================================
// Multi-Card Dispatcher
// ============================================================================
void drawCard(int cardIdx, int xOffset) {
  time_t now = time(nullptr);
  bool ntpOk = (now > 100000000);
  struct tm t;
  if (ntpOk) {
    localtime_r(&now, &t);
  } else {
    time_t sim = FALLBACK_BASE_EPOCH + ((millis() - bootMillis) / 1000);
    localtime_r(&sim, &t);
  }

  if (cardIdx == 1) {
    renderMainDashboard(xOffset, &t, ntpOk);
  } else if (cardIdx == 2) {
    renderMetricsCard(xOffset);
  } else if (cardIdx == 3) {
    renderCelestialCard(xOffset, &t);
  }
}

// ============================================================================
// Card 1: Main Station Dashboard (City, Weather Icon, Temp, Live Clock)
// ============================================================================
void renderMainDashboard(int xOffset, struct tm* t, bool ntpValid) {
  // Top Header: Location Pin + City Name
  display.drawBitmap(xOffset + 2, 2, BADGE_PIN, 7, 9, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(xOffset + 12, 3);
  display.print(cityName);

  // Status Badge (Wi-Fi or Live Demo)
  if (wifiConnected) {
    display.drawBitmap(xOffset + 116, 2, BADGE_WIFI, 8, 8, SSD1306_WHITE);
  } else {
    display.setCursor(xOffset + 98, 3);
    display.print(F("LIVE"));
  }

  // Heartbeat pulse dot
  if (heartbeatDot) {
    display.drawPixel(xOffset + 93, 6, SSD1306_WHITE);
  }

  display.drawFastHLine(xOffset, 13, SCREEN_WIDTH, SSD1306_WHITE);

  // Weather Icon (24x24)
  bool isDay = (t->tm_hour >= 6 && t->tm_hour < 18);
  const unsigned char* icon = getWeatherIcon(weatherCode, isDay);
  display.drawBitmap(xOffset + 4, 18, icon, 24, 24, SSD1306_WHITE);

  // Large High-Contrast Temperature Readout
  display.setTextSize(2);
  display.setCursor(xOffset + 34, 18);
  float dispTemp = (tempUnit == 'F') ? (currentTemp * 1.8f + 32.0f) : currentTemp;
  display.print((int)dispTemp);
  display.setTextSize(1);
  display.print((char)247); // Degree symbol
  display.print(tempUnit);

  // Weather Condition Sub-banner
  display.setTextSize(1);
  display.setCursor(xOffset + 34, 34);
  display.print(weatherCondition.substring(0, 14));

  display.drawFastHLine(xOffset, 46, SCREEN_WIDTH, SSD1306_WHITE);

  // Bottom Banner: Day of Week & Live Blinking Clock
  const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  display.setCursor(xOffset + 4, 52);
  display.print(days[t->tm_wday]);
  display.print(F(" "));
  if (t->tm_mday < 10) display.print('0');
  display.print(t->tm_mday);

  // Precise digital clock
  display.setCursor(xOffset + 72, 52);
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
// Card 2: Environment Atmosphere & Silicon Core Vitals
// ============================================================================
void renderMetricsCard(int xOffset) {
  display.setTextSize(1);
  display.setCursor(xOffset + 4, 2);
  display.print(F("PUNE ATMOSPHERE"));

  // Live FPS in top corner
  display.setCursor(xOffset + 104, 2);
  display.print(liveFps);
  display.print(F("F"));

  display.drawFastHLine(xOffset, 12, SCREEN_WIDTH, SSD1306_WHITE);

  // 1. Humidity Percentage & Graphic Bar
  display.drawBitmap(xOffset + 4, 17, BADGE_DROP, 7, 8, SSD1306_WHITE);
  display.setCursor(xOffset + 15, 17);
  display.print(F("HUMID: "));
  display.print(currentHumidity);
  display.print(F("%"));

  display.drawRect(xOffset + 86, 17, 38, 7, SSD1306_WHITE);
  int barW = (currentHumidity * 34) / 100;
  if (barW > 34) barW = 34;
  display.fillRect(xOffset + 88, 19, barW, 3, SSD1306_WHITE);

  // 2. Wind Speed
  display.drawBitmap(xOffset + 4, 30, BADGE_WIND, 8, 8, SSD1306_WHITE);
  display.setCursor(xOffset + 15, 30);
  display.print(F("WIND:  "));
  display.print(currentWindSpeed, 1);
  display.print(F(" km/h"));

  // 3. Hardware Silicon Core Temperature
  display.drawBitmap(xOffset + 4, 43, BADGE_THERMO, 7, 8, SSD1306_WHITE);
  display.setCursor(xOffset + 15, 43);
  display.print(F("CHIP:  "));
  display.print(coreTemp, 1);
  display.print((char)247);
  display.print(F("C (C3)"));

  // Bottom Range Bar
  display.drawFastHLine(xOffset, 54, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(xOffset + 4, 56);
  display.print(F("MIN:"));
  display.print((int)minTemp);
  display.print((char)247);
  display.print(F(" MAX:"));
  display.print((int)maxTemp);
  display.print((char)247);
  display.setCursor(xOffset + 88, 56);
  display.print(F("160MHz"));
}

// ============================================================================
// Card 3: Celestial Sky Arc (Sun & Moon Tracker)
// ============================================================================
void renderCelestialCard(int xOffset, struct tm* t) {
  display.setTextSize(1);
  display.setCursor(xOffset + 4, 2);
  display.print(F("SKY TRACKER"));

  // Active City Tag
  display.setCursor(xOffset + 82, 2);
  display.print(cityName.substring(0, 7));

  display.drawFastHLine(xOffset, 12, SCREEN_WIDTH, SSD1306_WHITE);

  // Celestial Arch Dome
  display.drawCircle(xOffset + 64, 76, 50, SSD1306_WHITE);
  display.drawFastHLine(xOffset, 51, SCREEN_WIDTH, SSD1306_WHITE);

  // Progress of day
  float dayFraction = (t->tm_hour * 60 + t->tm_min) / 1440.0f;
  float angleRad = 3.14159f - (dayFraction * 3.14159f);

  int celestialX = xOffset + 64 + (int)(cos(angleRad) * 46.0f);
  int celestialY = 76 - (int)(sin(angleRad) * 46.0f);

  if (celestialY < 50) {
    if (t->tm_hour >= 6 && t->tm_hour < 18) {
      // Daytime: Solid radiant Sun orb
      display.fillCircle(celestialX, celestialY, 4, SSD1306_WHITE);
    } else {
      // Nighttime: Crescent Moon
      display.drawCircle(celestialX, celestialY, 4, SSD1306_WHITE);
      display.drawPixel(celestialX + 1, celestialY - 1, SSD1306_WHITE);
    }
  }

  // Sunrise / Sunset Labels
  display.setCursor(xOffset + 4, 55);
  display.print(F("DAWN 06:00"));
  display.setCursor(xOffset + 68, 55);
  display.print(F("DUSK 18:30"));
}

// ============================================================================
// Card Transition Controller (Smooth Animated Slide)
// ============================================================================
void switchCard(int newCard) {
  if (newCard == currentCard && !isTransitioning) return;
  int direction = (newCard > currentCard) ? 1 : -1;
  if (currentCard == 3 && newCard == 1) direction = 1;
  if (currentCard == 1 && newCard == 3) direction = -1;

  currentCard = newCard;
  slideOffset = direction * (float)SCREEN_WIDTH;
  isTransitioning = true;
  lastAutoCycle = millis();
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
// FreeRTOS Background Worker Task (Runs completely decoupled from UI!)
// ============================================================================
void weatherWorkerTask(void* pvParameters) {
  unsigned long lastFetchAttempt = 0;

  while (true) {
    if (wifiSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
      Serial.printf("[WIFI WORKER] Connecting to '%s'...\n", wifiSSID.c_str());
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

      // Wait up to 10 seconds for connection in background
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("[WIFI WORKER] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
        triggerWeatherFetch = true;
      } else {
        Serial.println(F("[WIFI WORKER] Connection failed. Will retry later."));
      }
    }

    // Periodic or Triggered Weather Fetch
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      if (triggerWeatherFetch || (millis() - lastFetchAttempt > 600000)) {
        triggerWeatherFetch = false;
        lastFetchAttempt = millis();

        HTTPClient http;
        String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
                     "&longitude=" + String(longitude, 4) +
                     "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m" +
                     "&daily=temperature_2m_max,temperature_2m_min&timezone=auto";

        WiFiClientSecure client;
        client.setInsecure();

        if (http.begin(client, url)) {
          http.setTimeout(4000); // 4-second timeout max in background
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

              if (doc["daily"]["temperature_2m_min"].size() > 0) {
                minTemp = doc["daily"]["temperature_2m_min"][0];
                maxTemp = doc["daily"]["temperature_2m_max"][0];
              }

              if (weatherCode == 0) weatherCondition = "CLEAR SKY";
              else if (weatherCode == 1 || weatherCode == 2) weatherCondition = "PARTLY CLOUDY";
              else if (weatherCode == 3) weatherCondition = "OVERCAST";
              else if (weatherCode == 45 || weatherCode == 48) weatherCondition = "FOGGY";
              else if (weatherCode >= 51 && weatherCode <= 67) weatherCondition = "RAIN / SHOWERS";
              else if (weatherCode >= 71 && weatherCode <= 77) weatherCondition = "SNOWFALL";
              else if (weatherCode >= 95) weatherCondition = "THUNDERSTORM";

              portEXIT_CRITICAL(&weatherMux);

              lastWeatherSuccess = millis();
              Serial.printf("[BACKGROUND WEATHER OK] %s: %.1fC, Hum: %d%%, Code: %d (%s)\n",
                            cityName.c_str(), currentTemp, currentHumidity, weatherCode, weatherCondition.c_str());
            }
          }
          http.end();
        }
      }
    } else {
      wifiConnected = false;
    }

    // Sleep worker task for 1 second before next cycle
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================================
// Non-Blocking Character-by-Character Serial Command Processor
// ============================================================================
void handleSerialCommandsNonBlocking() {
  while (Serial.available()) {
    char c = Serial.read();

    // Instant single-character hotkeys
    if (serialBufferIdx == 0) {
      if (c == ' ' || c == 'm' || c == 'M') {
        switchCard((currentCard % 3) + 1);
        autoCycleEnabled = false;
        Serial.printf("[CARD] Switched to Card %d\n", currentCard);
        continue;
      } else if (c == '1') {
        switchCard(1); autoCycleEnabled = false;
        Serial.println(F("[CARD] Switched to Card 1 (Main)"));
        continue;
      } else if (c == '2') {
        switchCard(2); autoCycleEnabled = false;
        Serial.println(F("[CARD] Switched to Card 2 (Vitals)"));
        continue;
      } else if (c == '3') {
        switchCard(3); autoCycleEnabled = false;
        Serial.println(F("[CARD] Switched to Card 3 (Celestial)"));
        continue;
      } else if (c == 'u' || c == 'U') {
        tempUnit = (tempUnit == 'C') ? 'F' : 'C';
        Serial.printf("[UNIT] Temperature unit: %c\n", tempUnit);
        continue;
      } else if (c == 'a' || c == 'A') {
        autoCycleEnabled = !autoCycleEnabled;
        Serial.printf("[AUTO] Auto-cycle: %s\n", autoCycleEnabled ? "ENABLED" : "DISABLED");
        continue;
      } else if (c == 'p' || c == 'P') {
        cityName = "PUNE"; latitude = 18.5204f; longitude = 73.8567f; gmtOffsetSec = 19800;
        triggerWeatherFetch = true;
        Serial.println(F("[CITY] Preset: PUNE, India (18.52N, 73.86E)"));
        continue;
      }
    }

    if (c == '\n' || c == '\r') {
      if (serialBufferIdx > 0) {
        serialInputBuffer[serialBufferIdx] = '\0';
        String cmd = String(serialInputBuffer);
        cmd.trim();
        processCommand(cmd);
        serialBufferIdx = 0;
      }
    } else if (serialBufferIdx < sizeof(serialInputBuffer) - 1) {
      serialInputBuffer[serialBufferIdx++] = c;
    }
  }
}

// ============================================================================
// Process String Commands
// ============================================================================
void processCommand(const String& cmd) {
  if (cmd.startsWith("wifi ")) {
    int spaceIdx = cmd.indexOf(' ', 5);
    if (spaceIdx > 5) {
      wifiSSID = cmd.substring(5, spaceIdx);
      wifiPassword = cmd.substring(spaceIdx + 1);
      Serial.printf("[WIFI] Set credentials for '%s'\n", wifiSSID.c_str());
      WiFi.disconnect();
    }
  } else if (cmd.startsWith("city ")) {
    String c = cmd.substring(5);
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
    Serial.printf("[CITY] Updated to %s\n", cityName.c_str());
    triggerWeatherFetch = true;
  } else if (cmd.startsWith("loc ")) {
    int s1 = cmd.indexOf(' ', 4);
    int s2 = cmd.indexOf(' ', s1 + 1);
    if (s1 > 4 && s2 > s1) {
      latitude = cmd.substring(4, s1).toFloat();
      longitude = cmd.substring(s1 + 1, s2).toFloat();
      cityName = cmd.substring(s2 + 1);
      cityName.toUpperCase();
      Serial.printf("[LOC] Custom location: %s (%.4f, %.4f)\n", cityName.c_str(), latitude, longitude);
      triggerWeatherFetch = true;
    }
  } else if (cmd.equalsIgnoreCase("mode") || cmd.equalsIgnoreCase("next")) {
    switchCard((currentCard % 3) + 1);
    autoCycleEnabled = false;
  } else if (cmd.equalsIgnoreCase("auto")) {
    autoCycleEnabled = !autoCycleEnabled;
    Serial.printf("[AUTO] Auto-cycle: %s\n", autoCycleEnabled ? "ENABLED" : "DISABLED");
  } else if (cmd.startsWith("unit ")) {
    char u = cmd.charAt(5);
    if (u == 'c' || u == 'C') tempUnit = 'C';
    if (u == 'f' || u == 'F') tempUnit = 'F';
    Serial.printf("[UNIT] Temperature unit: %c\n", tempUnit);
  }
}
