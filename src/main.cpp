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

// ================= PALETTE (RGB565) ==================
// Sampled authentically from the BMO scan
#define C_CASE     0x85D5   // RGB(135, 186, 172) - BMO casing mint body
#define C_SCREEN   0xD7FC   // RGB(210, 252, 228) - Light mint cyan screen
#define C_DARK     0x1124   // RGB(20, 38, 34)    - Dark navy / screen bezel & facial lines
#define C_MOUTH    0x446B   // RGB(70, 142, 90)   - BMO green tongue / mouth interior
#define C_WHITE    0xFFFF   // RGB(255, 255, 255) - Teeth / highlights
#define C_TEAR     0x6E5C   // RGB(110, 200, 225) - Crying teardrop cyan
#define C_SHADOW   0xB73A   // RGB(182, 230, 210) - Screen bevel shadow
#define C_NOTE     0x1944   // RGB(25, 42, 38)    - Floating musical note
// =====================================================

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 *cv = nullptr;

HardwareSerial fpSerial(2);
Adafruit_Fingerprint finger(&fpSerial);
bool sensorFound = false;

// Screen geometry for 160x128
int W = 160;
int H = 128;
int scrX = 14;
int scrY = 8;
int scrW = 132;
int scrH = 92;
int scrR = 7;

int eyeY = 42;
int eyeLX = 40;
int eyeRX = 120;
int mouthX = 80;
int mouthY = 62;

// 9 Iconic Expressions from the user's scan
enum Expression {
  EXP_GRIT = 0,     // Row 0, Col 0: Clenched squint eyes & gritted teeth grimace
  EXP_WINK = 1,     // Row 0, Col 1: Dot eye + wink + open bean talking mouth
  EXP_SING = 2,     // Row 0, Col 2: Closed happy eyes + large round "O" singing mouth
  EXP_CONTENT = 3,  // Row 1, Col 0: Closed smiling eyes + gentle sweet smile
  EXP_GRIN = 4,     // Row 1, Col 1: Dot eyes + wide open crescent grin
  EXP_LAUGH = 5,    // Row 1, Col 2: Wide dot eyes + laughing :D with white teeth & tongue
  EXP_SMILE = 6,    // Row 2, Col 0: Classic cute BMO face: dot eyes + cute smile
  EXP_WORRIED = 7,  // Row 2, Col 1: Slanted sad eyes + falling tears + trembling wavy mouth
  EXP_NEUTRAL = 8   // Row 2, Col 2: Dot eyes + deadpan flat line mouth
};

Expression currentExp = EXP_SMILE;
bool autoAnimate = true;

// Animation state
bool isBlinking = false;
uint32_t blinkEndTime = 0;
uint32_t nextBlinkTime = 0;

int glanceX = 0;
int glanceY = 0;
uint32_t nextGlanceTime = 0;

int mouthAnim = 0;
int mouthAnimDir = 1;
uint32_t lastMouthTick = 0;

int shakeX = 0;
int shakeY = 0;

int tearY = 0;
uint32_t lastTearTick = 0;

int noteY = 0;
uint32_t lastNoteTick = 0;

// Auto showcase story timeline
uint32_t expStartTime = 0;
uint32_t expDuration = 4000;

// ---------- Buzzer Helpers ----------
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

void chirp() {
  buzzerTone(2600, 40);
  delay(30);
  buzzerTone(3200, 60);
}

// ---------- Drawing Primitives on Canvas ----------
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

// BMO Body, Screen Bezel, Drive Slot & Button
void drawBmoConsole(int sx = 0, int sy = 0) {
  // Casing body background
  cv->fillScreen(C_CASE);

  // Screen background (light mint cyan)
  cv->fillRoundRect(scrX + sx, scrY + sy, scrW, scrH, scrR, C_SCREEN);

  // Inner bevel shadow (subtle depth highlight)
  cv->drawFastHLine(scrX + 4 + sx, scrY + 3 + sy, scrW - 8, C_SHADOW);
  cv->drawFastHLine(scrX + 5 + sx, scrY + 4 + sy, scrW - 10, C_SHADOW);
  cv->drawFastVLine(scrX + 3 + sx, scrY + 4 + sy, scrH - 8, C_SHADOW);

  // Screen dark border (2px thick rounded rectangle)
  cv->drawRoundRect(scrX + sx, scrY + sy, scrW, scrH, scrR, C_DARK);
  cv->drawRoundRect(scrX + 1 + sx, scrY + 1 + sy, scrW - 2, scrH - 2, scrR - 1, C_DARK);

  // Floppy disk cartridge slot (bottom-left)
  cv->fillRoundRect(24 + sx, 112 + sy, 52, 6, 2, C_DARK);

  // Controller port / circular button (bottom-right)
  cv->fillCircle(136 + sx, 115 + sy, 5, C_DARK);
}

// Pixel art eyes
void drawDotEye(int cx, int cy, int r = 4) {
  // Crisp pixel dot with slightly rounded corners
  cv->fillRoundRect(cx - r, cy - r - 1, (r * 2) + 1, (r * 2) + 2, 2, C_DARK);
}

void drawArcEye(int cx, int cy, int w = 7, int h = 5, bool inverted = false) {
  // Closed smiling arc ^
  int prevX = cx - w;
  int prevY = cy + (inverted ? 0 : 0);
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

void drawSquintEye(int cx, int cy, bool pointingRight) {
  int dx = pointingRight ? 8 : -8;
  // Top horizontal crease
  drawThickLine(cx - 5, cy - 7, cx + 5, cy - 7, 2, C_DARK);
  // Wedge > or <
  drawThickLine(cx - dx, cy - 5, cx + dx, cy, 2, C_DARK);
  drawThickLine(cx + dx, cy, cx - dx, cy + 5, 2, C_DARK);
  // Bottom horizontal crease
  drawThickLine(cx - 5, cy + 7, cx + 5, cy + 7, 2, C_DARK);
}

void drawWinkEye(int cx, int cy) {
  // Playful wavy curve ~
  drawThickLine(cx - 7, cy - 4, cx - 1, cy + 1, 2, C_DARK);
  drawThickLine(cx - 1, cy + 1, cx + 7, cy - 2, 2, C_DARK);
}

void drawBlinkEye(int cx, int cy) {
  drawThickLine(cx - 6, cy, cx + 6, cy, 2, C_DARK);
}

// Floating pixel-art musical note ♪ and ♫
void drawNote(int nx, int ny, int type) {
  if (type == 0) {
    // Single eighth note ♪
    cv->fillRect(nx, ny + 4, 4, 3, C_DARK);
    cv->drawFastVLine(nx + 3, ny, 6, C_DARK);
    cv->drawFastHLine(nx + 3, ny, 3, C_DARK);
  } else {
    // Double eighth note ♫
    cv->fillRect(nx, ny + 5, 3, 3, C_DARK);
    cv->fillRect(nx + 8, ny + 5, 3, 3, C_DARK);
    cv->drawFastVLine(nx + 2, ny, 7, C_DARK);
    cv->drawFastVLine(nx + 10, ny, 7, C_DARK);
    cv->drawFastHLine(nx + 2, ny, 9, C_DARK);
  }
}

// Teardrop
void drawTeardrop(int tx, int ty) {
  cv->fillRoundRect(tx - 2, ty, 5, 8, 2, C_TEAR);
  cv->drawPixel(tx, ty - 1, C_TEAR);
}

// Draw the current expression and animation details
void drawCurrentFace() {
  int lx = eyeLX + glanceX + shakeX;
  int rx = eyeRX + glanceX + shakeX;
  int ly = eyeY + glanceY + shakeY;
  int ry = eyeY + glanceY + shakeY;
  int mx = mouthX + shakeX;
  int my = mouthY + shakeY;

  // Base BMO Casing
  drawBmoConsole(shakeX, shakeY);

  // 1. Draw Eyes
  if (isBlinking && currentExp != EXP_GRIT && currentExp != EXP_SING && currentExp != EXP_CONTENT) {
    drawBlinkEye(lx, ly);
    drawBlinkEye(rx, ry);
  } else {
    switch (currentExp) {
      case EXP_GRIT:
        drawSquintEye(lx, ly, true);
        drawSquintEye(rx, ry, false);
        break;

      case EXP_WINK:
        drawDotEye(lx, ly, 4);
        drawWinkEye(rx, ry);
        break;

      case EXP_SING:
        drawArcEye(lx, ly - (mouthAnim / 2), 6, 4, false);
        drawArcEye(rx, ry - (mouthAnim / 2), 6, 4, false);
        break;

      case EXP_CONTENT:
        drawArcEye(lx, ly, 7, 5, false);
        drawArcEye(rx, ry, 7, 5, false);
        break;

      case EXP_WORRIED:
        // Slanted sad eyes \ /
        drawThickLine(lx - 4, ly - 6, lx + 4, ly + 6, 3, C_DARK);
        drawThickLine(rx + 4, ry - 6, rx - 4, ry + 6, 3, C_DARK);
        // Falling teardrops
        if (tearY > 0) {
          drawTeardrop(lx + 1, ly + 6 + tearY);
          drawTeardrop(rx - 1, ry + 6 + tearY);
        }
        break;

      case EXP_LAUGH:
        drawDotEye(lx, ly, 5);
        drawDotEye(rx, ry, 5);
        break;

      case EXP_GRIN:
      case EXP_SMILE:
      case EXP_NEUTRAL:
      default:
        drawDotEye(lx, ly, 4);
        drawDotEye(rx, ry, 4);
        break;
    }
  }

  // 2. Draw Mouths
  switch (currentExp) {
    case EXP_GRIT: {
      // Clenched teeth grimace: wide rectangle with teeth grid
      int mw = 26;
      int mh = 11;
      cv->fillRect(mx - mw, my - mh, mw * 2, mh * 2, C_WHITE);
      cv->drawRect(mx - mw, my - mh, mw * 2, mh * 2, C_DARK);
      cv->drawRect(mx - mw - 1, my - mh - 1, (mw * 2) + 2, (mh * 2) + 2, C_DARK);
      // Horizontal teeth division line
      drawThickHLine(mx - mw, my, mw * 2, 2, C_DARK);
      // Vertical divider lines
      for (int tx = mx - mw + 7; tx < mx + mw; tx += 7) {
        cv->drawFastVLine(tx, my - mh + 1, mh * 2 - 2, C_DARK);
      }
      break;
    }

    case EXP_WINK: {
      // Tilted open bean / D talking mouth with tongue
      int mo = mouthAnim;
      int bw = 10 + mo;
      int bh = 14 + mo;
      cv->fillRoundRect(mx - 8, my - 6, bw * 2, bh, 6, C_MOUTH);
      cv->drawRoundRect(mx - 8, my - 6, bw * 2, bh, 6, C_DARK);
      cv->drawRoundRect(mx - 9, my - 7, (bw * 2) + 2, bh + 2, 7, C_DARK);
      // White top teeth row
      cv->fillRect(mx - 6, my - 5, bw + 4, 3, C_WHITE);
      break;
    }

    case EXP_SING: {
      // Big open "O" singing oval with tongue
      int rw = 10 + (mouthAnim / 2);
      int rh = 14 + mouthAnim;
      // Filled oval
      for (int dy = -rh; dy <= rh; dy++) {
        float norm = (float)dy / (float)rh;
        int dx = (int)(rw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, my + dy + 2, (dx * 2) + 1, C_MOUTH);
      }
      // Outline
      for (int dy = -rh; dy <= rh; dy++) {
        float norm = (float)dy / (float)rh;
        int dx = (int)(rw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawPixel(mx - dx, my + dy + 2, C_DARK);
        cv->drawPixel(mx + dx, my + dy + 2, C_DARK);
        cv->drawPixel(mx - dx - 1, my + dy + 2, C_DARK);
        cv->drawPixel(mx + dx + 1, my + dy + 2, C_DARK);
      }
      // Floating musical notes
      if (noteY > 0) {
        drawNote(scrX + 18, scrY + 68 - noteY, 0);
        drawNote(scrX + scrW - 24, scrY + 60 - noteY, 1);
      }
      break;
    }

    case EXP_CONTENT: {
      // Gentle sweet curved smile line
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

    case EXP_GRIN: {
      // Open happy crescent grin
      int gw = 15;
      int gh = 10;
      // Fill bottom half circle
      for (int dy = 0; dy <= gh; dy++) {
        float norm = (float)dy / (float)gh;
        int dx = (int)(gw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, my + dy - 1, (dx * 2) + 1, C_MOUTH);
      }
      // Top flat line
      drawThickHLine(mx - gw, my - 2, (gw * 2) + 1, 2, C_DARK);
      // Bottom curved outline
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

    case EXP_LAUGH: {
      // Big laughing mouth :D with white teeth row and tongue
      int lw = 17 + mouthAnim;
      int lh = 14 + mouthAnim;
      // Fill crescent
      for (int dy = 0; dy <= lh; dy++) {
        float norm = (float)dy / (float)lh;
        int dx = (int)(lw * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, my + dy - 2, (dx * 2) + 1, C_MOUTH);
      }
      // White top teeth band
      cv->fillRect(mx - lw + 2, my - 1, (lw * 2) - 3, 4, C_WHITE);
      // Top border
      drawThickHLine(mx - lw, my - 2, (lw * 2) + 1, 2, C_DARK);
      // Bottom curved outline
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

    case EXP_SMILE: {
      // Classic signature cute BMO smile
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

    case EXP_WORRIED: {
      // Quivering sad wavy mouth
      int ww = 13;
      int wh = 8;
      // Inverted crescent
      for (int dy = 0; dy <= wh; dy++) {
        float norm = (float)dy / (float)wh;
        int dx = (int)(ww * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawFastHLine(mx - dx, (my + 8) - dy, (dx * 2) + 1, C_MOUTH);
      }
      // Outlines
      for (int dy = 0; dy <= wh; dy++) {
        float norm = (float)dy / (float)wh;
        int dx = (int)(ww * sqrtf(max(0.0f, 1.0f - (norm * norm))));
        cv->drawPixel(mx - dx, (my + 8) - dy, C_DARK);
        cv->drawPixel(mx + dx, (my + 8) - dy, C_DARK);
      }
      drawThickHLine(mx - ww, my + 8, (ww * 2) + 1, 2, C_DARK);
      break;
    }

    case EXP_NEUTRAL: {
      // Deadpan straight flat line
      drawThickHLine(mx - 13, my, 27, 2, C_DARK);
      break;
    }
  }
}

// Push canvas to physical ST7735 display
void render() {
  if (cv != nullptr) {
    tft.drawRGBBitmap(0, 0, cv->getBuffer(), W, H);
  }
}

// Switch expression
void setExpression(Expression exp) {
  currentExp = exp;
  expStartTime = millis();
  tearY = 0;
  noteY = 0;
  shakeX = 0;
  shakeY = 0;
  mouthAnim = 0;
  Serial.print("BMO Expression -> ");
  Serial.println((int)exp);
}

// Next expression in sequence
void nextExpression() {
  int next = ((int)currentExp + 1) % 9;
  setExpression((Expression)next);
}

void prevExpression() {
  int prev = ((int)currentExp + 8) % 9;
  setExpression((Expression)prev);
}

// ---------- Arduino Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=========================================");
  Serial.println("  BMO 1.8\" TFT PIXEL ART ANIMATION");
  Serial.println("=========================================");
  Serial.println("Commands:");
  Serial.println("  0..8 : Jump to Expression");
  Serial.println("  'n'/SPACE : Next Expression");
  Serial.println("  'p'       : Previous Expression");
  Serial.println("  'a'       : Toggle Auto Story Loop");
  Serial.println("  'b'       : Force Blink");
  Serial.println("=========================================");

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
    chirp();
  }

  setExpression(EXP_SMILE);
  nextBlinkTime = millis() + 2500;
  nextGlanceTime = millis() + 3500;
  chirp();
}

void loop() {
  uint32_t now = millis();

  // 1. Process Serial Commands
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch >= '0' && ch <= '8') {
      autoAnimate = false;
      setExpression((Expression)(ch - '0'));
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
      isBlinking = true;
      blinkEndTime = now + 120;
    }
  }

  // 2. Hardware Touch / Fingerprint interaction
  if (sensorFound) {
    if (finger.getImage() == FINGERPRINT_OK) {
      // Touch detected! Happy excited reaction
      setExpression(EXP_LAUGH);
      chirp();
      delay(200);
    }
  }

  // 3. Auto Story / Mood Transition Loop
  if (autoAnimate) {
    // Dynamic durations per mood
    uint32_t dur = 4000;
    if (currentExp == EXP_GRIT) dur = 2500;
    else if (currentExp == EXP_SING) dur = 4500;
    else if (currentExp == EXP_WORRIED) dur = 3500;
    else if (currentExp == EXP_LAUGH) dur = 3000;

    if (now - expStartTime > dur) {
      nextExpression();
    }
  }

  // 4. Natural Blinking Animation
  if (!isBlinking && now > nextBlinkTime) {
    isBlinking = true;
    blinkEndTime = now + 130;
    nextBlinkTime = now + 2500 + (random(0, 3000));
  }
  if (isBlinking && now > blinkEndTime) {
    isBlinking = false;
  }

  // 5. Idle Eye Glance / Looking Around
  if (now > nextGlanceTime) {
    int r = random(0, 5);
    if (r == 0) glanceX = -3;
    else if (r == 1) glanceX = 3;
    else glanceX = 0;
    nextGlanceTime = now + 1500 + random(0, 2500);
  }

  // 6. Expression-Specific Dynamic Animations
  // A. Mouth flap animation for Talking & Laughing & Singing
  if (now - lastMouthTick > 140) {
    lastMouthTick = now;
    if (currentExp == EXP_WINK || currentExp == EXP_LAUGH || currentExp == EXP_SING) {
      mouthAnim += mouthAnimDir;
      if (mouthAnim >= 3) mouthAnimDir = -1;
      else if (mouthAnim <= 0) mouthAnimDir = 1;
    } else {
      mouthAnim = 0;
    }
  }

  // B. Musical notes for Singing
  if (currentExp == EXP_SING) {
    if (now - lastNoteTick > 40) {
      lastNoteTick = now;
      noteY = (noteY + 2) % 45;
    }
  } else {
    noteY = 0;
  }

  // C. Gritting teeth vibration shake
  if (currentExp == EXP_GRIT) {
    shakeX = random(-1, 2);
    shakeY = random(-1, 2);
  } else {
    shakeX = 0;
    shakeY = 0;
  }

  // D. Worried teardrop dripping animation
  if (currentExp == EXP_WORRIED) {
    if (now - lastTearTick > 60) {
      lastTearTick = now;
      tearY += 2;
      if (tearY > 30) tearY = 0;
    }
  } else {
    tearY = 0;
  }

  // 7. Render complete frame
  drawCurrentFace();
  render();

  delay(30); // ~33 FPS smooth retro animation
}
