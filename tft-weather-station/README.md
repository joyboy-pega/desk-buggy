# 🌈 Full-Color 1.8" TFT Weather & Clock Station (ESP32-C3 SuperMini)

A full-color smart desktop weather & clock station built for the **1.8" ST7735 SPI Color TFT Display (128x160 RGB565)** and powered by the **ESP32-C3 SuperMini** (160MHz RISC-V).

Automatically powers down / shuts off the 0.96" OLED display on the I2C bus upon boot.

---

## 🌟 Key Highlights

1. **Vivid 16-Bit RGB565 Full-Color Interface (128x160)**:
   - **Header Bar**: Crimson location pin `📍 PUNE, IN` + Green live Wi-Fi status indicator (`● ONLINE`).
   - **Weather Card**: Procedural weather illustrations (Golden radiant Sun with 8 flares, Silver/Cyan crescent moon with sparkling stars, shaded clouds, and cyan raindrops).
   - **Giant Temperature**: Big `24°C` readout in bright warm amber and gold.
   - **Atmosphere Vitals**: Dual glassmorphism cards for Humidity (`65%` with blue progress bar) and Wind Speed (`4.8 km/h`).
   - **Digital Clock & Calendar Hub**: Big glowing digital clock (`HH:MM:SS`) with pulsing seconds colons and full calendar date string (`WED, 04 NOV 2026`).

2. **Automated OLED Power Down**:
   - Sends `SSD1306_DISPLAYOFF` across I2C (`GPIO 8 SDA`, `GPIO 9 SCL`), putting the 0.96" OLED panel into 0mA deep power-off sleep.

3. **Autonomous Background Connectivity**:
   - Auto-connects to local Wi-Fi (`Arigatoo1`).
   - Decoupled FreeRTOS background task continuously keeps Pune weather synchronized via Open-Meteo REST API without stalling the UI.
   - Atomic clock accuracy via NTP (`pool.ntp.org`) in Indian Standard Time (IST, UTC+5:30).

---

## 🔌 Hardware Wiring (1.8" ST7735 TFT ⟷ ESP32-C3 SuperMini)

| 1.8" ST7735 TFT Pin | ESP32-C3 SuperMini Pin | Function |
|---|---|---|
| **CS** | **GPIO 7** | SPI Chip Select |
| **A0 / DC** | **GPIO 2** | Data / Command Select |
| **RESET / RST** | **GPIO 1** (or GPIO 3) | Hardware Reset |
| **SDA / MOSI** | **GPIO 6** | SPI Master Out Data |
| **SCK / SCLK** | **GPIO 4** | SPI Clock |
| **VCC** | **3.3V** or **5V** | Power Rail |
| **GND** | **GND** | Ground |
| **LED / BLK** | **3.3V** | Backlight Power |

*(Note: If you have an OLED connected to `GPIO 8` and `GPIO 9`, it will automatically be turned off at boot).*

---

## 🚀 Building & Flashing

```bash
cd tft-weather-station

# Compile firmware
~/.venv/bin/python -m platformio run

# Flash to connected ESP32-C3
~/.venv/bin/python -m platformio run -t upload --upload-port /dev/cu.usbmodem1301

# Open Serial Monitor
~/.venv/bin/python -m platformio device monitor -b 115200
```

