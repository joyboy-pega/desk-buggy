#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_Fingerprint.h>

// ================= HARDWARE SETTINGS =================
#define DISPLAY_ROTATION 3    // 3 = 180 deg landscape (160x128), 1 = alternate landscape
                              // (agar screen ulta ho to 1 karo; portrait ke liye 0 ya 2)

#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

#define BUZZER_PIN       13   // Passive or active buzzer (optional)
#define BUZZER_ACTIVE    1

#define FP_RX            16   // AS608 TX -> ESP32 GPIO16 (optional)
#define FP_TX            17   // AS608 RX -> ESP32 GPIO17 (optional)
#define FP_TOUCH_PIN     -1   // Touch wake wire, -1 if unused
// =====================================================

// ================= PALETTE: BMO (RGB565) =============
// Sampled authentically from the BMO scan
#define C_CASE         0x85D5   // RGB(135, 186, 172) - BMO casing mint body
#define C_SCREEN       0xD7FC   // RGB(210, 252, 228) - Light mint cyan screen
#define C_DARK         0x1124   // RGB(20, 38, 34)    - Dark navy / screen bezel & facial lines
#define C_MOUTH        0x446B   // RGB(70, 142, 90)   - BMO green tongue / mouth interior
#define C_WHITE        0xFFFF   // RGB(255, 255, 255) - Teeth / highlights
#define C_TEAR         0x6E5C   // RGB(110, 200, 225) - Crying teardrop cyan
#define C_SHADOW       0xB73A   // RGB(182, 230, 210) - Screen bevel shadow
#define C_NOTE         0x1944   // RGB(25, 42, 38)    - Floating musical note

// ================= PALETTE: BAYMAX (RGB565) ==========
// Clean white face with dark eyes and eyelid contours
#define C_BAYMAX_WHITE   0xFFFF // RGB(255, 255, 255) - Pure white full screen
#define C_BAYMAX_LINE    0x0000 // RGB(0, 0, 0)       - Deep black eyes & connecting bridge
#define C_BAYMAX_LID     0x7BEF // RGB(123, 123, 123) - Clean graphite eyelid contour
#define C_BAYMAX_SCAN    0x07FF // Cyan medical laser scan beam
#define C_BAYMAX_HEART   0xF800 // Red fist-bump / care heart
// =====================================================

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 *cv = nullptr;

HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);
bool sensorFound = false;

// Screen geometry for 160x128
int W = 160;
int H = 128;

// Character Mode
enum CharacterMode {
  CHAR_BMO = 0,
  CHAR_BAYMAX = 1
};

CharacterMode currentMode = CHAR_BAYMAX; // Default or switchable

// ---------------- BMO Geometry & State ----------------
int bmoScrX = 14;
int bmoScrY = 8;
int bmoScrW = 132;
int bmoScrH = 92;
int bmoScrR = 7;

int bmoEyeY = 42;
int bmoEyeLX = 40;
int bmoEyeRX = 120;
int bmoMouthX = 80;
int bmoMouthY = 62;

enum BmoExpression {
  BMO_GRIT = 0,     // Row 0, Col 0: Clenched squint eyes & gritted teeth grimace
  BMO_WINK = 1,     // Row 0, Col 1: Dot eye + wink + open bean talking mouth
  BMO_SING = 2,     // Row 0, Col 2: Closed happy eyes + large round "O" singing mouth
  BMO_CONTENT = 3,  // Row 1, Col 0: Closed smiling eyes + gentle sweet smile
  BMO_GRIN = 4,     // Row 1, Col 1: Dot eyes + wide open crescent grin
  BMO_LAUGH = 5,    // Row 1, Col 2: Wide dot eyes + laughing :D with white teeth & tongue
  BMO_SMILE = 6,    // Row 2, Col 0: Classic cute BMO face: dot eyes + cute smile
  BMO_WORRIED = 7,  // Row 2, Col 1: Slanted sad eyes + falling tears + trembling wavy mouth
  BMO_NEUTRAL = 8   // Row 2, Col 2: Dot eyes + deadpan flat line mouth
};

BmoExpression currentBmoExp = BMO_SMILE;

// ---------------- Baymax Geometry & State -------------
// Head geometry: center 80, 64; radius X=58, Y=48
int bmHeadCX = 80;
int bmHeadCY = 64;

int bmEyeY = 64;
int bmEyeLX = 48;
int bmEyeRX = 112;
int bmEyeR = 9;

enum BaymaxExpression {
  BM_NEUTRAL = 0,    // Frame 0 (Row 0, Col 0): Classic wide open solid black eyes & bridge
  BM_HAPPY = 1,      // Frame 1 (Row 0, Col 1): Satisfied cheek-push smile (top half black, bottom hollow)
  BM_SKEPTICAL = 2,  // Frame 2 (Row 0, Col 2): Suspicious diagnosis squint (\ / eyelids)
  BM_SAD = 3,        // Frame 3 (Row 1, Col 0): Compassionate puppy eyes (/ \ eyelids)
  BM_TIRED = 4,      // Frame 4 (Row 1, Col 1): Low battery / sleepy heavy drooping lids
  BM_SLEEPING = 5,   // Frame 5 (Row 1, Col 2): Fully closed / resting eyes with bridge line
  BM_SCAN = 6,       // Special: Medical scan beam sweeping with diagnostic tone
  BM_FISTBUMP = 7    // Special: "Ba-la-la-la-la!" fist bump celebration
};

BaymaxExpression currentBmExp = BM_NEUTRAL;

// Animation & Interaction Variables
bool autoAnimate = true;
uint32_t expStartTime = 0;
uint32_t expDuration = 4000;

// Blinking (Natural multi-frame for Baymax, quick for BMO)
bool isBlinking = false;
int blinkStage = 0; // 0=open, 1=half, 2=closed, 3=reopening
uint32_t blinkStageTime = 0;
uint32_t nextBlinkTime = 0;

// Idle Glance & Head Motion
int glanceX = 0;
int glanceY = 0;
uint32_t nextGlanceTime = 0;

// Subtle breathing bob
int breatheY = 0;

// Character & Expression Shake / Wobble
int shakeX = 0;
int shakeY = 0;

// BMO Specific animation states
int mouthAnim = 0;
int mouthAnimDir = 1;
uint32_t lastMouthTick = 0;
int tearY = 0;
uint32_t lastTearTick = 0;
int noteY = 0;
uint32_t lastNoteTick = 0;

// Baymax Specific animation states
int scanAnimY = 0;
int scanAnimDir = 2;
int fistBumpFrame = 0;
uint32_t lastFistBumpTick = 0;

// Touch double-tap detection
uint32_t lastTouchTime = 0;

// ================= BUZZER SOUND EFFECTS ==============
void buzzerTone(uint16_t freq, uint16_t ms) {
#if BUZZER_PIN >= 0
  pinMode(BUZZER_PIN, OUTPUT);
  #if BUZZER_ACTIVE
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
  #else
    tone(BUZZER_PIN, freq, ms);
  #endif
#endif
}

// BMO chirp
void bmoChirp() {
  buzzerTone(2600, 40);
  delay(30);
  buzzerTone(3200, 60);
}

// Baymax gentle diagnostic chime
void baymaxChime() {
  buzzerTone(880, 70);   // A5
  delay(40);
  buzzerTone(1318, 120); // E6
}

// Baymax "Ba-la-la-la-la!" celebration sound
void baymaxFistBumpSound() {
  buzzerTone(523, 50); // Ba
  delay(25);
  buzzerTone(659, 45); // la
  delay(20);
  buzzerTone(784, 45); // la
  delay(20);
  buzzerTone(880, 45); // la
  delay(20);
  buzzerTone(1046, 90);// la!
}

// Baymax medical scan sweep sound
void baymaxScanSound() {
  buzzerTone(1200, 30);
  buzzerTone(1500, 30);
  buzzerTone(1800, 30);
}

// ================= DRAWING UTILITIES =================
void drawThickHLine(int x, int y, int w, int thickness, uint16_t color) {
  cv->fillRect(x, y - (thickness / 2), w, thickness, color);
}

void drawThickLine(int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
  int ht = thickness / 2;
  for (int o = -ht; o <= ht; o++) {
    cv->drawLine(x0 + o, y0, x1 + o, y1, color);
    cv->drawLine(x0, y0 + o, x1, y1 + o, color);
  }
}

// ================= BMO RENDERING =====================
void drawBmoConsole(int sx, int sy) {
  // Casing body background
  cv->fillScreen(C_CASE);

  // Screen background (light mint cyan)
  cv->fillRoundRect(bmoScrX + sx, bmoScrY + sy, bmoScrW, bmoScrH, bmoScrR, C_SCREEN);

  // Inner bevel shadow (subtle depth highlight)
  cv->drawFastHLine(bmoScrX + 4 + sx, bmoScrY + 3 + sy, bmoScrW - 8, C_SHADOW);
  cv->drawFastHLine(bmoScrX + 5 + sx, bmoScrY + 4 + sy, bmoScrW - 10, C_SHADOW);
  cv->drawFastVLine(bmoScrX + 3 + sx, bmoScrY + 4 + sy, bmoScrH - 8, C_SHADOW);

  // Screen dark border (2px thick rounded rectangle)
  cv->drawRoundRect(bmoScrX + sx, bmoScrY + sy, bmoScrW, bmoScrH, bmoScrR, C_DARK);
  cv->drawRoundRect(bmoScrX + 1 + sx, bmoScrY + 1 + sy, bmoScrW - 2, bmoScrH - 2, bmoScrR - 1, C_DARK);

  // Floppy disk cartridge slot (bottom-left)
  cv->fillRoundRect(24 + sx, 112 + sy, 52, 6, 2, C_DARK);

  // Controller port / circular button (bottom-right)
  cv->fillCircle(136 + sx, 115 + sy, 5, C_DARK);
}

void drawBmoDotEye(int cx, int cy, int r = 4) {
  cv->fillRoundRect(cx - r, cy - r - 1, (r * 2) + 1, (r * 2) + 2, 2, C_DARK);
}

void drawBmoArcEye(int cx, int cy, int w = 7, int h = 5, bool inverted = false) {
  int prevX = cx - w;
  int prevY = cy;
  for (int dx = -w; dx <= w; dx++) {
    float norm = (float)dx / (float)w;
    int dy = (int)(h * (1.0f - (norm * norm)));
    int py = inverted ? (cy + dy) : (cy - dy);
    int px = cx + dx;
    drawThickLine(prevX, prevY, px, py, 2, C_DARK);
    prevX = px;
    prevY = py;
  }
}

void drawBmoSquintEye(int cx, int cy, bool pointingRight) {
  int dx = pointingRight ? 8 : -8;
  drawThickLine(cx - 5, cy - 7, cx + 5, cy - 7, 2, C_DARK);
  drawThickLine(cx - dx, cy - 5, cx + dx, cy, 2, C_DARK);
  drawThickLine(cx + dx, cy, cx - dx, cy + 5, 2, C_DARK);
  drawThickLine(cx - 5, cy + 7, cx + 5, cy + 7, 2, C_DARK);
}

void drawBmoWinkEye(int cx, int cy) {
  drawThickLine(cx - 7, cy - 4, cx - 1, cy + 1, 2, C_DARK);
  drawThickLine(cx - 1, cy + 1, cx + 7, cy - 2, 2, C_DARK);
}

void drawBmoBlinkEye(int cx, int cy) {
  drawThickLine(cx - 6, cy, cx + 6, cy, 2, C_DARK);
}

void drawBmoNote(int nx, int ny, int type) {
  if (type == 0) {
    cv->fillRect(nx, ny + 4, 4, 3, C_DARK);
    cv->drawFastVLine(nx + 3, ny, 6, C_DARK);
    cv->drawFastHLine(nx + 3, ny, 3, C_DARK);
  } else {
    cv->fillRect(nx, ny + 5, 3, 3, C_DARK);
    cv->fillRect(nx + 8, ny + 5, 3, 3, C_DARK);
    cv->drawFastVLine(nx + 2, ny, 7, C_DARK);
    cv->drawFastVLine(nx + 10, ny, 7, C_DARK);
    cv->drawFastHLine(nx + 2, ny, 9, C_DARK);
  }
}

void drawBmoTeardrop(int tx, int ty) {
  cv->fillRoundRect(tx - 2, ty, 5, 8, 2, C_TEAR);
  cv->drawPixel(tx, ty - 1, C_TEAR);
}

void drawBmoFace() {
  int lx = bmoEyeLX + glanceX + shakeX;
  int rx = bmoEyeRX + glanceX + shakeX;
  int ly = bmoEyeY + glanceY + shakeY;
  int ry = bmoEyeY + glanceY + shakeY;
  int mx = bmoMouthX + shakeX;
  int my = bmoMouthY + shakeY;

  drawBmoConsole(shakeX, shakeY);

  // 1. Draw Eyes
  if (isBlinking && currentBmoExp != BMO_GRIT && currentBmoExp != BMO_SING && currentBmoExp != BMO_CONTENT) {
    drawBmoBlinkEye(lx, ly);
    drawBmoBlinkEye(rx, ry);
  } else {
    switch (currentBmoExp) {
      case BMO_GRIT:
        drawBmoSquintEye(lx, ly, true);
        drawBmoSquintEye(rx, ry, false);
        break;

      case BMO_WINK:
        drawBmoDotEye(lx, ly, 4);
        drawBmoWinkEye(rx, ry);
        break;

      case BMO_SING:
        drawBmoArcEye(lx, ly - (mouthAnim / 2), 6, 4, false);
        drawBmoArcEye(rx, ry - (mouthAnim / 2), 6, 4, false);
        break;

      case BMO_CONTENT:
        drawBmoArcEye(lx, ly, 7, 5, false);
        drawBmoArcEye(rx, ry, 7, 5, false);
        break;

      case BMO_WORRIED:
        drawThickLine(lx - 4, ly - 6, lx + 4, ly + 6, 3, C_DARK);
        drawThickLine(rx + 4, ry - 6, rx - 4, ry + 6, 3, C_DARK);
        if (tearY > 0) {
          drawBmoTeardrop(lx + 1, ly + 6 + tearY);
          drawBmoTeardrop(rx - 1, ry + 6 + tearY);
        }
        break;

      case BMO_LAUGH:
        drawBmoDotEye(lx, ly, 5);
        drawBmoDotEye(rx, ry, 5);
        break;

      case BMO_GRIN:
      case BMO_SMILE:
      case BMO_NEUTRAL:
      default:
        drawBmoDotEye(lx, ly, 4);
        drawBmoDotEye(rx, ry, 4);
        break;
    }
  }

  // 2. Draw Mouth
  switch (currentBmoExp) {
    case BMO_GRIT: {
      int mw = 26;
      int mh = 11;
      cv->fillRect(mx - mw, my - mh, mw * 2, mh * 2, C_WHITE);
      cv->drawRect(mx - mw, my - mh, mw * 2, mh * 2, C_DARK);
      cv->drawRect(mx - mw - 1, my - mh - 1, (mw * 2) + 2, (mh * 2) + 2, C_DARK);
      drawThickHLine(mx - mw, my, mw * 2, 2, C_DARK);
      for (int tx = mx - mw + 7; tx < mx + mw; tx += 7) {
        cv->drawFastVLine(tx, my - mh + 1, mh * 2 - 2, C_DARK);
      }
      break;
    }

    case BMO_WINK: {
      int mo = mouthAnim;
      int bw = 10 + mo;
      int bh = 14 + mo;
      cv->fillRoundRect(mx - 8, my - 6, bw * 2, bh, 6, C_MOUTH);
      cv->drawRoundRect(mx - 8, my - 6, bw * 2, bh, 6, C_DARK);
      cv->drawRoundRect(mx - 9, my - 7, (bw * 2) + 2, bh + 2, 7, C_DARK);
      cv->fillRect(mx - 6, my - 5, bw + 4, 3, C_WHITE);
      break;
    }

    case BMO_SING: {
      int rw = 10 + (mouthAnim / 2);
      int rh = 14 + mouthAnim;
      for (int dy = -rh; dy <= rh; dy++) {
        float norm = (float)dy / (float)rh;
        int dx = (int)(rw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, my + dy + 2, (dx * 2) + 1, C_MOUTH);
      }
      for (int dy = -rh; dy <= rh; dy++) {
        float norm = (float)dy / (float)rh;
        int dx = (int)(rw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawPixel(mx - dx, my + dy + 2, C_DARK);
        cv->drawPixel(mx + dx, my + dy + 2, C_DARK);
        cv->drawPixel(mx - dx - 1, my + dy + 2, C_DARK);
        cv->drawPixel(mx + dx + 1, my + dy + 2, C_DARK);
      }
      if (noteY > 0) {
        drawBmoNote(bmoScrX + 18, bmoScrY + 68 - noteY, 0);
        drawBmoNote(bmoScrX + bmoScrW - 24, bmoScrY + 60 - noteY, 1);
      }
      break;
    }

    case BMO_CONTENT: {
      int prevX = mx - 12;
      int prevY = my - 2;
      for (int dx = -12; dx <= 12; dx++) {
        float norm = (float)dx / 12.0f;
        int py = (my - 2) + (int)(5.0f * (norm * norm));
        int px = mx + dx;
        drawThickLine(prevX, prevY, px, py, 2, C_DARK);
        prevX = px;
        prevY = py;
      }
      break;
    }

    case BMO_GRIN: {
      int gw = 15;
      int gh = 10;
      for (int dy = 0; dy <= gh; dy++) {
        float norm = (float)dy / (float)gh;
        int dx = (int)(gw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, my + dy - 1, (dx * 2) + 1, C_MOUTH);
      }
      drawThickHLine(mx - gw, my - 2, (gw * 2) + 1, 2, C_DARK);
      for (int dy = 0; dy <= gh; dy++) {
        float norm = (float)dy / (float)gh;
        int dx = (int)(gw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawPixel(mx - dx, my + dy - 1, C_DARK);
        cv->drawPixel(mx + dx, my + dy - 1, C_DARK);
        cv->drawPixel(mx - dx - 1, my + dy - 1, C_DARK);
        cv->drawPixel(mx + dx + 1, my + dy - 1, C_DARK);
      }
      break;
    }

    case BMO_LAUGH: {
      int lw = 17 + mouthAnim;
      int lh = 14 + mouthAnim;
      for (int dy = 0; dy <= lh; dy++) {
        float norm = (float)dy / (float)lh;
        int dx = (int)(lw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, my + dy - 2, (dx * 2) + 1, C_MOUTH);
      }
      cv->fillRect(mx - lw + 2, my - 1, (lw * 2) - 3, 4, C_WHITE);
      drawThickHLine(mx - lw, my - 2, (lw * 2) + 1, 2, C_DARK);
      for (int dy = 0; dy <= lh; dy++) {
        float norm = (float)dy / (float)lh;
        int dx = (int)(lw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawPixel(mx - dx, my + dy - 2, C_DARK);
        cv->drawPixel(mx + dx, my + dy - 2, C_DARK);
        cv->drawPixel(mx - dx - 1, my + dy - 2, C_DARK);
        cv->drawPixel(mx + dx + 1, my + dy - 2, C_DARK);
      }
      break;
    }

    case BMO_SMILE: {
      int prevX = mx - 14;
      int prevY = my - 2;
      for (int dx = -14; dx <= 14; dx++) {
        float norm = (float)dx / 14.0f;
        int py = (my - 2) + (int)(5.5f * (norm * norm));
        int px = mx + dx;
        drawThickLine(prevX, prevY, px, py, 2, C_DARK);
        prevX = px;
        prevY = py;
      }
      break;
    }

    case BMO_WORRIED: {
      int ww = 13;
      int wh = 8;
      for (int dy = 0; dy <= wh; dy++) {
        float norm = (float)dy / (float)wh;
        int dx = (int)(ww * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, (my + 8) - dy, (dx * 2) + 1, C_MOUTH);
      }
      for (int dy = 0; dy <= wh; dy++) {
        float norm = (float)dy / (float)wh;
        int dx = (int)(ww * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawPixel(mx - dx, (my + 8) - dy, C_DARK);
        cv->drawPixel(mx + dx, (my + 8) - dy, C_DARK);
      }
      drawThickHLine(mx - ww, my + 8, (ww * 2) + 1, 2, C_DARK);
      break;
    }

    case BMO_NEUTRAL: {
      drawThickHLine(mx - 13, my, 27, 2, C_DARK);
      break;
    }
  }
}

// ================= BAYMAX RENDERING ==================
// Draw a single Baymax eye with exact eyelid morphology on clean full white background
void drawBaymaxSingleEye(int ecx, int ecy, int er, bool is_right, BaymaxExpression exp, int currentBlink) {
  for (int dy = -er; dy <= er; dy++) {
    for (int dx = -er; dx <= er; dx++) {
      int d2 = (dx * dx) + (dy * dy);
      if (d2 <= (er * er)) {
        int px = ecx + dx;
        int py = ecy + dy;
        if (px < 0 || px >= W || py < 0 || py >= H) continue;

        bool is_black = false;
        bool is_eyelid_edge = false;
        float cutoff = 0.0f;

        // Dynamic blink stages (Frame 0 -> Frame 1 -> Frame 5 -> Frame 1 -> Frame 0)
        if (currentBlink == 1 || currentBlink == 3) {
          // Half closed eyelid
          is_black = (dy <= 0);
          if (dy == 0) is_eyelid_edge = true;
        } else if (currentBlink == 2) {
          // Fully closed eye
          is_black = false;
        } else {
          switch (exp) {
            case BM_NEUTRAL:
              // Solid circular black eye
              is_black = true;
              break;

            case BM_HAPPY:
              // Cheeks push up lower eyelid: top half black, bottom hollow
              is_black = (dy <= 0);
              if (dy == 0) is_eyelid_edge = true;
              break;

            case BM_SKEPTICAL: {
              // Suspicious diagnosis squint (\ /):
              cutoff = (is_right ? -dx : dx) * 0.75f;
              is_black = (dy >= cutoff);
              if (fabsf(dy - cutoff) <= 0.85f) is_eyelid_edge = true;
              break;
            }

            case BM_SAD: {
              // Compassionate sad puppy eyes (/ \):
              cutoff = (is_right ? dx : -dx) * 0.75f;
              is_black = (dy >= cutoff);
              if (fabsf(dy - cutoff) <= 0.85f) is_eyelid_edge = true;
              break;
            }

            case BM_TIRED:
              // Low battery / sleepy: heavy upper eyelid droops down
              is_black = (dy >= 1);
              if (dy == 1) is_eyelid_edge = true;
              break;

            case BM_SLEEPING:
              // Both eyes resting closed
              is_black = false;
              break;

            case BM_SCAN:
            case BM_FISTBUMP:
            default:
              is_black = true;
              break;
          }
        }

        // Horizontal connecting line cuts through dy in [-1, 1]
        if (abs(dy) <= 1) {
          cv->drawPixel(px, py, C_BAYMAX_LINE);
        } else if (is_black) {
          cv->drawPixel(px, py, C_BAYMAX_LINE);
        } else {
          // Eyelid perimeter outline and cut edge
          bool is_border = (d2 >= ((er - 1) * (er - 1)));
          cv->drawPixel(px, py, (is_border || is_eyelid_edge) ? C_BAYMAX_LID : C_BAYMAX_WHITE);
        }
      }
    }
  }
}

// Draw Baymax facial features on pure white background across whole screen
void drawBaymaxFace() {
  int hcx = bmHeadCX + glanceX + shakeX;
  int hcy = bmHeadCY + breatheY + shakeY;
  int elx = hcx - 32;
  int erx = hcx + 32;
  int ey = hcy + glanceY;

  // 1. Entire screen background is pure white (circle removed)
  cv->fillScreen(C_BAYMAX_WHITE);

  // 2. Draw Horizontal Connecting Eye Bridge (3px thick)
  cv->fillRect(elx, ey - 1, (erx - elx) + 1, 3, C_BAYMAX_LINE);

  // 3. Draw Left and Right Eyes
  drawBaymaxSingleEye(elx, ey, bmEyeR, false, currentBmExp, blinkStage);
  drawBaymaxSingleEye(erx, ey, bmEyeR, true, currentBmExp, blinkStage);

  // 4. Special Expression Animations
  // A. Medical Laser Scanning Beam across full screen
  if (currentBmExp == BM_SCAN) {
    int sy = scanAnimY;
    if (sy >= 2 && sy < H - 2) {
      cv->drawFastHLine(0, sy - 2, W, 0xB638); // Soft cyan trail
      cv->drawFastHLine(0, sy - 1, W, 0x6EFE); // Bright cyan glow
      cv->drawFastHLine(0, sy,     W, C_BAYMAX_SCAN); // Laser beam core
      cv->drawFastHLine(0, sy + 1, W, 0x6EFE);
      cv->drawFastHLine(0, sy + 2, W, 0xB638);
    }
  }

  // B. Fist Bump Celebration ("Ba-la-la-la-la!")
  if (currentBmExp == BM_FISTBUMP) {
    int hx = hcx + 42;
    int hy = hcy + 22;
    int pulse = (fistBumpFrame % 2 == 0) ? 0 : 1;
    // Cute small pixel heart on white screen
    cv->fillRect(hx - 3 - pulse, hy - 2, 3 + pulse, 3, C_BAYMAX_HEART);
    cv->fillRect(hx + 1, hy - 2, 3 + pulse, 3, C_BAYMAX_HEART);
    cv->fillRect(hx - 4 - pulse, hy, 9 + (pulse * 2), 3, C_BAYMAX_HEART);
    cv->fillRect(hx - 3, hy + 3, 7, 2, C_BAYMAX_HEART);
    cv->fillRect(hx - 2, hy + 5, 5, 2, C_BAYMAX_HEART);
    cv->drawPixel(hx, hy + 7, C_BAYMAX_HEART);

    // Radiating joy sparkles
    if (fistBumpFrame > 2) {
      cv->drawPixel(hx - 8, hy - 4, C_BAYMAX_HEART);
      cv->drawPixel(hx + 10, hy - 4, C_BAYMAX_HEART);
      cv->drawPixel(hx + 8, hy + 8, C_BAYMAX_HEART);
    }
  }
}

// Push canvas to physical ST7735 display
void render() {
  if (cv != nullptr) {
    tft.drawRGBBitmap(0, 0, cv->getBuffer(), W, H);
  }
}

// Switch Character (BMO <-> Baymax)
void switchCharacter(CharacterMode mode) {
  currentMode = mode;
  expStartTime = millis();
  blinkStage = 0;
  isBlinking = false;
  glanceX = 0;
  glanceY = 0;
  shakeX = 0;
  shakeY = 0;

  Serial.println("=========================================");
  if (currentMode == CHAR_BMO) {
    Serial.println("[*] ACTIVE MODE: BMO (Adventure Time)");
    bmoChirp();
  } else {
    Serial.println("[*] ACTIVE MODE: BAYMAX (Big Hero 6)");
    baymaxChime();
  }
  Serial.println("=========================================");
}

// Switch Expression for Current Character
void setExpressionIndex(int idx) {
  expStartTime = millis();
  blinkStage = 0;
  isBlinking = false;
  shakeX = 0;
  shakeY = 0;

  if (currentMode == CHAR_BMO) {
    if (idx >= 0 && idx <= 8) {
      if (currentBmoExp != (BmoExpression)idx) {
        Serial.print("BMO Expression -> ");
        Serial.println(idx);
      }
      currentBmoExp = (BmoExpression)idx;
      tearY = 0;
      noteY = 0;
      mouthAnim = 0;
    }
  } else {
    if (idx >= 0 && idx <= 7) {
      if (currentBmExp != (BaymaxExpression)idx) {
        Serial.print("Baymax Expression -> ");
        Serial.println(idx);
      }
      currentBmExp = (BaymaxExpression)idx;
      scanAnimY = 0;
      fistBumpFrame = 0;
      if (currentBmExp == BM_FISTBUMP) {
        baymaxFistBumpSound();
      } else if (currentBmExp == BM_SCAN) {
        baymaxScanSound();
      }
    }
  }
}

void nextExpression() {
  if (currentMode == CHAR_BMO) {
    int next = ((int)currentBmoExp + 1) % 9;
    setExpressionIndex(next);
  } else {
    int next = ((int)currentBmExp + 1) % 6; // Loop through the 6 iconic expressions
    setExpressionIndex(next);
  }
}

void prevExpression() {
  if (currentMode == CHAR_BMO) {
    int prev = ((int)currentBmoExp + 8) % 9;
    setExpressionIndex(prev);
  } else {
    int prev = ((int)currentBmExp + 5) % 6;
    setExpressionIndex(prev);
  }
}

// ---------- Arduino Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n========================================================");
  Serial.println("  DESK BUGGY: BMO & BAYMAX DUAL CHARACTER ANIMATION");
  Serial.println("========================================================");
  Serial.println("Interactive Serial Commands:");
  Serial.println("  'c' / 'm' : Switch Character (BMO <-> Baymax)");
  Serial.println("  '1'       : Select BMO Mode directly");
  Serial.println("  '2'       : Select BAYMAX Mode directly");
  Serial.println("  0..8      : Jump to Expression in active mode");
  Serial.println("              [Baymax: 0:Normal 1:Happy 2:Skeptical");
  Serial.println("                      3:Compassion 4:Tired 5:Sleep");
  Serial.println("                      6:Scan 7:Fistbump]");
  Serial.println("  'n'/SPACE : Next Expression");
  Serial.println("  'p'       : Previous Expression");
  Serial.println("  'a'       : Toggle Auto Story Loop");
  Serial.println("  'b'       : Force Blink");
  Serial.println("  's'       : Trigger Health Scan / Fistbump");
  Serial.println("========================================================");

#if BUZZER_PIN >= 0
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

#if FP_TOUCH_PIN >= 0
  pinMode(FP_TOUCH_PIN, INPUT_PULLDOWN);
#endif

  // Initialize 1.8" ST7735 TFT
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(DISPLAY_ROTATION);
  W = tft.width();
  H = tft.height();

  // Create double buffer in RAM
  cv = new GFXcanvas16(W, H);

  // Optional fingerprint sensor check
  fpSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  finger.begin(57600);
  delay(50);
  sensorFound = finger.verifyPassword();
  if (sensorFound) {
    Serial.println("[+] AS608 Fingerprint Sensor Detected!");
    finger.LEDcontrol(true);
  }

  // Start with Baymax (or BMO)
  switchCharacter(CHAR_BAYMAX);
  setExpressionIndex(BM_NEUTRAL);

  nextBlinkTime = millis() + 2000;
  nextGlanceTime = millis() + 3500;
}

void loop() {
  uint32_t now = millis();

  // 1. Process Serial Commands
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r' || ch == '\n' || ch == '\0') continue;

    if (ch == 'c' || ch == 'C' || ch == 'm' || ch == 'M') {
      autoAnimate = false;
      switchCharacter(currentMode == CHAR_BMO ? CHAR_BAYMAX : CHAR_BMO);
    } else if (ch == '1') {
      autoAnimate = false;
      switchCharacter(CHAR_BMO);
    } else if (ch == '2') {
      autoAnimate = false;
      switchCharacter(CHAR_BAYMAX);
    } else if (ch >= '0' && ch <= '8') {
      autoAnimate = false;
      setExpressionIndex(ch - '0');
    } else if (ch == ' ' || ch == 'n' || ch == 'N') {
      autoAnimate = false;
      nextExpression();
    } else if (ch == 'p' || ch == 'P') {
      autoAnimate = false;
      prevExpression();
    } else if (ch == 'a' || ch == 'A') {
      autoAnimate = !autoAnimate;
      Serial.print("Auto Story Mode: ");
      Serial.println(autoAnimate ? "ON" : "OFF");
    } else if (ch == 'b' || ch == 'B') {
      // Trigger Blink
      isBlinking = true;
      blinkStage = 1;
      blinkStageTime = now + 40;
    } else if (ch == 's' || ch == 'S') {
      autoAnimate = false;
      if (currentMode == CHAR_BAYMAX) {
        setExpressionIndex(BM_SCAN);
      } else {
        bmoChirp();
      }
    }
  }

  // 2. Hardware Touch / Fingerprint interaction
  if (sensorFound && (now - lastTouchTime > 300)) {
    if (finger.getImage() == FINGERPRINT_OK) {
      if (now - lastTouchTime < 700) {
        // Double tap: Switch character!
        switchCharacter(currentMode == CHAR_BMO ? CHAR_BAYMAX : CHAR_BMO);
        delay(250);
      } else {
        // Single tap: Character celebratory reaction!
        if (currentMode == CHAR_BAYMAX) {
          setExpressionIndex(BM_FISTBUMP);
        } else {
          setExpressionIndex(BMO_LAUGH);
          bmoChirp();
        }
      }
      lastTouchTime = now;
      delay(200);
    }
  }

  // 3. Auto Story / Mood Transition Loop
  if (autoAnimate) {
    uint32_t dur = 4000;
    if (currentMode == CHAR_BMO) {
      if (currentBmoExp == BMO_GRIT) dur = 2500;
      else if (currentBmoExp == BMO_SING) dur = 4500;
      else if (currentBmoExp == BMO_WORRIED) dur = 3500;
      else if (currentBmoExp == BMO_LAUGH) dur = 3000;
    } else {
      if (currentBmExp == BM_SCAN) dur = 3200;
      else if (currentBmExp == BM_FISTBUMP) dur = 3000;
      else if (currentBmExp == BM_TIRED) dur = 4500;
      else if (currentBmExp == BM_SLEEPING) dur = 3500;
    }

    if (now >= expStartTime && (now - expStartTime > dur)) {
      expStartTime = now;
      nextExpression();
    }
  }

  // 4. Natural Blinking Animation
  if (!isBlinking && now > nextBlinkTime) {
    isBlinking = true;
    blinkStage = 1; // Stage 1: Half lid
    blinkStageTime = now + (currentMode == CHAR_BAYMAX ? 50 : 30);
    nextBlinkTime = now + 2500 + (random(0, 3000));
  }

  if (isBlinking && now > blinkStageTime) {
    if (currentMode == CHAR_BAYMAX) {
      // Smooth 4-stage Baymax blink: Half -> Closed -> Half -> Open
      if (blinkStage == 1) {
        blinkStage = 2; // Fully closed
        blinkStageTime = now + 75;
      } else if (blinkStage == 2) {
        blinkStage = 3; // Reopening half
        blinkStageTime = now + 45;
      } else {
        blinkStage = 0; // Fully open
        isBlinking = false;
      }
    } else {
      // BMO Quick 2-stage blink
      if (blinkStage == 1) {
        blinkStage = 2;
        blinkStageTime = now + 90;
      } else {
        blinkStage = 0;
        isBlinking = false;
      }
    }
  }

  // 5. Idle Eye Glance & Breathing Motion
  if (now > nextGlanceTime) {
    int r = random(0, 5);
    if (r == 0) glanceX = -3;
    else if (r == 1) glanceX = 3;
    else glanceX = 0;
    nextGlanceTime = now + 1800 + random(0, 2500);
  }

  // Soft Baymax breathing bob (organic vinyl inflatable motion)
  if (currentMode == CHAR_BAYMAX) {
    breatheY = (int)(sinf((float)now / 420.0f) * 1.5f);
  } else {
    breatheY = 0;
  }

  // 6. Character Specific Animations
  if (currentMode == CHAR_BMO) {
    // A. Mouth flap animation
    if (now - lastMouthTick > 140) {
      lastMouthTick = now;
      if (currentBmoExp == BMO_WINK || currentBmoExp == BMO_LAUGH || currentBmoExp == BMO_SING) {
        mouthAnim += mouthAnimDir;
        if (mouthAnim >= 3) mouthAnimDir = -1;
        else if (mouthAnim <= 0) mouthAnimDir = 1;
      } else {
        mouthAnim = 0;
      }
    }

    // B. Musical notes for Singing
    if (currentBmoExp == BMO_SING) {
      if (now - lastNoteTick > 40) {
        lastNoteTick = now;
        noteY = (noteY + 2) % 45;
      }
    } else {
      noteY = 0;
    }

    // C. Gritting teeth vibration shake
    if (currentBmoExp == BMO_GRIT) {
      shakeX = random(-1, 2);
      shakeY = random(-1, 2);
    } else {
      shakeX = 0;
      shakeY = 0;
    }

    // D. Worried teardrop dripping animation
    if (currentBmoExp == BMO_WORRIED) {
      if (now - lastTearTick > 60) {
        lastTearTick = now;
        tearY += 2;
        if (tearY > 30) tearY = 0;
      }
    } else {
      tearY = 0;
    }

  } else { // CHAR_BAYMAX
    // A. Low Battery Wobble
    if (currentBmExp == BM_TIRED) {
      shakeX = (int)(sinf((float)now / 220.0f) * 2.5f);
      shakeY = 0;
    } else {
      shakeX = 0;
      shakeY = 0;
    }

    // B. Medical Laser Scanner Sweep
    if (currentBmExp == BM_SCAN) {
      scanAnimY += scanAnimDir;
      if (scanAnimY >= H - 6) {
        scanAnimDir = -2;
      } else if (scanAnimY <= 6) {
        scanAnimDir = 2;
      }
    }

    // C. Fist Bump Animation Tick
    if (currentBmExp == BM_FISTBUMP) {
      if (now - lastFistBumpTick > 150) {
        lastFistBumpTick = now;
        fistBumpFrame = (fistBumpFrame + 1) % 6;
      }
    }
  }

  // 7. Render complete double-buffered frame
  if (currentMode == CHAR_BMO) {
    drawBmoFace();
  } else {
    drawBaymaxFace();
  }

  render();

  delay(30); // ~33 FPS smooth retro animation
}
