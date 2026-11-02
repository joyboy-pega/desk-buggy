/*
  Minecraft-style map demo for ESP32 + 1.8" TFT (ST7735, 128x160)
  ----------------------------------------------------------------
  - Generates an endless terrain on the fly (water, sand, grass, forest, stone, snow)
  - Red dot = player, centred on screen. The map scrolls around it.
  - Navigate with touch wires (bare jumper wire, just touch the end with a finger)
  - Also supports Serial keyboard control (W/A/S/D to move, Z to zoom)
  - If you don't touch anything for 5 s, the player walks by itself (demo mode)
*/

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);

const int W = 160, H = 128;       // landscape size after setRotation(1)
const uint32_t SEED = 12345;

int camX = 0, camZ = 0;           // world position of the player (blocks)
int zoom = 1;                     // blocks per pixel: 1, 2 or 4
bool dirty = true;

const uint8_t PIN_L = 13, PIN_R = 14, PIN_U = 27, PIN_D = 33, PIN_Z = 32;
int baseL, baseR, baseU, baseD, baseZ;

// ---------- noise ----------
uint32_t hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + SEED;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
float rnd(int x, int y) { return (hash2(x, y) & 0xFFFF) / 65535.0f; }

float vnoise(float x, float y) {
  int xi = floorf(x), yi = floorf(y);
  float xf = x - xi, yf = y - yi;
  xf = xf * xf * (3 - 2 * xf);
  yf = yf * yf * (3 - 2 * yf);
  float a = rnd(xi, yi),     b = rnd(xi + 1, yi);
  float c = rnd(xi, yi + 1), d = rnd(xi + 1, yi + 1);
  return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}

float height(int wx, int wz) {
  float v = 0, amp = 0.55f, f = 1.0f / 90.0f;
  for (int i = 0; i < 4; i++) {
    v += amp * vnoise(wx * f, wz * f);
    f *= 2.0f; amp *= 0.5f;
  }
  return v;   // roughly 0..1
}

uint16_t terrainColor(int wx, int wz) {
  float h = height(wx, wz);
  if (h < 0.30f) return tft.color565(20, 50, 160);      // deep water
  if (h < 0.38f) return tft.color565(40, 90, 210);      // water
  if (h < 0.41f) return tft.color565(230, 215, 150);    // sand
  if (h < 0.56f) {                                      // grass
    int s = (int)(h * 60) % 6;
    return tft.color565(70 + s * 3, 160 + s * 4, 50);
  }
  if (h < 0.66f) {                                      // forest
    bool tree = (hash2(wx / 3, wz / 3) & 7) == 0;
    return tree ? tft.color565(10, 70, 20) : tft.color565(40, 120, 40);
  }
  if (h < 0.76f) {                                      // stone
    int g = 100 + (int)((h - 0.66f) * 500);
    return tft.color565(g, g, g);
  }
  return tft.color565(245, 245, 255);                   // snow
}

// ---------- drawing ----------
void drawMap() {
  for (int y = 0; y < H; y++) {
    int wz = camZ + (y - H / 2) * zoom;
    for (int x = 0; x < W; x++) {
      int wx = camX + (x - W / 2) * zoom;
      spr.drawPixel(x, y, terrainColor(wx, wz));
    }
  }

  // player marker
  spr.fillCircle(W / 2, H / 2, 4, TFT_WHITE);
  spr.fillCircle(W / 2, H / 2, 3, TFT_RED);

  // info bar
  spr.fillRect(0, 0, W, 11, TFT_BLACK);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.setTextSize(1);
  spr.setCursor(2, 2);
  spr.printf("X:%d Z:%d  %dx", camX, camZ, zoom);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.setCursor(W - 8, 2);
  spr.print("N");

  spr.pushSprite(0, 0);
}

// ---------- touch ----------
int avgTouch(uint8_t pin) {
  long s = 0;
  for (int i = 0; i < 8; i++) s += touchRead(pin);
  return s / 8;
}
bool touched(uint8_t pin, int base) { return avgTouch(pin) < base * 7 / 10; }

unsigned long lastInput = 0, lastZoom = 0;
float autoAngle = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  // Reset display regardless of whether RST is on GPIO 4 or GPIO 25
  pinMode(4, OUTPUT);
  pinMode(25, OUTPUT);
  digitalWrite(4, LOW);
  digitalWrite(25, LOW);
  delay(20);
  digitalWrite(4, HIGH);
  digitalWrite(25, HIGH);
  delay(50);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) {
    Serial.println("Not enough RAM for sprite");
    while (1) delay(1000);
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 50);
  tft.print("Calibrating touch...");
  delay(500);
  baseL = avgTouch(PIN_L); baseR = avgTouch(PIN_R);
  baseU = avgTouch(PIN_U); baseD = avgTouch(PIN_D);
  baseZ = avgTouch(PIN_Z);
  lastInput = millis();

  Serial.println("Minecraft Map Demo Ready!");
  Serial.println("Touch wires: GPIO13=L, 14=R, 27=U, 33=D, 32=Zoom");
  Serial.println("Serial keys: W/A/S/D to move, Z to zoom");
}

void loop() {
  int step = 4 * zoom;
  bool moved = false;

  // Touch wire controls
  if (touched(PIN_L, baseL)) { camX -= step; moved = true; }
  if (touched(PIN_R, baseR)) { camX += step; moved = true; }
  if (touched(PIN_U, baseU)) { camZ -= step; moved = true; }
  if (touched(PIN_D, baseD)) { camZ += step; moved = true; }

  if (touched(PIN_Z, baseZ) && millis() - lastZoom > 400) {
    zoom = (zoom == 4) ? 1 : zoom * 2;
    lastZoom = millis();
    moved = true;
  }

  // Serial keyboard controls
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'a' || ch == 'A') { camX -= step * 2; moved = true; }
    else if (ch == 'd' || ch == 'D') { camX += step * 2; moved = true; }
    else if (ch == 'w' || ch == 'W') { camZ -= step * 2; moved = true; }
    else if (ch == 's' || ch == 'S') { camZ += step * 2; moved = true; }
    else if (ch == 'z' || ch == 'Z' || ch == ' ') {
      zoom = (zoom == 4) ? 1 : zoom * 2;
      moved = true;
    }
  }

  if (moved) lastInput = millis();

  // demo mode: wander around by itself
  if (!moved && millis() - lastInput > 5000) {
    autoAngle += 0.03f + (rnd((int)(millis() / 2000), 7) - 0.5f) * 0.05f;
    camX += (int)(cosf(autoAngle) * 3 * zoom);
    camZ += (int)(sinf(autoAngle) * 3 * zoom);
    moved = true;
  }

  if (moved || dirty) {
    drawMap();
    dirty = false;
  }
  delay(30);
}
