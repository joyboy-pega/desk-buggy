# 🔌 Hardware Design & Pinout Specifications

Complete wiring tables, schematic diagrams, electrical characteristics, and pin multiplexing for all projects in the **Desk Buggy** ecosystem.

---

## 📋 Master Pinout Matrix

| Peripheral Pin | Target Signal | ESP32-WROOM-32 (Desk Buggy) | ESP32-C3 SuperMini (Mac / DOOM) | Notes |
|---|---|---|---|---|
| **Display SDA / MOSI** | SPI MOSI / I2C SDA | **GPIO 23** (SPI MOSI) | **GPIO 8** (I2C SDA) | High-speed data line |
| **Display SCL / SCK** | SPI SCLK / I2C SCL | **GPIO 18** (SPI SCK) | **GPIO 9** (I2C SCL) | Clock line (1MHz on C3) |
| **Display CS** | SPI Chip Select | **GPIO 5** | *N/A (I2C uses 0x3C)* | Active LOW |
| **Display DC / A0** | Data / Command | **GPIO 2** | *N/A (I2C control byte)* | LOW = Command, HIGH = Data |
| **Display RESET** | Hardware Reset | **GPIO 4** | *N/A (Internal power-on reset)* | Active LOW reset pulse |
| **AS608 Fingerprint TX** | UART RX | **GPIO 16** (RX2) | *Optional* | 57600 baud serial protocol |
| **AS608 Fingerprint RX** | UART TX | **GPIO 17** (TX2) | *Optional* | 3.3V logic level compatible |
| **Buzzer (+)** | PWM / Tone | **GPIO 13** | *Optional GPIO 2* | Active or passive piezo |
| **Display VCC** | Power Supply | **3.3V** or **5V** (VIN) | **3.3V** | 3.3V recommended for OLED |
| **Display GND** | Ground | **GND** | **GND** | Common reference ground |

---

## 🛠️ Circuit 1: ESP32-WROOM-32 + 1.8" ST7735 TFT (Desk Buggy)

### Pin Connections
```text
           ESP32-WROOM-32 (30-pin)
               ┌───────────────┐
           3V3 │ [1]      [30] │ GND ─── ST7735 GND / Buzzer (-)
               │               │
      GPIO 23  │ [8]      [23] │ GPIO 13 ─── Buzzer (+)
  (SPI MOSI) ── ST7735 SDA     │
      GPIO 18  │ [9]      [20] │ GPIO 16 ─── AS608 TX
  (SPI SCLK) ── ST7735 SCK     │
      GPIO 5   │ [10]     [19] │ GPIO 17 ─── AS608 RX
    (SPI CS) ── ST7735 CS      │
      GPIO 2   │ [11]          │
      (D/C)  ── ST7735 A0      │
      GPIO 4   │ [12]          │
    (RESET)  ── ST7735 RESET   │
               └───────────────┘
```

### Wiring Guide
1. **ST7735 TFT SPI Display:**
   - `VCC` ➔ `3.3V` or `5V` (depending on display module voltage regulator)
   - `GND` ➔ `GND`
   - `CS` ➔ `GPIO 5`
   - `RESET` ➔ `GPIO 4`
   - `A0 / DC` ➔ `GPIO 2`
   - `SDA / MOSI` ➔ `GPIO 23`
   - `SCK / SCLK` ➔ `GPIO 18`
   - `LED / BLK` (Backlight) ➔ `3.3V` (or via 100Ω current limiting resistor)

2. **AS608 Optical Fingerprint Sensor:**
   - `VCC` ➔ `3.3V`
   - `GND` ➔ `GND`
   - `TX` ➔ `GPIO 16` (ESP32 RX2)
   - `RX` ➔ `GPIO 17` (ESP32 TX2)

3. **Audio Buzzer:**
   - `Positive (+)` ➔ `GPIO 13`
   - `Negative (-)` ➔ `GND`

---

## ⚡ Circuit 2: ESP32-C3 SuperMini + 0.96" SSD1306 OLED

The **ESP32-C3 SuperMini** is an ultra-compact single-core RISC-V development board measuring just 22.5 x 18 mm.

```text
               ESP32-C3 SuperMini
                 ┌──────────────┐
                 │   [USB-C]    │
                 │              │
           3.3V ─┤ 3V3      GND ├─ GND ─── SSD1306 GND
      SSD1306    │              │
       VCC ──────┤ 5V      IO9  ├─ GPIO 9 ─ SSD1306 SCL (I2C Clock)
                 │ IO0     IO8  ├─ GPIO 8 ─ SSD1306 SDA (I2C Data)
                 │ IO1     IO7  │
                 │ IO2     IO6  │
                 │ IO3     IO5  │
                 │ IO4          │
                 └──────────────┘
```

### Direct Pin Connections
| 0.96" OLED Pin | SuperMini Pin | Function |
|---|---|---|
| **VCC** | **3V3** | 3.3V System Power |
| **GND** | **GND** | Ground |
| **SCL** | **IO9 (GPIO 9)** | I2C SCL Clock |
| **SDA** | **IO8 (GPIO 8)** | I2C SDA Data |

> [!TIP]
> The internal pull-up resistors on the ESP32-C3 I2C pins are enabled by the `Wire` library. Most 0.96" SSD1306 OLED breakout boards also contain onboard 4.7kΩ pull-up resistors to 3.3V, ensuring signal integrity even at 1.0 MHz bus frequencies.

---

## 🔋 Electrical Power Budget

| Subsystem | Typical Active Current | Peak Inrush Current | Voltage Requirement |
|---|---|---|---|
| **ESP32-WROOM-32 (240MHz)** | 120 mA – 180 mA | 240 mA (Wi-Fi/BT active) | 3.0V – 3.6V (3.3V Nom) |
| **ESP32-C3 SuperMini (160MHz)**| 35 mA – 65 mA | 120 mA (Wi-Fi Tx burst) | 3.0V – 3.6V (3.3V Nom) |
| **1.8" ST7735 TFT + Backlight**| 45 mA – 60 mA | 70 mA | 3.3V or 5.0V |
| **0.96" SSD1306 Monochrome OLED**| 12 mA – 22 mA | 30 mA (All pixels lit) | 3.3V |
| **AS608 Fingerprint Sensor** | 30 mA (Idle) / 100 mA (LED Optical Scan) | 120 mA | 3.3V – 5.0V |
| **Active Buzzer** | 15 mA – 25 mA | 30 mA | 3.3V |

**Total peak power consumption:**
- **Desk Buggy (ESP32 + TFT + AS608 + Buzzer):** $\approx 350\text{ mA}$ at peak scan. A standard 500mA USB port provides ample margin.
- **ESP32-C3 SuperMini + 0.96" OLED:** $\approx 85\text{ mA}$ total. Runs cool and efficiently over USB or small LiPo cells.
