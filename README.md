# 🤖 Desk Buggy — Smart Desktop Companion & Retro Display Suite

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%20%7C%20ESP32--C3-blue.svg?logo=espressif)](https://www.espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino%2FC%2B%2B-00979D.svg?logo=arduino)](https://www.arduino.cc/)
[![Build Tool: PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange.svg?logo=platformio)](https://platformio.org/)
[![FPS: 60 FPS](https://img.shields.io/badge/Performance-Up%20to%2060%20FPS-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

An open-source embedded graphics ecosystem featuring **four distinct projects** designed for the **ESP32** and **ESP32-C3 SuperMini** microcontrollers, driving color TFT and monochrome OLED displays with custom rendering engines.

---

## 🗂️ Project Portfolio

| Project | Target Microcontroller | Display Module | Frame Rate | Key Highlights | Location |
|---|---|---|---|---|---|
| **🤖 Desk Buggy** | ESP32-WROOM-32 (240MHz) | 1.8" ST7735 Color TFT (SPI) | ~33 FPS | BMO & Baymax animated companions, AS608 fingerprint touch gestures, buzzer audio | [`src/`](src/) |
| **🖥️ Macintosh Desk Buddy** | ESP32-C3 SuperMini (160MHz) | 0.96" SSD1306 OLED (I2C) | **60 FPS** | 7 retro 1984 Mac moods, animated cursive `"hello."`, System 1.0 desktop | [`macintosh-desk-buddy/`](macintosh-desk-buddy/) |
| **🌡️ Macintosh Clock & Temp HUD** | ESP32-C3 SuperMini (160MHz) | 0.96" SSD1306 OLED (I2C) | 30 FPS | Live clock, on-chip silicon core temp (°C/°F), 64-pt scrolling graph, NTP sync | [`macintosh-clock-temp/`](macintosh-clock-temp/) |
| **🔥 DOOM 60 FPS Engine** | ESP32-C3 SuperMini (160MHz) | 0.96" SSD1306 OLED (I2C) | **60 FPS** | 128-ray 3D DDA raycaster, depth dithering, animated shotgun, 3D imps, PSX fire | [`doom-oled/`](doom-oled/) |
| **☀️ Weather & Clock Station** | ESP32-C3 SuperMini (160MHz) | 0.96" SSD1306 OLED (I2C) | 33 FPS | Live Open-Meteo weather API, location badge, temp/humidity/wind, NTP clock, celestial sky arc | [`weather-clock-station/`](weather-clock-station/) |
| **🌈 Color TFT Weather Station**| ESP32-C3 SuperMini (160MHz) | 1.8" ST7735 Color TFT (SPI) | 30 FPS | Full 16-bit RGB565 color dashboard, Pune weather, NTP clock, auto-shuts OLED OFF | [`tft-weather-station/`](tft-weather-station/) |
| **🐱 Chibi Cute Face Motion**   | ESP32-C3 SuperMini (160MHz) | 0.96" SSD1306 OLED (I2C) | **60 FPS** | Sparkly anime eyes, ear twitches, sub-pixel breathing, blinks, blush, floating hearts | [`chibi-face/`](chibi-face/) |
| **⚡ Sleeping Pikachu Companion**| ESP32-C3 SuperMini (160MHz) | 0.96" SSD1306 OLED (I2C) | 33 FPS | Laying Pikachu snoozing, deep breathing, tail wags, cheek electric sparks, floating Zzz | [`pikachu-oled/`](pikachu-oled/) |

---

## 📚 Technical Design Specifications

Complete architectural, electrical, and mathematical design documents are available in the [`docs/`](docs/) directory:

- 🏛️ **[System Architecture (`docs/ARCHITECTURE.md`)](docs/ARCHITECTURE.md)** — Dual-MCU comparison, bus bandwidths (SPI vs 1.0 MHz I2C Fast Mode+), and memory layouts.
- 🔌 **[Hardware Design & Pinouts (`docs/HARDWARE_DESIGN.md`)](docs/HARDWARE_DESIGN.md)** — Complete wiring matrices, schematics, electrical power budgets, and pull-up resistor guides.
- 🎮 **[Engines Design Specification (`docs/ENGINES_DESIGN.md`)](docs/ENGINES_DESIGN.md)** — Breathing physics equations, DDA raycaster formulas, EMA temp filtering, and PSX fire cellular automata.

---

## ⚡ Master Hardware Pinouts

### 1. ESP32-WROOM-32 + 1.8" ST7735 TFT (Desk Buggy)
| ST7735 Pin | ESP32 GPIO | Function |
|---|---|---|
| **CS** | **GPIO 5** | SPI Chip Select |
| **A0 / DC** | **GPIO 2** | Data / Command |
| **RESET** | **GPIO 4** | Hardware Reset |
| **SDA / MOSI** | **GPIO 23** | SPI Master Out |
| **SCK / SCLK** | **GPIO 18** | SPI Clock |
| **AS608 TX / RX**| **GPIO 16 / 17** | Optical Fingerprint UART2 |
| **Buzzer (+)** | **GPIO 13** | PWM Audio Output |
| **VCC / GND** | **3.3V / GND** | Power Rails |

### 2. ESP32-C3 SuperMini + 0.96" SSD1306 OLED (Mac Desk Buddy / Clock / DOOM)
| SSD1306 OLED Pin | ESP32-C3 SuperMini Pin | Function |
|---|---|---|
| **SDA** | **GPIO 8** | I2C Data (Fast Mode+ 1.0 MHz) |
| **SCL** | **GPIO 9** | I2C Clock (Fast Mode+ 1.0 MHz) |
| **VCC** | **3.3V** | System Power |
| **GND** | **GND** | Ground |

---

## ⌨️ Serial Interactive Commands (115200 baud)

### Desk Buggy (BMO & Baymax)
- `c` or `m` — Toggle between **BMO** and **Baymax**
- `0` to `8` — Jump to specific facial expression
- `a` — Toggle autonomous idle breathing & blinking
- `s` — Trigger medical scan laser (Baymax) or chirp (BMO)
- `b` — Trigger natural multi-stage blink

### DOOM 60 FPS Engine
- `w` / `s` — Move forward / backward
- `a` / `d` — Rotate left / right
- `SPACE` or `f` — **Fire Double-Barrel Shotgun!**
- `m` — Toggle 2D radar mini-map overlay
- `auto` — Toggle autonomous marine bot patrol AI
- `1` / `2` / `3` — Switch between 3D Raycaster, PSX DOOM Fire, and Doomguy Showcase

### Macintosh Clock & Temp HUD
- `mode` — Cycle display modes (Window HUD, Giant Clock, Temp Graph)
- `time HH:MM:SS` — Set digital clock time manually
- `date YYYY-MM-DD` — Set date manually
- `wifi <SSID> <PASSWORD>` — Connect to Wi-Fi and sync via NTP internet time
- `temp` — Print live raw & filtered core temperatures

### Smart Weather & Clock Station
- `wifi <SSID> <PASSWORD>` — Connect to local Wi-Fi and auto-sync NTP & Open-Meteo
- `city <tokyo|london|ny|sf|blr|paris>` — Select city preset
- `loc <LAT> <LON> <CITY>` — Set custom GPS coordinates and city name
- `unit <C|F>` — Toggle Celsius / Fahrenheit
- `mode` — Cycle dashboard views (Main Station, Atmosphere Vitals, Celestial Sky Arc)
- `auto` — Toggle 8-second auto-cycling cards ON / OFF

---

## 🚀 Building & Flashing

This project uses [PlatformIO](https://platformio.org/).

### Flash Desk Buggy (ESP32-WROOM-32)
```bash
# Build & upload to ESP32
pio run -t upload
pio device monitor -b 115200
```

### Flash DOOM 60 FPS (ESP32-C3 SuperMini)
```bash
cd doom-oled
pio run -t upload
pio device monitor -b 115200
```

### Flash Macintosh Desk Buddy (ESP32-C3 SuperMini)
```bash
cd macintosh-desk-buddy
pio run -t upload
pio device monitor -b 115200
```

### Flash Macintosh Clock & Temp HUD (ESP32-C3 SuperMini)
```bash
cd macintosh-clock-temp
pio run -t upload
pio device monitor -b 115200
```

### Flash Weather & Clock Station (ESP32-C3 SuperMini)
```bash
cd weather-clock-station
pio run -t upload
pio device monitor -b 115200
```

---

## 🛠️ Repository Organization

```text
desk-buggy/
├── docs/                      # Architectural & Hardware Design Specs
│   ├── ARCHITECTURE.md        # System architecture, bus bandwidth, dual-MCU specs
│   ├── HARDWARE_DESIGN.md     # Wiring matrices, pinouts, and electrical budgets
│   └── ENGINES_DESIGN.md      # Rendering formulas, DDA raycaster, and state machines
├── src/                       # Project 1: Desk Buggy (BMO & Baymax 1.8" TFT)
├── doom-oled/                 # Project 2: DOOM 60 FPS 3D Raycaster (0.96" OLED)
├── macintosh-desk-buddy/      # Project 3: Macintosh Desk Buddy 60 FPS (0.96" OLED)
├── macintosh-clock-temp/      # Project 4: Macintosh Clock & Temp HUD (0.96" OLED)
├── weather-clock-station/     # Project 5: Smart Weather & Clock Station (0.96" OLED)
├── tools/                     # Helper flashing and port diagnosis utilities
├── platformio.ini             # Root PlatformIO configuration
└── README.md                  # Project landing page
```

---

## 📜 License
Distributed under the **MIT License**. Created with passion for retro computing and embedded systems.
