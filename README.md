# Desk Buggy 🤖 — BMO & Baymax Dual-Character Animation

A retro animated desktop companion featuring **BMO** (*Adventure Time*) and **Baymax** (*Big Hero 6*), powered by ESP32 and a 1.8" ST7735 TFT SPI display.

Both characters are completely distinct and faithfully recreated with smooth, organic ~33 FPS animations rendered in RAM via `GFXcanvas16` double-buffering.

---

## ✨ Characters & Expressions

### 🤍 BAYMAX (*Big Hero 6*)
Rendered on a seamless **pure white full display** with Baymax's iconic black eyes, connecting horizontal bridge, and authentic multi-stage eyelid physics (no circle boundary):

1. `0` **Neutral / Awake**: Classic wide open solid black circular eyes with connecting bridge line.
2. `1` **Happy / Satisfied Smile**: Cheeks push up lower eyelids (top half solid black, bottom half contoured outline).
3. `2` **Skeptical / Squint**: Focused diagnostic squint with inward-slanted eyelids (`\ /`).
4. `3` **Compassionate / Sad**: Empathetic puppy-dog eyes with outward-slanted eyelids (`/ \`) for when you are experiencing distress.
5. `4` **Low Battery / Tired**: Heavy drooping upper eyelids hanging across the eyes with a sleepy side-to-side wobble.
6. `5` **Sleeping / Resting**: Fully closed eyes with subtle outline and the connecting bridge.
7. `6` **Medical Vitality Scan**: Animated cyan diagnostic laser beam sweeping up and down with multi-frequency scanner audio.
8. `7` **Fist Bump ("Ba-la-la-la-la!")**: Joyful heart badge animation, celebratory chime arpeggio, and smiling eyes!

#### Baymax Living Animations:
- **Organic Inflatable Breathing**: Subtle, soft sinusoidal vertical bobbing that brings Baymax to life.
- **Natural Multi-Stage Blink**: Transitions smoothly from Open ➔ Half-Lid ➔ Closed ➔ Half-Lid ➔ Open.
- **Low-Battery Wobble**: Gentle drunken sinusoidal swaying when exhausted.
- **Curious Glances**: Looks left and right naturally during idle moments.

---

### 💚 BMO (*Adventure Time*)
Classic mint-green console body with dark screen bezel, floppy cartridge slot, circular port, and 9 expressive facial modes:

1. `0` **Grit / Strain**: Clenched squint eyes `> <`, rectangular grimacing teeth grid, and screen shake vibration.
2. `1` **Wink / Talk**: Dot eye, playful wink curve, and animated open talking bean mouth.
3. `2` **Sing / Shock**: Closed happy eyes `^ ^`, large round "O" mouth, and floating musical notes (♪ ♫).
4. `3` **Content**: Closed smiling eyes `^ ^` with sweet gentle curved smile.
5. `4` **Grin**: Classic dot eyes with wide open crescent grin.
6. `5` **Laugh :D**: Chuckling open mouth `:D` with white teeth band and bouncing belly laugh.
7. `6` **Cute Smile**: Signature BMO smile with natural blinks and idle glances.
8. `7` **Worried / Crying**: Slanted sad eyes `\ /`, quivering wavy mouth, and falling teardrops.
9. `8` **Deadpan / Neutral**: Solid dot eyes with flat horizontal line `—`.

---

## ⌨️ Serial Interactive Commands (115200 baud)

Open the Serial Monitor to interact with your Desk Buggy:

| Command | Action |
|---|---|
| `c` or `m` | **Switch Character Mode** (BMO ⟷ Baymax) |
| `1` | Select **BMO** mode directly |
| `2` | Select **Baymax** mode directly |
| `0` – `8` | Jump to expression in active character |
| `SPACE` or `n` | Step to the **next** expression |
| `p` | Step to the **previous** expression |
| `a` | Toggle **Autonomous Life Mode** ON / OFF |
| `b` | Trigger a manual smooth blink |
| `s` | Trigger **Medical Scan** (Baymax) or **Chirp** (BMO) |

---

## 👆 Hardware Touch & Fingerprint Interactions (AS608)

- **Single Tap**:
  - In **Baymax Mode**: Triggers the iconic **Fist Bump** ("Ba-la-la-la-la!") with heart animation and celebratory audio!
  - In **BMO Mode**: BMO giggles happily (`EXP_LAUGH`) with a double chirp.
- **Double Tap** (within 600ms):
  - Instantly toggles between **BMO** and **Baymax**!

---

## 🔌 Hardware Wiring

### ESP32 to 1.8" ST7735 TFT SPI Display

| ST7735 Pin | ESP32 GPIO | Description |
|---|---|---|
| **CS** | **GPIO 5** | Chip Select |
| **A0 / DC** | **GPIO 2** | Data / Command |
| **RESET / RST** | **GPIO 4** | Hardware Reset |
| **SDA / MOSI** | **GPIO 23** | SPI Master Out |
| **SCK / SCLK** | **GPIO 18** | SPI Clock |
| **VCC** | **3.3V** or **5V** | Power |
| **GND** | **GND** | Ground |

### Optional Peripherals

| Peripheral | ESP32 GPIO | Note |
|---|---|---|
| **Buzzer** | **GPIO 13** | Active or Passive buzzer |
| **AS608 Fingerprint TX** | **GPIO 16** | ESP32 RX2 |
| **AS608 Fingerprint RX** | **GPIO 17** | ESP32 TX2 |

---

## 🚀 Building & Flashing

```bash
# Build firmware
pio run

# Flash to ESP32
pio run -t upload

# Open Serial Monitor
pio device monitor -b 115200
```

---

## 🖥️ Sub-Projects for ESP32-C3 SuperMini + 0.96" OLED

### 1. Macintosh Clock & Temp HUD ([`macintosh-clock-temp/`](macintosh-clock-temp/))
- **Features:** Real-time clock HUD, live hardware core temperature readout (°C/°F), moving average filter, live 64-sample temperature history graph, and optional Wi-Fi NTP auto-sync.
- **Display Modes:** System Status Window (``), Giant Retro Digital Clock & Temp, and Real-time Scrolling Temp Graph.
- See [`macintosh-clock-temp/README.md`](macintosh-clock-temp/README.md) for full documentation, wiring, and serial commands.

### 2. Macintosh Desk Buddy ([`macintosh-desk-buddy/`](macintosh-desk-buddy/))
- **Target MCU:** ESP32-C3 SuperMini (RISC-V)
- **Display:** 0.96" I2C OLED (SSD1306, 128x64) on SDA=GPIO 8, SCL=GPIO 9
- **Features:** 7 retro 1984 Macintosh animated moods (Happy Mac, Wink, Thinking with rotating watch cursor, Classic System 1.0 Desktop with mouse cursor, Sleepy Zzz, and cursive "hello." boot intro).
- See [`macintosh-desk-buddy/README.md`](macintosh-desk-buddy/README.md) for full documentation and wiring.

### 3. DOOM 60 FPS Engine ([`doom-oled/`](doom-oled/))
- **Target MCU:** ESP32-C3 SuperMini (RISC-V @ 160MHz)
- **Display:** 0.96" I2C OLED (SSD1306, 128x64) on SDA=GPIO 8, SCL=GPIO 9
- **Features:** 3D DDA Raycaster engine casting 128 rays, depth dithering, directional lighting, 3D Imp & Barrel sprites, double-barrel shotgun with recoil/muzzle flash/pump reload, animated Doomguy status bar face, live 60 FPS counter, procedural PSX DOOM Fire simulator, and autonomous marine bot AI.
- See [`doom-oled/README.md`](doom-oled/README.md) for full documentation, controls, and wiring.

