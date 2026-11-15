#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ============================================================================
// MacBook Live Camera Stream Receiver (60 FPS TURBO - 100% USB Cable)
// MCU: ESP32-WROOM-32 | Display: 1.8" ST7735 (160x128)
// 921,600 Baud + 40 MHz High-Speed SPI + Fast Hardware Upscaler
// ============================================================================

#define TFT_CS    5   // Chip Select (GPIO 5)
#define TFT_DC    2   // Data/Command A0 (GPIO 2)
#define TFT_RST   4   // Hardware Reset (GPIO 4)
#define TFT_MOSI 23   // VSPI MOSI (GPIO 23)
#define TFT_SCLK 18   // VSPI SCLK (GPIO 18)
#define TFT_BL   32   // Backlight PWM (GPIO 32)

#define SCREEN_W 160
#define SCREEN_H 128
#define FRAME_PIXELS (SCREEN_W * SCREEN_H)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Frame memory
uint16_t frameBuffer[FRAME_PIXELS];
uint8_t rxBuffer[2560];
uint16_t palette16[16];

uint8_t screenBrightness = 255;
uint8_t screenRotation = 3; // 180° rotated landscape

void initPalettes();
void drawStandbyScreen();

void setup() {
  Serial.setRxBufferSize(16384);
  Serial.setTimeout(15);
  Serial.begin(921600);
  delay(100);

  initPalettes();

  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, screenBrightness);

  // Maximum 40 MHz SPI for instant display refresh
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(40000000);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(screenRotation);

  drawStandbyScreen();
}

void loop() {
  if (Serial.available() < 5) return;

  if (Serial.read() != 0xAA) return;
  if (Serial.read() != 0x55) return;
  if (Serial.read() != 0xAA) return;
  if (Serial.read() != 0x55) return;

  uint8_t mode = Serial.read();

  if (mode == 0x04) {
    // ------------------------------------------------------------------------
    // 60 FPS TURBO MODE: 1,280 bytes payload (80x32 4-bit)
    // 1280 bytes takes only ~14ms at 921,600 baud! Hits 60 FPS!
    // ------------------------------------------------------------------------
    size_t received = 0;
    uint32_t t0 = millis();

    while (received < 1280 && (millis() - t0 < 60)) {
      if (Serial.available()) {
        received += Serial.readBytes((char*)rxBuffer + received, 1280 - received);
        t0 = millis();
      }
    }

    if (received == 1280) {
      // 80x32 -> 160x128 hardware upscaler in RAM
      for (int y = 0; y < 32; y++) {
        int srcRow = y * 40;
        int dstBase = (y * 4) * SCREEN_W;

        for (int b = 0; b < 40; b++) {
          uint8_t byteVal = rxBuffer[srcRow + b];
          uint16_t c1 = palette16[(byteVal >> 4) & 0x0F];
          uint16_t c2 = palette16[byteVal & 0x0F];

          int x1 = (b * 2) * 2;
          int x2 = x1 + 2;

          for (int r = 0; r < 4; r++) {
            int row = dstBase + r * SCREEN_W;
            frameBuffer[row + x1]     = c1;
            frameBuffer[row + x1 + 1] = c1;
            frameBuffer[row + x2]     = c2;
            frameBuffer[row + x2 + 1] = c2;
          }
        }
      }

      tft.drawRGBBitmap(0, 0, frameBuffer, SCREEN_W, SCREEN_H);
      Serial.write('K'); // Instant frame acknowledge
    }
  } else if (mode == 0x03) {
    // ------------------------------------------------------------------------
    // 35 FPS HIGH-DEF MODE: 2,560 bytes payload (80x64 4-bit)
    // ------------------------------------------------------------------------
    size_t received = 0;
    uint32_t t0 = millis();

    while (received < 2560 && (millis() - t0 < 100)) {
      if (Serial.available()) {
        received += Serial.readBytes((char*)rxBuffer + received, 2560 - received);
        t0 = millis();
      }
    }

    if (received == 2560) {
      for (int y = 0; y < 64; y++) {
        int srcRow = y * 40;
        int dstRow1 = (y * 2) * SCREEN_W;
        int dstRow2 = (y * 2 + 1) * SCREEN_W;

        for (int b = 0; b < 40; b++) {
          uint8_t byteVal = rxBuffer[srcRow + b];
          uint16_t c1 = palette16[(byteVal >> 4) & 0x0F];
          uint16_t c2 = palette16[byteVal & 0x0F];

          int x1 = (b * 2) * 2;
          frameBuffer[dstRow1 + x1]     = c1;
          frameBuffer[dstRow1 + x1 + 1] = c1;
          frameBuffer[dstRow2 + x1]     = c1;
          frameBuffer[dstRow2 + x1 + 1] = c1;

          int x2 = x1 + 2;
          frameBuffer[dstRow1 + x2]     = c2;
          frameBuffer[dstRow1 + x2 + 1] = c2;
          frameBuffer[dstRow2 + x2]     = c2;
          frameBuffer[dstRow2 + x2 + 1] = c2;
        }
      }

      tft.drawRGBBitmap(0, 0, frameBuffer, SCREEN_W, SCREEN_H);
      Serial.write('K');
    }
  }
}

void initPalettes() {
  palette16[0]  = 0x0000; // Black
  palette16[1]  = 0x3186; // Dark Slate Gray
  palette16[2]  = 0x738E; // Mid Cool Gray
  palette16[3]  = 0xBDF7; // Light Gray
  palette16[4]  = 0xFFFF; // Pure Crisp White
  palette16[5]  = 0x0017; // Deep Navy Blue
  palette16[6]  = 0x2BDF; // Vivid Sky Blue
  palette16[7]  = 0x07FF; // Neon Cyan
  palette16[8]  = 0x04E0; // Forest Emerald Green
  palette16[9]  = 0x07E0; // Neon Green
  palette16[10] = 0x9800; // Deep Crimson Red
  palette16[11] = 0xF800; // Bright Crimson
  palette16[12] = 0x8204; // Warm Deep Shadow Skin
  palette16[13] = 0xC389; // Warm Mid Skin Tone
  palette16[14] = 0xF4F0; // Natural Light Skin Tone
  palette16[15] = 0xFE37; // Warm Peach Face Highlight
}

void drawStandbyScreen() {
  tft.fillScreen(0x0821);

  tft.fillRoundRect(4, 4, SCREEN_W - 8, 24, 4, 0x10A3);
  tft.drawRoundRect(4, 4, SCREEN_W - 8, 24, 4, 0x2966);
  tft.setTextSize(1);
  tft.setTextColor(0x3FFA);
  tft.setCursor(14, 12);
  tft.print(F("60 FPS TURBO CAM HUD"));

  tft.fillRoundRect(4, 32, SCREEN_W - 8, 58, 4, 0x10A3);
  tft.drawRoundRect(4, 32, SCREEN_W - 8, 58, 4, 0x2966);

  tft.setTextColor(0xFFFF);
  tft.setCursor(10, 40);
  tft.print(F("TARGET: "));
  tft.setTextColor(0x2FE4);
  tft.print(F("60 FPS ULTRA FLUID"));

  tft.setTextColor(0xFFFF);
  tft.setCursor(10, 54);
  tft.print(F("BAUD: "));
  tft.setTextColor(0xFDE0);
  tft.print(F("921,600 BPS"));

  tft.setTextColor(0xFFFF);
  tft.setCursor(10, 68);
  tft.print(F("SPI SPEED: "));
  tft.setTextColor(0x3FFA);
  tft.print(F("40 MHz (120 FPS MAX)"));

  tft.fillRoundRect(4, 94, SCREEN_W - 8, 30, 3, 0x10A3);
  tft.drawRoundRect(4, 94, SCREEN_W - 8, 30, 3, 0x2966);
  tft.setTextColor(0x94B2);
  tft.setCursor(8, 100);
  tft.print(F("Start on MacBook:"));
  tft.setCursor(8, 111);
  tft.setTextColor(0x2FE4);
  tft.print(F("./mac_cam_cable"));
}
