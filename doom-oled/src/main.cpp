#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cmath>
#include "doom_bitmaps.h"

// ============================================================================
// DOOM 60 FPS Engine for ESP32-C3 SuperMini + 0.96" I2C OLED (SSD1306)
// ============================================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_SDA       8
#define I2C_SCL       9
#define OLED_ADDR     0x3C

#define VIEW_HEIGHT   52
#define HUD_HEIGHT    12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----------------------------------------------------------------------------
// World Map & 3D Raycaster Definitions
// ----------------------------------------------------------------------------
#define MAP_W 16
#define MAP_H 16

const uint8_t worldMap[MAP_W][MAP_H] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,0,0,1,0,1,1,1,1,0,1,0,1},
  {1,0,1,0,0,0,0,0,0,0,0,1,0,1,0,1},
  {1,0,1,0,1,1,1,1,1,0,0,1,0,1,0,1},
  {1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,1},
  {1,0,0,0,1,0,0,0,1,0,1,1,1,0,0,1},
  {1,1,1,0,0,0,0,0,0,0,1,0,1,0,0,1},
  {1,0,0,0,0,1,1,0,0,0,1,0,1,0,0,1},
  {1,0,1,1,0,1,1,0,1,0,0,0,0,0,0,1},
  {1,0,1,0,0,0,0,0,1,0,1,1,1,1,0,1},
  {1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,1},
  {1,0,0,0,1,1,0,0,0,0,1,0,0,1,0,1},
  {1,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Player Position & Camera Vector
float posX = 2.5f, posY = 2.5f;
float dirX = 1.0f, dirY = 0.0f;
float planeX = 0.0f, planeY = 0.66f; // ~66 deg FOV
float moveSpeed = 0.08f;
float rotSpeed  = 0.06f;
float walkBob   = 0.0f;

// 1D Z-Buffer for 128 screen columns
float zBuffer[SCREEN_WIDTH];

// Sprites (Enemies & Props)
struct Sprite {
  float x;
  float y;
  int type;      // 0 = Imp, 1 = Barrel
  int health;    // Imp takes 2 hits
  bool alive;
  float dist;
};

#define NUM_SPRITES 6
Sprite sprites[NUM_SPRITES] = {
  {3.5f, 5.5f, 0, 2, true, 0.0f}, // Imp 1
  {8.5f, 2.5f, 0, 2, true, 0.0f}, // Imp 2
  {5.5f, 9.5f, 0, 2, true, 0.0f}, // Imp 3
  {13.5f, 3.5f, 0, 2, true, 0.0f}, // Imp 4
  {4.5f, 3.5f, 1, 1, true, 0.0f}, // Barrel 1
  {9.5f, 8.5f, 1, 1, true, 0.0f}, // Barrel 2
};

// ----------------------------------------------------------------------------
// Game State & HUD
// ----------------------------------------------------------------------------
int ammo = 50;
int health = 100;
int armor = 100;
int kills = 0;

int weaponAnimState = 0; // 0=idle, 1=flash, 2=recoil, 3=pump
int weaponAnimTimer = 0;
bool screenFlash = false;

// Doomguy face state
int faceLook = 0; // 0=center, 1=left, 2=right, 3=grin, 4=ouch
unsigned long lastFaceChange = 0;

// Autonomous Marine AI
bool autoNavMode = true;
float aiTurnTarget = 0.0f;
int aiShootCooldown = 0;

// Minimap toggle
bool showMiniMap = false;

// Operating Modes
// 1 = 3D Raycaster, 2 = Iconic DOOM Fire, 3 = Doomguy Showcase
int appMode = 1;

// ----------------------------------------------------------------------------
// PSX DOOM Fire Simulation Buffer
// ----------------------------------------------------------------------------
#define FIRE_W 128
#define FIRE_H 48
uint8_t firePixels[FIRE_W * FIRE_H];

// ----------------------------------------------------------------------------
// FPS Benchmark & Timing
// ----------------------------------------------------------------------------
unsigned long lastFpsMillis = 0;
int frameCounter = 0;
int currentFps = 60;

// ----------------------------------------------------------------------------
// Helper Prototypes
// ----------------------------------------------------------------------------
void updateAutonomousAI();
void fireShotgun();
void renderRaycaster();
void renderHUD();
void renderMiniMap();
void renderDoomFire();
void renderFaceShowcase();
void handleSerialCommands();

// ============================================================================
// Setup
// ============================================================================
void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 1000);

  Serial.println(F("\n========================================"));
  Serial.println(F("       DOOM 60 FPS OLED ENGINE"));
  Serial.println(F("     ESP32-C3 SuperMini (160MHz)"));
  Serial.println(F("========================================"));

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERROR] SSD1306 OLED allocation failed!"));
    while (1) delay(100);
  }

  // Maximize I2C bus speed for pure 60 FPS throughput (1 MHz Fast Mode Plus)
  Wire.setClock(1000000);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Play iconic intro DOOM logo
  display.drawBitmap((SCREEN_WIDTH - 80) / 2, 18, DOOM_LOGO, 80, 13, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(24, 38);
  display.print(F("60 FPS ENGINE"));
  display.setCursor(18, 50);
  display.print(F("ESP32-C3 RISC-V"));
  display.display();
  delay(1200);

  // Initialize Fire buffer bottom row
  memset(firePixels, 0, sizeof(firePixels));
  for (int x = 0; x < FIRE_W; x++) {
    firePixels[(FIRE_H - 1) * FIRE_W + x] = 7; // Max heat
  }

  Serial.println(F("[OK] DOOM Engine Running at 60 FPS!"));
  Serial.println(F("Controls via Serial:"));
  Serial.println(F("  'w' / 's' : Move Forward / Back"));
  Serial.println(F("  'a' / 'd' : Rotate Left / Right"));
  Serial.println(F("  ' ' / 'f' : Fire Double-Barrel Shotgun"));
  Serial.println(F("  'm'       : Toggle Top-Down Radar MiniMap"));
  Serial.println(F("  'auto'    : Toggle Autonomous Bot AI"));
  Serial.println(F("  '1'       : 3D Raycaster"));
  Serial.println(F("  '2'       : PSX DOOM Fire Effect"));
  Serial.println(F("  '3'       : Doomguy Gallery"));
}

// ============================================================================
// Main Game Loop (Locked at 60 FPS target)
// ============================================================================
void loop() {
  unsigned long frameStart = micros();

  handleSerialCommands();

  display.clearDisplay();

  if (appMode == 1) {
    if (autoNavMode) {
      updateAutonomousAI();
    }
    renderRaycaster();
    renderHUD();
    if (showMiniMap) {
      renderMiniMap();
    }
  } else if (appMode == 2) {
    renderDoomFire();
  } else if (appMode == 3) {
    renderFaceShowcase();
  }

  display.display();

  // FPS Counter calculation
  frameCounter++;
  if (millis() - lastFpsMillis >= 1000) {
    currentFps = frameCounter;
    frameCounter = 0;
    lastFpsMillis = millis();
  }

  // Precise frame rate limiter to lock smoothly at 60 FPS
  unsigned long frameElapsed = micros() - frameStart;
  const unsigned long targetFrameUs = 16666; // 60 FPS = 16.666 ms
  if (frameElapsed < targetFrameUs) {
    delayMicroseconds(targetFrameUs - frameElapsed);
  }
}

// ============================================================================
// 3D DDA Raycaster Implementation (Full 128-column resolution)
// ============================================================================
void renderRaycaster() {
  // Screen flash on shotgun discharge
  if (screenFlash) {
    display.invertDisplay(true);
    screenFlash = false;
  } else {
    display.invertDisplay(false);
  }

  // Draw Horizon floor separator line
  display.drawFastHLine(0, VIEW_HEIGHT / 2, SCREEN_WIDTH, SSD1306_WHITE);

  // Cast 128 rays across horizontal viewport
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    float cameraX = 2.0f * x / (float)SCREEN_WIDTH - 1.0f;
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;

    int mapX = int(posX);
    int mapY = int(posY);

    float deltaDistX = (rayDirX == 0) ? 1e30f : std::abs(1.0f / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30f : std::abs(1.0f / rayDirY);

    float sideDistX, sideDistY;
    int stepX, stepY;

    if (rayDirX < 0) {
      stepX = -1;
      sideDistX = (posX - mapX) * deltaDistX;
    } else {
      stepX = 1;
      sideDistX = (mapX + 1.0f - posX) * deltaDistX;
    }

    if (rayDirY < 0) {
      stepY = -1;
      sideDistY = (posY - mapY) * deltaDistY;
    } else {
      stepY = 1;
      sideDistY = (mapY + 1.0f - posY) * deltaDistY;
    }

    int hit = 0;
    int side = 0; // 0: X-wall, 1: Y-wall

    // DDA Step loop
    while (hit == 0) {
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) break;
      if (worldMap[mapX][mapY] > 0) hit = 1;
    }

    float perpWallDist;
    if (side == 0) perpWallDist = (sideDistX - deltaDistX);
    else perpWallDist = (sideDistY - deltaDistY);

    if (perpWallDist < 0.15f) perpWallDist = 0.15f;
    zBuffer[x] = perpWallDist;

    // Calculate height of wall on screen
    int lineHeight = (int)(VIEW_HEIGHT / perpWallDist);
    int drawStart = -lineHeight / 2 + VIEW_HEIGHT / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + VIEW_HEIGHT / 2;
    if (drawEnd >= VIEW_HEIGHT) drawEnd = VIEW_HEIGHT - 1;

    // Depth shading & Lighting dithering
    if (perpWallDist < 2.0f) {
      // Close distance: solid or side-shaded
      if (side == 1) {
        // Vertical micro-stripe shading for side lighting
        for (int y = drawStart; y <= drawEnd; y++) {
          if (((x + y) & 1) == 0) display.drawPixel(x, y, SSD1306_WHITE);
        }
      } else {
        display.drawFastVLine(x, drawStart, drawEnd - drawStart + 1, SSD1306_WHITE);
      }
    } else if (perpWallDist < 4.2f) {
      // Medium distance: 50% checkerboard dither
      for (int y = drawStart; y <= drawEnd; y++) {
        if (side == 0) {
          if (((x + y) & 1) == 0) display.drawPixel(x, y, SSD1306_WHITE);
        } else {
          if ((y & 1) == 0 && (x & 1) == 0) display.drawPixel(x, y, SSD1306_WHITE);
        }
      }
    } else if (perpWallDist < 7.0f) {
      // Far distance: 25% sparse dither
      for (int y = drawStart; y <= drawEnd; y++) {
        if ((x % 2 == 0) && (y % 2 == 0)) {
          display.drawPixel(x, y, SSD1306_WHITE);
        }
      }
    } else {
      // Very far: top & bottom edge borders only
      display.drawPixel(x, drawStart, SSD1306_WHITE);
      display.drawPixel(x, drawEnd, SSD1306_WHITE);
    }
  }

  // --------------------------------------------------------------------------
  // 3D Sprite Projection (Imps & Barrels)
  // --------------------------------------------------------------------------
  // Calculate distance for all sprites
  for (int i = 0; i < NUM_SPRITES; i++) {
    sprites[i].dist = ((posX - sprites[i].x) * (posX - sprites[i].x) +
                       (posY - sprites[i].y) * (posY - sprites[i].y));
  }

  // Sort sprites by distance (furthest to nearest)
  for (int i = 0; i < NUM_SPRITES - 1; i++) {
    for (int j = i + 1; j < NUM_SPRITES; j++) {
      if (sprites[i].dist < sprites[j].dist) {
        Sprite tmp = sprites[i];
        sprites[i] = sprites[j];
        sprites[j] = tmp;
      }
    }
  }

  // Project sprites onto camera plane
  for (int i = 0; i < NUM_SPRITES; i++) {
    if (!sprites[i].alive) continue;

    float spriteX = sprites[i].x - posX;
    float spriteY = sprites[i].y - posY;

    float invDet = 1.0f / (planeX * dirY - dirX * planeY);
    float transformX = invDet * (dirY * spriteX - dirX * spriteY);
    float transformY = invDet * (-planeY * spriteX + planeX * spriteY); // Depth in front of camera

    if (transformY > 0.25f) { // Sprite is in front of player
      int spriteScreenX = int((SCREEN_WIDTH / 2) * (1.0f + transformX / transformY));
      int spriteH = std::abs(int(VIEW_HEIGHT / transformY));
      if (spriteH > 40) spriteH = 40;
      int spriteW = spriteH;

      int drawStartY = -spriteH / 2 + VIEW_HEIGHT / 2;
      if (drawStartY < 0) drawStartY = 0;
      int drawEndY = spriteH / 2 + VIEW_HEIGHT / 2;
      if (drawEndY >= VIEW_HEIGHT) drawEndY = VIEW_HEIGHT - 1;

      int drawStartX = -spriteW / 2 + spriteScreenX;
      if (drawStartX < 0) drawStartX = 0;
      int drawEndX = spriteW / 2 + spriteScreenX;
      if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;

      // Draw sprite columns if in front of Z-Buffer
      for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
        if (transformY < zBuffer[stripe]) {
          // Draw scaled sprite column
          int texX = (stripe - (-spriteW / 2 + spriteScreenX)) * 16 / spriteW;
          if (texX >= 0 && texX < 16) {
            for (int y = drawStartY; y < drawEndY; y++) {
              int texY = (y - drawStartY) * 16 / (drawEndY - drawStartY + 1);
              if (texY >= 0 && texY < 16) {
                bool pixelOn = false;
                if (sprites[i].type == 0) { // Imp
                  uint8_t byteVal = pgm_read_byte(&IMP_SPRITE[texY * 2 + (texX / 8)]);
                  pixelOn = (byteVal & (1 << (7 - (texX % 8)))) != 0;
                } else { // Barrel
                  if (texX < 12 && texY < 14) {
                    uint8_t byteVal = pgm_read_byte(&BARREL_SPRITE[texY * 2 + (texX / 8)]);
                    pixelOn = (byteVal & (1 << (7 - (texX % 8)))) != 0;
                  }
                }
                if (pixelOn) {
                  display.drawPixel(stripe, y, SSD1306_WHITE);
                }
              }
            }
          }
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // Weapon Animation (Double-barrel Shotgun with walk bob & recoil)
  // --------------------------------------------------------------------------
  int bobY = (int)(sin(walkBob) * 2.5f);
  int gunBaseX = (SCREEN_WIDTH - 24) / 2;
  int gunBaseY = 32 + bobY;

  if (weaponAnimState == 1) { // Muzzle flash
    // Draw explosive blast flash
    display.drawBitmap((SCREEN_WIDTH - 32) / 2, 14, SHOTGUN_FLASH, 32, 18, SSD1306_WHITE);
    // Recoil shotgun slightly back
    display.drawBitmap(gunBaseX, gunBaseY + 4, SHOTGUN_IDLE, 24, 20, SSD1306_WHITE);
    weaponAnimTimer++;
    if (weaponAnimTimer >= 2) {
      weaponAnimState = 2;
      weaponAnimTimer = 0;
    }
  } else if (weaponAnimState == 2) { // Recoil downward
    display.drawBitmap(gunBaseX, gunBaseY + 6, SHOTGUN_IDLE, 24, 20, SSD1306_WHITE);
    // Draw smoke puffs
    display.drawPixel(gunBaseX + 6, gunBaseY - 2, SSD1306_WHITE);
    display.drawPixel(gunBaseX + 18, gunBaseY - 3, SSD1306_WHITE);
    display.drawPixel(gunBaseX + 12, gunBaseY - 5, SSD1306_WHITE);
    weaponAnimTimer++;
    if (weaponAnimTimer >= 3) {
      weaponAnimState = 3;
      weaponAnimTimer = 0;
    }
  } else if (weaponAnimState == 3) { // Pump action
    display.drawBitmap(gunBaseX, gunBaseY + 2, SHOTGUN_IDLE, 24, 20, SSD1306_WHITE);
    weaponAnimTimer++;
    if (weaponAnimTimer >= 3) {
      weaponAnimState = 0;
      weaponAnimTimer = 0;
      faceLook = 0; // Return face to center
    }
  } else { // Idle
    display.drawBitmap(gunBaseX, gunBaseY, SHOTGUN_IDLE, 24, 20, SSD1306_WHITE);
  }
}

// ============================================================================
// DOOM Status Bar HUD (Bottom 12 Pixels)
// ============================================================================
void renderHUD() {
  // Top horizontal border
  display.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);
  display.fillRect(0, 53, SCREEN_WIDTH, HUD_HEIGHT, SSD1306_BLACK);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Left: Ammo & Health
  display.setCursor(2, 55);
  display.print(ammo);
  display.setCursor(22, 55);
  display.print(health);
  display.print(F("%"));

  // Center: Doomguy Animated Face (16x12)
  int faceX = (SCREEN_WIDTH - 16) / 2;
  int faceY = 52;

  // Face glancing animation
  if (weaponAnimState > 0) {
    faceLook = 3; // Grin when firing!
  } else if (millis() - lastFaceChange > 2500) {
    lastFaceChange = millis();
    int r = esp_random() % 10;
    if (r < 6) faceLook = 0;      // Center
    else if (r < 8) faceLook = 1; // Left
    else faceLook = 2;            // Right
  }

  const unsigned char* faceBitmap = DOOMGUY_CENTER;
  if (faceLook == 1) faceBitmap = DOOMGUY_LEFT;
  else if (faceLook == 2) faceBitmap = DOOMGUY_RIGHT;
  else if (faceLook == 3) faceBitmap = DOOMGUY_GRIN;
  else if (faceLook == 4) faceBitmap = DOOMGUY_OUCH;

  display.drawBitmap(faceX, faceY, faceBitmap, 16, 12, SSD1306_WHITE);

  // Right: Armor & 60 FPS Counter
  display.setCursor(76, 55);
  display.print(armor);
  display.print(F("%"));

  display.setCursor(106, 55);
  display.print(currentFps);
  display.print(F("F"));
}

// ============================================================================
// Top-Down Radar MiniMap Overlay
// ============================================================================
void renderMiniMap() {
  const int mmX = 2;
  const int mmY = 2;
  const int scale = 2;

  display.drawRect(mmX - 1, mmY - 1, MAP_W * scale + 2, MAP_H * scale + 2, SSD1306_WHITE);
  display.fillRect(mmX, mmY, MAP_W * scale, MAP_H * scale, SSD1306_BLACK);

  for (int x = 0; x < MAP_W; x++) {
    for (int y = 0; y < MAP_H; y++) {
      if (worldMap[x][y] > 0) {
        display.fillRect(mmX + x * scale, mmY + y * scale, scale, scale, SSD1306_WHITE);
      }
    }
  }

  // Draw player position & view direction
  int pX = mmX + (int)(posX * scale);
  int pY = mmY + (int)(posY * scale);
  display.drawPixel(pX, pY, SSD1306_WHITE);
  display.drawLine(pX, pY, pX + (int)(dirX * 3.5f), pY + (int)(dirY * 3.5f), SSD1306_WHITE);

  // Draw enemy blips
  for (int i = 0; i < NUM_SPRITES; i++) {
    if (sprites[i].alive) {
      int sX = mmX + (int)(sprites[i].x * scale);
      int sY = mmY + (int)(sprites[i].y * scale);
      display.drawPixel(sX, sY, SSD1306_WHITE);
    }
  }
}

// ============================================================================
// Weapon Firing Logic
// ============================================================================
void fireShotgun() {
  if (weaponAnimState != 0) return; // Still animating
  if (ammo > 0) ammo--;
  else ammo = 50; // Auto-reload

  weaponAnimState = 1;
  weaponAnimTimer = 0;
  screenFlash = true;
  faceLook = 3; // Evil Grin!

  // Check if any sprite is in crosshairs (center column)
  for (int i = 0; i < NUM_SPRITES; i++) {
    if (!sprites[i].alive) continue;

    float sprX = sprites[i].x - posX;
    float sprY = sprites[i].y - posY;
    float invDet = 1.0f / (planeX * dirY - dirX * planeY);
    float transformX = invDet * (dirY * sprX - dirX * sprY);
    float transformY = invDet * (-planeY * sprX + planeX * sprY);

    if (transformY > 0.2f && transformY < 5.0f) {
      int screenCol = int((SCREEN_WIDTH / 2) * (1.0f + transformX / transformY));
      if (std::abs(screenCol - (SCREEN_WIDTH / 2)) < 18) {
        sprites[i].health--;
        if (sprites[i].health <= 0) {
          sprites[i].alive = false;
          kills++;
          Serial.printf("[BOOM] Target neutralized! Total kills: %d\n", kills);
        }
      }
    }
  }
}

// ============================================================================
// Autonomous Marine AI (Explores, avoids obstacles, targets & blasts demons)
// ============================================================================
void updateAutonomousAI() {
  // Check obstacle in front
  float frontX = posX + dirX * 0.7f;
  float frontY = posY + dirY * 0.7f;

  bool blocked = false;
  if (worldMap[int(frontX)][int(frontY)] > 0) {
    blocked = true;
  }

  // Smooth rotation or movement
  if (blocked) {
    // Rotate smoothly to clear path
    float oldDirX = dirX;
    float cosR = cos(rotSpeed * 1.5f);
    float sinR = sin(rotSpeed * 1.5f);
    dirX = dirX * cosR - dirY * sinR;
    dirY = oldDirX * sinR + dirY * cosR;

    float oldPlaneX = planeX;
    planeX = planeX * cosR - planeY * sinR;
    planeY = oldPlaneX * sinR + planeY * cosR;
  } else {
    // Move forward
    float newX = posX + dirX * moveSpeed;
    float newY = posY + dirY * moveSpeed;
    if (worldMap[int(newX)][int(posY)] == 0) posX = newX;
    if (worldMap[int(posX)][int(newY)] == 0) posY = newY;
    walkBob += 0.25f;
  }

  // Autonomous firing when enemy in sight
  if (aiShootCooldown > 0) {
    aiShootCooldown--;
  } else {
    for (int i = 0; i < NUM_SPRITES; i++) {
      if (!sprites[i].alive) continue;
      float dX = sprites[i].x - posX;
      float dY = sprites[i].y - posY;
      float dist = sqrt(dX * dX + dY * dY);
      if (dist < 4.5f) {
        // Dot product to check if in front
        float dot = (dX * dirX + dY * dirY) / dist;
        if (dot > 0.85f) { // Directly facing
          fireShotgun();
          aiShootCooldown = 40; // Cooldown frames
          break;
        }
      }
    }
  }

  // Respawn enemies if all eliminated
  bool anyAlive = false;
  for (int i = 0; i < NUM_SPRITES; i++) {
    if (sprites[i].alive) { anyAlive = true; break; }
  }
  if (!anyAlive) {
    for (int i = 0; i < NUM_SPRITES; i++) {
      sprites[i].alive = true;
      sprites[i].health = (sprites[i].type == 0) ? 2 : 1;
    }
  }
}

// ============================================================================
// Mode 2: Iconic PSX DOOM Fire Simulation (60 FPS Procedural Heat)
// ============================================================================
void renderDoomFire() {
  // Propagate fire upward with random decay & wind
  for (int x = 0; x < FIRE_W; x++) {
    for (int y = 1; y < FIRE_H; y++) {
      int src = y * FIRE_W + x;
      int randOffset = (esp_random() & 3);
      int dstX = x - randOffset + 1;
      if (dstX < 0) dstX = 0;
      if (dstX >= FIRE_W) dstX = FIRE_W - 1;
      int dstY = y - 1;

      int decay = (esp_random() & 1);
      int val = firePixels[src] - decay;
      if (val < 0) val = 0;
      firePixels[dstY * FIRE_W + dstX] = val;
    }
  }

  // Render fire buffer using dither patterns
  for (int y = 0; y < FIRE_H; y++) {
    for (int x = 0; x < FIRE_W; x++) {
      uint8_t heat = firePixels[y * FIRE_W + x];
      bool on = false;
      switch (heat) {
        case 7: on = true; break;
        case 6: on = ((x + y) % 4 != 0); break;
        case 5: on = ((x + y) % 2 == 0) || (x % 3 == 0); break;
        case 4: on = ((x + y) % 2 == 0); break;
        case 3: on = (x % 2 == 0) && (y % 2 == 0); break;
        case 2: on = (x % 3 == 0) && (y % 3 == 0); break;
        case 1: on = (x % 4 == 0) && (y % 4 == 0); break;
        default: on = false; break;
      }
      if (on) {
        display.drawPixel(x, y + 6, SSD1306_WHITE);
      }
    }
  }

  // Draw DOOM Logo floating in center of flames
  display.drawBitmap((SCREEN_WIDTH - 80) / 2, 10, DOOM_LOGO, 80, 13, SSD1306_WHITE);

  // HUD bottom banner
  display.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);
  display.fillRect(0, 53, SCREEN_WIDTH, 11, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(6, 55);
  display.print(F("INFERNO 60 FPS"));
  display.setCursor(104, 55);
  display.print(currentFps);
  display.print(F("F"));
}

// ============================================================================
// Mode 3: Doomguy Face Gallery & Combat Showcase
// ============================================================================
void renderFaceShowcase() {
  display.drawRect(0, 0, SCREEN_WIDTH, 52, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 4);
  display.print(F("DOOM MARINE"));

  int faceIdx = (millis() / 1500) % 5;
  const unsigned char* bm = DOOMGUY_CENTER;
  const char* desc = "READY";
  if (faceIdx == 1) { bm = DOOMGUY_LEFT; desc = "SCAN LEFT"; }
  else if (faceIdx == 2) { bm = DOOMGUY_RIGHT; desc = "SCAN RIGHT"; }
  else if (faceIdx == 3) { bm = DOOMGUY_GRIN; desc = "RAMPAGE!"; }
  else if (faceIdx == 4) { bm = DOOMGUY_OUCH; desc = "WOUNDED"; }

  // Draw scaled 2x face
  int fx = 16;
  int fy = 18;
  for (int y = 0; y < 12; y++) {
    for (int x = 0; x < 16; x++) {
      uint8_t b = pgm_read_byte(&bm[y * 2 + (x / 8)]);
      if (b & (1 << (7 - (x % 8)))) {
        display.fillRect(fx + x * 2, fy + y * 2, 2, 2, SSD1306_WHITE);
      }
    }
  }

  display.setCursor(56, 22);
  display.print(desc);
  display.setCursor(56, 34);
  display.print(F("RISC-V 160MHz"));

  renderHUD();
}

// ============================================================================
// Interactive Serial Command Handler
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) continue;

    char c = cmd.charAt(0);
    if (cmd.equalsIgnoreCase("auto")) {
      autoNavMode = !autoNavMode;
      Serial.printf("[AI] Autonomous Bot Mode: %s\n", autoNavMode ? "ENABLED" : "DISABLED");
    } else if (c == 'w' || c == 'W') { // Move forward
      autoNavMode = false;
      float newX = posX + dirX * moveSpeed * 2.0f;
      float newY = posY + dirY * moveSpeed * 2.0f;
      if (worldMap[int(newX)][int(posY)] == 0) posX = newX;
      if (worldMap[int(posX)][int(newY)] == 0) posY = newY;
      walkBob += 0.4f;
    } else if (c == 's' || c == 'S') { // Move backward
      autoNavMode = false;
      float newX = posX - dirX * moveSpeed * 2.0f;
      float newY = posY - dirY * moveSpeed * 2.0f;
      if (worldMap[int(newX)][int(posY)] == 0) posX = newX;
      if (worldMap[int(posX)][int(newY)] == 0) posY = newY;
      walkBob += 0.4f;
    } else if (c == 'a' || c == 'A') { // Rotate left
      autoNavMode = false;
      float oldDirX = dirX;
      float cosR = cos(rotSpeed * 2.0f);
      float sinR = sin(rotSpeed * 2.0f);
      dirX = dirX * cosR - dirY * sinR;
      dirY = oldDirX * sinR + dirY * cosR;
      float oldPlaneX = planeX;
      planeX = planeX * cosR - planeY * sinR;
      planeY = oldPlaneX * sinR + planeY * cosR;
    } else if (c == 'd' || c == 'D') { // Rotate right
      autoNavMode = false;
      float oldDirX = dirX;
      float cosR = cos(-rotSpeed * 2.0f);
      float sinR = sin(-rotSpeed * 2.0f);
      dirX = dirX * cosR - dirY * sinR;
      dirY = oldDirX * sinR + dirY * cosR;
      float oldPlaneX = planeX;
      planeX = planeX * cosR - planeY * sinR;
      planeY = oldPlaneX * sinR + planeY * cosR;
    } else if (c == ' ' || c == 'f' || c == 'F') { // Fire
      fireShotgun();
    } else if (c == 'm' || c == 'M') { // Toggle minimap
      showMiniMap = !showMiniMap;
      Serial.printf("[HUD] MiniMap: %s\n", showMiniMap ? "ON" : "OFF");
    } else if (c == '1') {
      appMode = 1;
      Serial.println(F("[MODE] 1: 3D DOOM Raycaster"));
    } else if (c == '2') {
      appMode = 2;
      Serial.println(F("[MODE] 2: PSX DOOM Fire Effect"));
    } else if (c == '3') {
      appMode = 3;
      Serial.println(F("[MODE] 3: Doomguy Face Gallery"));
    } else if (cmd.equalsIgnoreCase("mode")) {
      appMode = (appMode % 3) + 1;
      Serial.printf("[MODE] Switched to Mode %d\n", appMode);
    }
  }
}

