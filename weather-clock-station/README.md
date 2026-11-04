# ☀️ Smart Weather & Clock Station — ESP32-C3 + 0.96" OLED

A smart, connected desktop weather monitor and digital clock station designed for the **ESP32-C3 SuperMini** (160MHz RISC-V) and a 0.96" I2C Monochrome OLED (`SSD1306` 128x64).

Fetches live global weather data from **Open-Meteo** (no API key required), synchronizes real-time clock via **NTP**, and displays conditions with pixel-art weather icons and a multi-card dashboard.

---

## 🌟 Key Features

1. **Live Global Weather & Forecast**:
   - Real-time weather condition, temperature, humidity, wind speed, and min/max day ranges.
   - High-resolution (24x24) pixel art weather icons (Sunny, Clear Night Moon, Partly Cloudy, Overcast, Rain, Thunderstorm, Snow).
   - Powered by Open-Meteo REST API with worldwide location support.

2. **Accurate Digital Clock & Date**:
   - NTP network time synchronization (`pool.ntp.org`).
   - Displays Day of Week, Date, and 24-hour digital clock with blinking colon seconds.
   - Built-in standalone fallback RTC mode for immediate offline operation.

3. **Multi-Card Dashboard UI**:
   - **Card 1: Main Station Dashboard**: City location badge, Wi-Fi status, weather icon, large temperature reading (°C / °F), condition text, and real-time clock ribbon.
   - **Card 2: Atmosphere & Vitals**: Live humidity percentage with visual gauge bar, wind speed (km/h), silicon core temperature, and min/max temp forecast.
   - **Card 3: Celestial Sky Arc**: Real-time visual tracking of sun and moon across the sky dome based on current time of day.
   - Auto-cycles between views every 8 seconds (can be toggled or paused).

4. **Internal Core Temperature Sensor**:
   - Reads the ESP32-C3 on-chip silicon temperature sensor with low-pass filtering.

---

## 🔌 Hardware Wiring

| OLED Pin (SSD1306) | ESP32-C3 SuperMini Pin | Description |
|---|---|---|
| **SDA** | **GPIO 8** | I2C Data line |
| **SCL** | **GPIO 9** | I2C Clock line (Fast Mode @ 800kHz) |
| **VCC** | **3.3V** | Power (3.3V) |
| **GND** | **GND** | Ground |

---

## ⌨️ Serial Interactive Commands (115200 baud)

Connect over Serial Monitor to configure Wi-Fi and location on the fly:

| Command | Action | Example |
|---|---|---|
| `wifi <SSID> <PASS>` | Connect to local Wi-Fi & auto-sync | `wifi MyHomeNetwork secret123` |
| `city <NAME>` | Select city preset (Tokyo, London, NY, SF, BLR, Paris) | `city london` |
| `loc <LAT> <LON> <NAME>` | Set custom GPS coordinates & city name | `loc 12.97 77.59 Bengaluru` |
| `unit <C\|F>` | Toggle Celsius / Fahrenheit | `unit F` |
| `mode` | Manually step to the next dashboard card | `mode` |
| `auto` | Toggle 8-second auto-cycle ON / OFF | `auto` |

---

## 🚀 Building & Flashing

```bash
cd weather-clock-station

# Compile firmware
~/.venv/bin/python -m platformio run

# Flash to connected ESP32-C3
~/.venv/bin/python -m platformio run -t upload --upload-port /dev/cu.usbmodem1301

# Open Serial Monitor
~/.venv/bin/python -m platformio device monitor -b 115200
```
