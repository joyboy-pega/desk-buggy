#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ============================================================================
// Hangry Pixel Mood Badge & Marquee Scroll for ESP32 + 1.8" ST7735 Color TFT
// Matches the viral ":( hangry" pastel pink electronic badge
// ============================================================================

// ----------------------------------------------------------------------------
// Master Pinout for ESP32-WROOM-32 (Standard 30-pin)
// ----------------------------------------------------------------------------
#define TFT_CS    5   // Chip Select (GPIO 5)
#define TFT_DC    2   // Data/Command A0 (GPIO 2)
#define TFT_RST   4   // Hardware Reset (GPIO 4)
#define TFT_MOSI 23   // Hardware VSPI MOSI / SDA (GPIO 23)
#define TFT_SCLK 18   // Hardware VSPI SCLK / SCK (GPIO 18)

// Hardware SPI ST7735 Driver (Runs at full 27 MHz for 60 FPS)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 160x128 Double-Buffered Canvas for ZERO-Flicker Silky Smooth Scrolling
GFXcanvas16* canvas = nullptr;
const int SCREEN_W = 160;
const int SCREEN_H = 128;

// ----------------------------------------------------------------------------
// Color Palette (16-bit RGB565)
// ----------------------------------------------------------------------------
// Default: Iconic Bubblegum / Pastel Neon Pink as seen in user's photo
uint16_t colorBg   = 0xFC79; // Vibrant pastel bubblegum pink
uint16_t colorText = 0x0000; // Crisp pitch black pixel font

// Preset Color Themes
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
// Marquee Message & Animation Settings
// ----------------------------------------------------------------------------
String currentMessage = ":( hangry";
int textSize = 3;             // Size 3: 24px high glyphs, perfectly proportioned
float scrollX = (float)SCREEN_W;
float scrollSpeed = 2.0f;     // Pixels per frame
bool isPaused = false;
bool autoCycleMessages = true;
int messageIndex = 0;

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
void drawPixelArtFace(GFXcanvas16* cv, int x, int y, int scale);
void renderBadgeFrame();
void handleSerialCommands();

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n========================================================");
  Serial.println("  1.8\" ST7735 TFT Hangry Mood Badge & Marquee Scroll    ");
  Serial.println("========================================================");
  Serial.printf("Pins: CS=%d, DC=%d, RST=%d, MOSI=%d, SCK=%d\n",
                TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK);

  // Initialize Hardware SPI on ESP32 VSPI pins
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(27000000); // 27 MHz high-speed SPI

  // Initialize ST7735 in 1.8" Black Tab Landscape Mode
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); // Landscape: 160 pixels wide x 128 pixels tall
  tft.fillScreen(colorBg);

  // Allocate 160x128 16-bit double-buffered canvas (40 KB RAM)
  canvas = new GFXcanvas16(SCREEN_W, SCREEN_H);
  if (canvas) {
    Serial.println("[OK] 160x128 Double-Buffered Canvas Allocated (Zero Flicker!)");
  } else {
    Serial.println("[WARN] Canvas alloc failed, falling back to direct render");
  }

  scrollX = (float)SCREEN_W;

  Serial.println("[OK] Badge Running!");
  Serial.println("Type in Serial Monitor:");
  Serial.println("  msg <text>   -> Set any custom scrolling text");
  Serial.println("  hangry       -> Reset to original ':( hangry'");
  Serial.println("  theme <1-6>  -> Change pastel background color");
  Serial.println("  speed <1-8>  -> Change scroll speed");
  Serial.println("  pause        -> Pause / Resume marquee");
  Serial.println("  size <2-5>   -> Change font scale");
}

// ============================================================================
// Main Loop (~45 FPS Silky Smooth Scroll)
// ============================================================================
void loop() {
  handleSerialCommands();

  // Render frame
  renderBadgeFrame();

  // Advance scroll position
  if (!isPaused) {
    scrollX -= scrollSpeed;

    // Calculate total message pixel width
    // In Adafruit_GFX, standard font char width is 6 * textSize (5px char + 1px space)
    int charW = 6 * textSize;
    int totalWidth = currentMessage.length() * charW;

    // When message scrolls completely off the left side of screen
    if (scrollX < -totalWidth) {
      scrollX = (float)SCREEN_W; // Wrap back to right edge

      // Auto cycle through fun mood badges
      if (autoCycleMessages) {
        messageIndex = (messageIndex + 1) % NUM_PRESET_MSGS;
        currentMessage = PRESET_MESSAGES[messageIndex];
      }
    }
  }

  delay(20); // ~50 FPS silky smooth cadence
}

// ============================================================================
// Custom Chunky Pixel Art Face: :(
// ============================================================================
void drawPixelArtFace(GFXcanvas16* cv, int x, int y, int scale) {
  // Bold square eyes
  cv->fillRect(x, y, 2 * scale, 3 * scale, colorText);
  cv->fillRect(x + 5 * scale, y, 2 * scale, 3 * scale, colorText);

  // Frowning sad mouth
  cv->fillRect(x + 1 * scale, y + 6 * scale, 5 * scale, 1 * scale, colorText);
  cv->fillRect(x, y + 7 * scale, 1 * scale, 2 * scale, colorText);
  cv->fillRect(x + 6 * scale, y + 7 * scale, 1 * scale, 2 * scale, colorText);
}

// ============================================================================
// Double-Buffered Frame Renderer (Zero Tearing / Zero Flicker)
// ============================================================================
void renderBadgeFrame() {
  if (canvas) {
    // 1. Fill background with pastel pink
    canvas->fillScreen(colorBg);

    // 2. Draw subtle retro badge border around screen
    canvas->drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, colorText);
    canvas->drawRect(3, 3, SCREEN_W - 6, SCREEN_H - 6, colorText);

    // 3. Render scrolling pixel text
    canvas->setTextSize(textSize);
    canvas->setTextColor(colorText);
    canvas->setTextWrap(false);

    int fontH = 8 * textSize;
    int textY = (SCREEN_H - fontH) / 2; // Center vertically on badge

    canvas->setCursor((int)scrollX, textY);
    canvas->print(currentMessage);

    // 4. Push full frame directly over high-speed 27MHz SPI
    tft.drawRGBBitmap(0, 0, canvas->getBuffer(), SCREEN_W, SCREEN_H);

  } else {
    // Direct rendering fallback if canvas RAM unavailable
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
      if (sz >= 1 && sz <= 5) {
        textSize = sz;
        Serial.printf("[BADGE] Text size set to: %d\n", textSize);
      }
    } else if (line.startsWith("theme ")) {
      int tIdx = line.substring(6).toInt() - 1;
      if (tIdx >= 0 && tIdx < NUM_THEMES) {
        currentThemeIdx = tIdx;
        colorBg = themes[tIdx].bg;
        colorText = themes[tIdx].text;
        Serial.printf("[BADGE] Theme switched to: %s\n", themes[tIdx].name);
      }
    } else if (line.equalsIgnoreCase("pink")) {
      colorBg = themes[0].bg; colorText = themes[0].text;
      Serial.println("[BADGE] Color set to Bubblegum Pink!");
    } else if (line.equalsIgnoreCase("tab black")) {
      tft.initR(INITR_BLACKTAB);
      tft.setRotation(1);
      Serial.println("[BADGE] Switched to Black Tab");
    } else if (line.equalsIgnoreCase("tab red")) {
      tft.initR(INITR_REDTAB);
      tft.setRotation(1);
      Serial.println("[BADGE] Switched to Red Tab");
    } else if (line.equalsIgnoreCase("tab green")) {
      tft.initR(INITR_GREENTAB);
      tft.setRotation(1);
      Serial.println("[BADGE] Switched to Green Tab");
    }
  }
}

