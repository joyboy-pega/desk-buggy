# 🏛️ System Architecture Design

This document details the architectural blueprints, microcontroller specifications, bus topologies, and graphics rendering pipelines implemented across the **Desk Buggy** repository.

---

## 🗺️ System Overview Diagram

```mermaid
flowchart TD
    subgraph Host_Control ["🖥️ Development & Control"]
        PC["Host Machine (macOS / Linux / Windows)"]
        CLI["PlatformIO Core / Serial Terminal (115200 baud)"]
        PC --> CLI
    end

    subgraph ESP32_DualCore ["🤖 Project 1: Desk Buggy (BMO & Baymax)"]
        MCU1["ESP32-WROOM-32\n(Xtensa LX6 Dual-Core @ 240MHz)"]
        TFT["1.8\" ST7735 Color TFT\n(128x160 RGB565 via SPI @ 27MHz)"]
        FP["AS608 Optical Fingerprint\n(UART2: GPIO 16/17)"]
        SPK["Active Buzzer\n(PWM: GPIO 13)"]
        CANVAS["GFXcanvas16 Double-Buffer\n(40KB Internal SRAM)"]

        MCU1 --> CANVAS
        CANVAS -->|SPI Transaction| TFT
        MCU1 <-->|Hardware Serial| FP
        MCU1 -->|LEDC PWM Tone| SPK
    end

    subgraph ESP32_C3_OLED ["⚡ Projects 2, 3 & 4: ESP32-C3 SuperMini (0.96\" OLED)"]
        MCU2["ESP32-C3 SuperMini\n(32-bit RISC-V @ 160MHz)"]
        OLED["0.96\" SSD1306 OLED\n(128x64 1-bit Monochrome)"]
        I2C["I2C Fast Mode+\n(1.0 MHz Clock: GPIO 8 SDA, GPIO 9 SCL)"]

        PROJ2["Macintosh Desk Buddy\n(7 Retro Moods, 60 FPS Engine)"]
        PROJ3["Macintosh Clock & Temp\n(NTP Sync, Silicon Core Temp, 64-pt Graph)"]
        PROJ4["DOOM 60 FPS Raycaster\n(128-Ray DDA, 3D Sprites, PSX Fire)"]

        MCU2 --> I2C
        I2C --> OLED
        MCU2 --- PROJ2
        MCU2 --- PROJ3
        MCU2 --- PROJ4
    end

    CLI <==>|USB CDC / UART (115200)| MCU1
    CLI <==>|Native USB CDC (115200)| MCU2
```

---

## 🔬 Microcontroller Platform Comparison

| Specification | ESP32-WROOM-32 (Desk Buggy) | ESP32-C3 SuperMini (Desk Buddy / Clock / DOOM) |
|---|---|---|
| **CPU Core Architecture** | Dual-Core Xtensa® 32-bit LX6 | Single-Core 32-bit RISC-V (RV32IMC) |
| **Clock Frequency** | Up to **240 MHz** | Up to **160 MHz** |
| **Internal SRAM** | 520 KB | 400 KB (320 KB usable) |
| **Embedded Flash** | 4 MB SPI Flash | 4 MB Embedded SPI Flash (XMC) |
| **USB Interface** | External USB-to-UART bridge (CP2102/CH340) | Native On-Chip USB-Serial/JTAG CDC |
| **Primary Display** | 1.8" ST7735 SPI TFT (128x160, 16-bit color) | 0.96" SSD1306 I2C OLED (128x64, 1-bit monochrome) |
| **Bus Protocol** | Hardware SPI @ 27 MHz | Fast Mode+ I2C @ 1.0 MHz |
| **Input Sensors** | AS608 Optical Fingerprint (UART2) | Serial CDC Terminal / Touch GPIO |
| **Form Factor** | 30-pin DevKit board | Ultra-compact SuperMini (22.5 x 18 mm) |

---

## ⚡ Bus Protocols & Throughput Analysis

### 1. SPI Bus (1.8" ST7735 TFT)
- **Clock Frequency:** 27 MHz hardware SPI (`VSPI`).
- **Transfer Volume:** 128 x 160 pixels x 2 bytes/pixel = **40,960 bytes per frame**.
- **Theoretical Transfer Time:** $\frac{40960 \times 8}{27,000,000} \approx 12.1 \text{ ms}$.
- **Achieved Frame Rate:** **~33 FPS** with software double-buffering.

### 2. I2C Fast Mode+ Bus (0.96" SSD1306 OLED)
- **Standard I2C Speed:** 100 kHz (Slow, ~10 FPS).
- **Fast Mode:** 400 kHz (~35 FPS).
- **Our Fast Mode+ Configuration:** **1,000,000 Hz (1.0 MHz)** (`Wire.setClock(1000000)`).
- **Transfer Volume:** 128 x 64 pixels / 8 bits = **1,024 bytes per frame**.
- **I2C Protocol Overhead:** 1 start bit + 8 data bits + 1 ACK bit = 10 bit times per byte transmitted.
- **Frame Transfer Duration:** $\frac{1024 \times 10}{1,000,000} \approx 10.24 \text{ ms}$.
- **Achieved Frame Rate:** **60.0 FPS locked** with room for DDA raycasting computation ($\sim 2.5\text{ ms}$).

---

## 🎨 Graphics Rendering Pipelines

### Pipeline A: RGB565 In-Memory Double Buffering (Desk Buggy)
```text
[State Logic & Expression Triggers]
              │
              ▼
    [GFXcanvas16 Buffer (40KB)]
       ├── Background layer clearing
       ├── Sinusoidal breathing offset
       ├── Organic eyelid contouring
       └── Facial feature & tear render
              │
              ▼
   [SPI DMA / Burst Transmission]
              │
              ▼
   [ST7735 TFT Active Panel]
```
- Eliminates screen tearing and pixel flickering completely.
- Smooth natural transitions between expressions.

---

### Pipeline B: 1-Bit Monochromatic Raycasting & DDA Buffer (DOOM 60 FPS)
```text
   [Player Input / Autonomous AI Navigator]
                     │
                     ▼
  [128-Ray DDA Raycasting Engine (RV32IMC)]
       ├── Column stepping & side detection
       ├── Perpendicular wall distance
       └── 128-entry Z-Buffer array populated
                     │
                     ▼
  [3D Sprite Billboard Sorter & Projector]
       ├── Distance sorting (Nearest to Furthest)
       └── Z-Buffer occlusion testing & scaling
                     │
                     ▼
  [Depth-Aware Dither Generator]
       ├── < 2.0 dist: Solid wall / side-stripes
       ├── 2.0-4.2 dist: 50% checkerboard dither
       └── 4.2-7.0 dist: 25% sparse dither
                     │
                     ▼
  [HUD Overlay & Weapon Bob/Recoil Compositor]
                     │
                     ▼
  [1 MHz I2C Fast Mode+ Stream to SSD1306 Panel]
```
