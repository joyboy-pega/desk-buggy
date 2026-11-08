#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA 8
#define I2C_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

uint8_t oledAddress = 0x3C;
bool oledFound = false;

// I2C Scanner to detect OLED address
uint8_t scanI2C(int sda, int scl) {
  Wire.begin(sda, scl);
  delay(100);
  Serial.printf("[I2C SCAN] Checking SDA=%d, SCL=%d...\n", sda, scl);

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(">>> Found I2C device at 0x%02X!\n", addr);
      return addr;
    }
  }
  return 0;
}

// Draw Baymax Eyes Animation on 128x64 OLED
// Left eye center: (42, 32), Right eye center: (86, 32), Radius: 10
void drawBaymax(int eyeOffsetX, int eyeOffsetY, int blinkHeight) {
  display.clearDisplay();

  int leftX = 42 + eyeOffsetX;
  int rightX = 86 + eyeOffsetX;
  int eyeY = 32 + eyeOffsetY;
  int radius = 9;

  if (blinkHeight >= radius) {
    // Fully open eyes
    display.fillCircle(leftX, eyeY, radius, SSD1306_WHITE);
    display.fillCircle(rightX, eyeY, radius, SSD1306_WHITE);
    // Connecting bar between eyes
    display.fillRect(leftX, eyeY - 2, rightX - leftX, 4, SSD1306_WHITE);
  } else if (blinkHeight > 0) {
    // Eyelids closing
    display.fillRoundRect(leftX - radius, eyeY - blinkHeight, radius * 2, blinkHeight * 2, 3, SSD1306_WHITE);
    display.fillRoundRect(rightX - radius, eyeY - blinkHeight, radius * 2, blinkHeight * 2, 3, SSD1306_WHITE);
    display.fillRect(leftX, eyeY - 1, rightX - leftX, 3, SSD1306_WHITE);
  } else {
    // Closed (slits)
    display.drawLine(leftX - radius, eyeY, rightX + radius, eyeY, SSD1306_WHITE);
    display.drawLine(leftX - radius, eyeY + 1, rightX + radius, eyeY + 1, SSD1306_WHITE);
  }

  // Small status bar at bottom
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 56);
  display.print("ESP32-C3 OLED OK");

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32-C3 SuperMini 0.96\" OLED (SSD1306) Display Test  ");
  Serial.println("========================================================");
  Serial.printf("Configured Pins: SDA=GPIO %d, SCL=GPIO %d\n", I2C_SDA, I2C_SCL);

  // 1. Scan with SDA=8, SCL=9
  oledAddress = scanI2C(I2C_SDA, I2C_SCL);

  // If not found, automatically test swapped SDA=9, SCL=8
  if (oledAddress == 0) {
    Serial.println("[AUTO-TEST] Testing swapped SDA=9, SCL=8...");
    uint8_t swapAddr = scanI2C(9, 8);
    if (swapAddr != 0) {
      Serial.println(">>> SCL and SDA were swapped in wiring, auto-corrected!");
      oledAddress = swapAddr;
      Wire.begin(9, 8);
    } else {
      Wire.begin(I2C_SDA, I2C_SCL);
      oledAddress = 0x3C; // fallback default
    }
  }

  // Initialize SSD1306
  if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddress)) {
    Serial.printf("[ERROR] SSD1306 allocation failed at address 0x%02X!\n", oledAddress);
    Serial.println("Check 3.3V power, GND, SDA (pin 8), and SCL (pin 9).");
    oledFound = false;
  } else {
    Serial.printf("[SUCCESS] SSD1306 128x64 OLED initialized at 0x%02X!\n", oledAddress);
    oledFound = true;
    display.clearDisplay();
    display.display();
  }
}

int animState = 0;
unsigned long lastAnim = 0;

void loop() {
  if (!oledFound) {
    Serial.println("[WAITING] OLED display not detected on I2C bus (SDA=8, SCL=9).");
    delay(2000);
    return;
  }

  // Baymax Life-like Animation Sequence
  // State 0: Normal Eyes
  drawBaymax(0, 0, 9);
  delay(1800);

  // Blink down
  drawBaymax(0, 0, 5);
  delay(30);
  drawBaymax(0, 0, 0); // closed
  delay(80);
  drawBaymax(0, 0, 5);
  delay(30);
  drawBaymax(0, 0, 9); // open
  delay(1200);

  // Look Left
  drawBaymax(-6, 0, 9);
  delay(1400);

  // Look Right
  drawBaymax(6, 0, 9);
  delay(1400);

  // Double Blink
  drawBaymax(0, 0, 0);
  delay(70);
  drawBaymax(0, 0, 9);
  delay(100);
  drawBaymax(0, 0, 0);
  delay(70);
  drawBaymax(0, 0, 9);
  delay(1500);
}
