#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ============================================================================
// Hangry Pixel Mood Badge & Web Controller for ESP32 + 1.8" ST7735 Color TFT
// Wireless Web Control + USB Serial Control + 60 FPS Double-Buffered Marquee
// ============================================================================

// ----------------------------------------------------------------------------
// Master Pinout for ESP32-WROOM-32 (Standard 30-pin)
// ----------------------------------------------------------------------------
#define TFT_CS    5   // Chip Select (GPIO 5)
#define TFT_DC    2   // Data/Command A0 (GPIO 2)
#define TFT_RST   4   // Hardware Reset (GPIO 4)
#define TFT_MOSI 23   // Hardware VSPI MOSI / SDA (GPIO 23)
#define TFT_SCLK 18   // Hardware VSPI SCLK / SCK (GPIO 18)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 160x128 Double-Buffered Canvas for ZERO-Flicker Silky Smooth Scrolling
GFXcanvas16* canvas = nullptr;
const int SCREEN_W = 160;
const int SCREEN_H = 128;

// ----------------------------------------------------------------------------
// Wi-Fi Credentials & Built-In Web Server
// ----------------------------------------------------------------------------
const char* wifiSSID = "Arigatoo1";
const char* wifiPassword = "12345678";
const char* apSSID = "ESP32-Hangry-Badge";
WebServer server(80);
String currentIP = "";

// ----------------------------------------------------------------------------
// Color Palette (16-bit RGB565)
// ----------------------------------------------------------------------------
uint16_t colorBg   = 0xFC79; // Vibrant pastel bubblegum pink
uint16_t colorText = 0x0000; // Crisp pitch black pixel font

struct Theme {
  const char* name;
  uint16_t bg;
  uint16_t text;
};
Theme themes[] = {
  {"Pink (Original)",   0xFC79, 0x0000}, // Pastel Bubblegum Pink
  {"Lilac / Lavender",  0xD57F, 0x18C3}, // Cute Soft Lavender
  {"Cyber Cyan",        0x47FD, 0x0000}, // Electric Neon Cyan
  {"Matcha Mint",       0x9730, 0x0000}, // Soft Pastel Mint Green
  {"Neon Lemon",        0xFFE0, 0x0000}, // Vivid Cyber Yellow
  {"OLED Pitch Dark",   0x0000, 0xFD5B}  // Inverted Black with Pink Glow
};
const int NUM_THEMES = sizeof(themes) / sizeof(themes[0]);
int currentThemeIdx = 0;

// ----------------------------------------------------------------------------
// Marquee Message & Dynamics
// ----------------------------------------------------------------------------
String currentMessage = ":( hangry";
int textSize = 5;             // Size 5: 40px tall bold & chunky pixel glyphs!
float scrollX = (float)SCREEN_W;
float scrollSpeed = 2.5f;     // Pixels per frame
bool isPaused = false;
bool autoCycleMessages = true;
int messageIndex = 0;
uint8_t screenRotation = 3;   // Default: 180° rotated landscape

const char* PRESET_MESSAGES[] = {
  ":( hangry",
  ">:O need coffee",
  ":3 feed me snacks",
  "x_x low battery",
  "(•_•) do not disturb",
  "(>_<) so sleepy...",
  "<3 loading love 99%"
};
const int NUM_PRESET_MSGS = sizeof(PRESET_MESSAGES) / sizeof(PRESET_MESSAGES[0]);

// ----------------------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------------------
void renderBadgeFrame();
void handleSerialCommands();
void setupWebServer();
uint16_t hexToRGB565(const String& hex);

// HTML Web Page hosted directly by the ESP32
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Badge Controller</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #121216; color: #fff; margin: 0; padding: 16px; text-align: center; }
    .card { background: #1c1c24; max-width: 420px; margin: 0 auto; padding: 20px; border-radius: 20px; border: 1px solid #2e2e3a; box-shadow: 0 8px 24px rgba(0,0,0,0.5); }
    h1 { font-size: 1.3rem; margin: 0 0 4px; }
    p { font-size: 0.8rem; color: #888; margin: 0 0 16px; }
    .preview { background: #ff80df; width: 100%; height: 90px; border-radius: 12px; display: flex; align-items: center; justify-content: center; font-weight: 900; font-size: 26px; color: #000; font-family: monospace; overflow: hidden; white-space: nowrap; border: 2px solid #000; box-sizing: border-box; }
    label { display: block; font-size: 0.8rem; font-weight: 600; text-align: left; margin: 12px 0 4px; color: #bbb; }
    input[type=text] { width: 100%; box-sizing: border-box; padding: 10px; border-radius: 10px; border: 1px solid #333; background: #0c0c10; color: #fff; font-size: 1rem; font-family: monospace; }
    .row { display: flex; gap: 8px; margin-top: 10px; }
    .btn { flex: 1; padding: 10px; border-radius: 10px; border: none; font-weight: 600; cursor: pointer; font-size: 0.85rem; }
    .btn-primary { background: #ff4fa7; color: #fff; }
    .btn-secondary { background: #2a2a38; color: #ddd; }
    .color-row { display: flex; justify-content: space-between; align-items: center; background: #252532; padding: 8px 12px; border-radius: 10px; margin-top: 6px; }
    input[type=color] { border: none; width: 32px; height: 32px; border-radius: 6px; cursor: pointer; background: transparent; }
    input[type=range] { width: 100%; accent-color: #ff4fa7; margin: 6px 0; }
    .badge-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; margin-top: 8px; }
    .badge-btn { padding: 8px 4px; font-size: 0.7rem; border-radius: 8px; border: 1px solid #333; background: #22222e; color: #eee; cursor: pointer; font-family: monospace; }
  </style>
</head>
<body>
  <div class="card">
    <h1>📟 Hangry Badge Controller</h1>
    <p>Live Wireless Control for 1.8" TFT</p>

    <div id="pv" class="preview">:( hangry</div>

    <label>Custom Text</label>
    <input type="text" id="t" value=":( hangry">

    <div class="row">
      <button class="btn btn-primary" onclick="pushText()">Update Text</button>
      <button class="btn btn-secondary" onclick="togglePause()">⏸️ Pause</button>
      <button class="btn btn-secondary" onclick="flipScreen()">🔄 180°</button>
    </div>

    <div class="badge-grid">
      <button class="badge-btn" onclick="setPreset(':( hangry')">:( hangry</button>
      <button class="badge-btn" onclick="setPreset('>:O coffee')">>:O coffee</button>
      <button class="badge-btn" onclick="setPreset(':3 snack'):3 snack</button>
      <button class="badge-btn" onclick="setPreset('x_x battery')">x_x battery</button>
      <button class="badge-btn" onclick="setPreset('(>_<) sleepy')">(>_<) sleepy</button>
      <button class="badge-btn" onclick="setPreset('<3 love 99%')"><3 love 99%</button>
    </div>

    <div class="color-row">
      <span>Background Color</span>
      <input type="color" id="cBg" value="#ff80df" onchange="updateColors()">
    </div>
    <div class="color-row">
      <span>Text & Face Color</span>
      <input type="color" id="cTx" value="#000000" onchange="updateColors()">
    </div>

    <label id="lblSz">Text Size: 5 (40px)</label>
    <input type="range" id="rSz" min="2" max="7" value="5" oninput="updateSize(this.value)">

    <label id="lblSp">Scroll Speed: 2.5</label>
    <input type="range" id="rSp" min="0.5" max="8.0" step="0.5" value="2.5" oninput="updateSpeed(this.value)">
  </div>

  <script>
    function updateColors() {
      let bg = document.getElementById('cBg').value;
      let tx = document.getElementById('cTx').value;
      document.getElementById('pv').style.backgroundColor = bg;
      document.getElementById('pv').style.color = tx;
      document.getElementById('pv').style.borderColor = tx;
      fetch(`/update?bg=${encodeURIComponent(bg)}&text=${encodeURIComponent(tx)}`);
    }
    function pushText() {
      let msg = document.getElementById('t').value;
      document.getElementById('pv').textContent = msg;
      fetch(`/update?msg=${encodeURIComponent(msg)}`);
    }
    function setPreset(txt) {
      document.getElementById('t').value = txt;
      pushText();
    }
    function updateSize(v) {
      document.getElementById('lblSz').textContent = `Text Size: ${v}`;
      fetch(`/update?size=${v}`);
    }
    function updateSpeed(v) {
      document.getElementById('lblSp').textContent = `Scroll Speed: ${v}`;
      fetch(`/update?speed=${v}`);
    }
    function togglePause() {
      fetch('/update?pause=1');
    }
    function flipScreen() {
      fetch('/update?flip=1');
    }
  </script>
</body>
</html>
)rawliteral";

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n========================================================");
  Serial.println("  1.8\" ST7735 TFT Hangry Mood Badge & Web Studio        ");
  Serial.println("========================================================");

  // Initialize Hardware SPI on ESP32 VSPI pins
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(27000000); // 27 MHz high-speed SPI

  // Initialize ST7735 in 1.8" Black Tab Landscape Mode (Rotated 180°: rotation = 3)
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(screenRotation);
  tft.fillScreen(colorBg);

  // Allocate 160x128 16-bit double-buffered canvas (40 KB RAM)
  canvas = new GFXcanvas16(SCREEN_W, SCREEN_H);
  if (canvas) {
    Serial.println("[OK] 160x128 Double-Buffered Canvas Allocated (Zero Flicker!)");
  }

  scrollX = (float)SCREEN_W;

  // Initialize Wi-Fi Station & SoftAP
  Serial.printf("[WIFI] Connecting to '%s'...\n", wifiSSID);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentIP = WiFi.localIP().toString();
    Serial.println("\n[WIFI] Connected successfully!");
    Serial.printf(">>> WEB CONTROLLER URL: http://%s/\n", currentIP.c_str());
  } else {
    Serial.println("\n[WIFI] Could not connect to local Wi-Fi, starting SoftAP...");
    WiFi.softAP(apSSID);
    currentIP = WiFi.softAPIP().toString();
    Serial.printf("[WIFI] Connect to Wi-Fi '%s', then open: http://%s/\n", apSSID, currentIP.c_str());
  }

  // Setup Web Server Routes
  setupWebServer();
  server.begin();
  Serial.println("[OK] Web Server Active on Port 80!");
}

// ============================================================================
// Web Server Route Handlers
// ============================================================================
void setupWebServer() {
  // Main Web Control Page
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  // REST API Endpoint: /update
  server.on("/update", HTTP_GET, []() {
    if (server.hasArg("msg")) {
      currentMessage = server.arg("msg");
      autoCycleMessages = false;
      scrollX = (float)SCREEN_W;
      Serial.printf("[WEB] Message updated: '%s'\n", currentMessage.c_str());
    }
    if (server.hasArg("bg")) {
      colorBg = hexToRGB565(server.arg("bg"));
      Serial.printf("[WEB] Background updated: 0x%04X\n", colorBg);
    }
    if (server.hasArg("text")) {
      colorText = hexToRGB565(server.arg("text"));
      Serial.printf("[WEB] Text updated: 0x%04X\n", colorText);
    }
    if (server.hasArg("size")) {
      textSize = server.arg("size").toInt();
      if (textSize < 1) textSize = 1;
      if (textSize > 8) textSize = 8;
      Serial.printf("[WEB] Size updated: %d\n", textSize);
    }
    if (server.hasArg("speed")) {
      scrollSpeed = server.arg("speed").toFloat();
      if (scrollSpeed < 0.2f) scrollSpeed = 0.2f;
      Serial.printf("[WEB] Speed updated: %.1f\n", scrollSpeed);
    }
    if (server.hasArg("pause")) {
      isPaused = !isPaused;
      Serial.printf("[WEB] Pause toggled: %d\n", isPaused);
    }
    if (server.hasArg("flip")) {
      screenRotation = (screenRotation + 2) % 4;
      tft.setRotation(screenRotation);
      Serial.printf("[WEB] Flipped 180°. Rotation: %d\n", screenRotation);
    }

    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });
}

// Convert HTML Hex String ("#FF80DF" or "0xFC79") to 16-bit RGB565
uint16_t hexToRGB565(const String& hex) {
  if (hex.startsWith("0x") || hex.startsWith("0X")) {
    return (uint16_t)strtol(hex.c_str(), NULL, 16);
  }
  long number = strtol(hex.startsWith("#") ? hex.c_str() + 1 : hex.c_str(), NULL, 16);
  uint8_t r = (number >> 16) & 0xFF;
  uint8_t g = (number >> 8) & 0xFF;
  uint8_t b = number & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ============================================================================
// Main Loop (~50 FPS Silky Smooth Scroll)
// ============================================================================
void loop() {
  server.handleClient();
  handleSerialCommands();

  // Render frame
  renderBadgeFrame();

  // Advance marquee scroll position
  if (!isPaused) {
    scrollX -= scrollSpeed;

    int charW = 6 * textSize;
    int totalWidth = currentMessage.length() * charW;

    if (scrollX < -totalWidth) {
      scrollX = (float)SCREEN_W;
      if (autoCycleMessages) {
        messageIndex = (messageIndex + 1) % NUM_PRESET_MSGS;
        currentMessage = PRESET_MESSAGES[messageIndex];
      }
    }
  }

  delay(20);
}

// ============================================================================
// Double-Buffered Frame Renderer (Zero Tearing / Zero Flicker)
// ============================================================================
void renderBadgeFrame() {
  if (canvas) {
    // 1. Fill background with pastel color
    canvas->fillScreen(colorBg);

    // 2. Retro pixel border
    canvas->drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, colorText);
    canvas->drawRect(3, 3, SCREEN_W - 6, SCREEN_H - 6, colorText);

    // 3. Render scrolling pixel text
    canvas->setTextSize(textSize);
    canvas->setTextColor(colorText);
    canvas->setTextWrap(false);

    int fontH = 8 * textSize;
    int textY = (SCREEN_H - fontH) / 2;

    canvas->setCursor((int)scrollX, textY);
    canvas->print(currentMessage);

    // 4. Push full frame directly over 27MHz SPI
    tft.drawRGBBitmap(0, 0, canvas->getBuffer(), SCREEN_W, SCREEN_H);

  } else {
    tft.fillScreen(colorBg);
    tft.setTextSize(textSize);
    tft.setTextColor(colorText);
    tft.setTextWrap(false);
    tft.setCursor((int)scrollX, (SCREEN_H - 8 * textSize) / 2);
    tft.print(currentMessage);
  }
}

// ============================================================================
// Interactive Serial Commands (115200 baud)
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (line.startsWith("msg ")) {
      currentMessage = line.substring(4);
      autoCycleMessages = false;
      scrollX = (float)SCREEN_W;
      Serial.printf("[BADGE] Custom message set: '%s'\n", currentMessage.c_str());
    } else if (line.equalsIgnoreCase("hangry")) {
      currentMessage = ":( hangry";
      autoCycleMessages = false;
      scrollX = (float)SCREEN_W;
      Serial.println("[BADGE] Set to original ':( hangry'");
    } else if (line.equalsIgnoreCase("auto")) {
      autoCycleMessages = !autoCycleMessages;
      Serial.printf("[BADGE] Auto-cycle presets: %s\n", autoCycleMessages ? "ON" : "OFF");
    } else if (line.equalsIgnoreCase("pause") || line == " ") {
      isPaused = !isPaused;
      Serial.printf("[BADGE] Scroll: %s\n", isPaused ? "PAUSED" : "RUNNING");
    } else if (line.startsWith("speed ")) {
      float sp = line.substring(6).toFloat();
      if (sp > 0.2f && sp <= 10.0f) {
        scrollSpeed = sp;
        Serial.printf("[BADGE] Scroll speed: %.1f px/frame\n", scrollSpeed);
      }
    } else if (line.startsWith("size ")) {
      int sz = line.substring(5).toInt();
      if (sz >= 1 && sz <= 8) {
        textSize = sz;
        Serial.printf("[BADGE] Text size set to: %d\n", textSize);
      }
    } else if (line.startsWith("bg ")) {
      colorBg = hexToRGB565(line.substring(3));
      Serial.printf("[BADGE] Background Color: 0x%04X\n", colorBg);
    } else if (line.startsWith("text ")) {
      colorText = hexToRGB565(line.substring(5));
      Serial.printf("[BADGE] Text Color: 0x%04X\n", colorText);
    } else if (line.equalsIgnoreCase("flip")) {
      screenRotation = (screenRotation + 2) % 4;
      tft.setRotation(screenRotation);
      Serial.printf("[BADGE] Flipped 180°. Rotation: %d\n", screenRotation);
    } else if (line.startsWith("rotate ")) {
      int r = line.substring(7).toInt();
      if (r >= 0 && r <= 3) {
        screenRotation = r;
        tft.setRotation(screenRotation);
        Serial.printf("[BADGE] Rotation set to: %d\n", screenRotation);
      }
    } else if (line.equalsIgnoreCase("ip")) {
      Serial.printf("[BADGE] IP Address: http://%s/\n", currentIP.c_str());
    }
  }
}
