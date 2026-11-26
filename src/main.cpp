#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

// ---------- PIN DEFINITIONS (Standard ESP32 VSPI) ----------
#define TFT_CS          5    // Chip Select
#define TFT_DC          2    // Data / Command
#define TFT_RST         4    // Reset
#define TFT_MOSI       23    // VSPI MOSI (SDA)
#define TFT_SCLK       18    // VSPI SCK (SCL)
#define TFT_BL         32    // Backlight PWM pin (LED)

#define FP_RX          16    // ESP32 RX2 <- Fingerprint Sensor TX
#define FP_TX          17    // ESP32 TX2 -> Fingerprint Sensor RX

#define BUZZER_PIN     13    // Buzzer pin
#define BOOT_BUTTON     0    // On-board BOOT button for simulation

// ---------- COLOR PALETTE (RGB565) ----------
#define C_BG          0x0000 // Deep Black (#000000)
#define C_WHITE       0xFFFF // Crisp White (#FFFFFF)
#define C_GREEN       0x07E0 // Bright Emerald Green (#00FF66)
#define C_RED         0xF904 // Crimson Red (#E01A22)
#define C_CARD        0x10A2 // Slate Card (#12151A)
#define C_BORDER      0x2965 // Outline Gray (#282C34)
#define C_MUTED       0x7BEF // Subtitle Gray (#7A808C)
#define C_AMBER       0xFD20 // Amber Orange (#FFAA00)

void setup() {}
void loop() {}
