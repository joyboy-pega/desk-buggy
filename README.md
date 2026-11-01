# Desk Buggy 🤖 — BMO 1.8" TFT Pixel Art Animation

A retro pixel-art animated desktop companion (BMO from Adventure Time) powered by ESP32 and a 1.8" ST7735 TFT SPI display.

Features all **9 iconic facial expressions** faithfully matched from the original scan with dynamic expressions, blinking, idle eye wandering, talking chatter, singing with floating musical notes, crying tears, and screen-vibrating teeth gritting.

---

## ✨ Features

- **Double-Buffered Rendering**: Smooth ~33 FPS retro pixel animation rendered in RAM via `GFXcanvas16` with zero screen tearing or flicker.
- **Authentic BMO Case & Console**: Mint green casing, dark bezel, floppy drive slot, and circular controller port framing the screen.
- **9 Expressions**:
  1. `0` **Grit / Strain**: Clenched squint eyes `> <`, rectangular grimacing teeth grid, and screen shake vibration.
  2. `1` **Wink / Talk**: Dot eye, playful wink curve, and tilted open talking bean mouth.
  3. `2` **Sing / Shock**: Closed happy eyes `^ ^`, large round "O" mouth, and floating musical notes (♪ ♫).
  4. `3` **Content**: Closed smiling eyes `^ ^` with sweet gentle curved smile.
  5. `4` **Grin**: Classic dot eyes with wide open crescent grin.
  6. `5` **Laugh :D**: Chuckling open mouth `:D` with white teeth band and bouncing belly laugh.
  7. `6` **Cute Smile**: Signature BMO smile with natural blinks and idle glances.
  8. `7` **Worried / Crying**: Slanted sad eyes `\ /`, quivering wavy mouth, and falling teardrops.
  9. `8` **Deadpan / Neutral**: Solid dot eyes with flat horizontal line `—`.
- **Autonomous Story Mode**: Cycles moods and expressions naturally over time.
- **Touch / Fingerprint Sensor Integration**: Reacts with a happy laugh and chirp when an AS608 optical fingerprint sensor is touched.
- **Serial Controls**: Live control via USB Serial at 115200 baud.

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

## ⌨️ Serial Interactive Commands (115200 baud)

Open the Serial Monitor in VSCode/PlatformIO to control Desk Buggy:

- `0` – `8` : Jump directly to any of the 9 expressions
- `n` or `SPACE` : Step to the next expression
- `p` : Step to the previous expression
- `a` : Toggle Autonomous Life Mode ON/OFF
- `b` : Trigger a manual blink

---

## 🚀 Building & Flashing

This project uses [PlatformIO](https://platformio.org/).

### Clone & Build
```bash
git clone https://github.com/joyboy-pega/desk-buggy.git
cd desk-buggy

# Build firmware
pio run

# Flash to ESP32
pio run -t upload

# Open Serial Monitor
pio device monitor -b 115200
```
