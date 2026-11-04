# ⏰ Macintosh Clock & Temperature Monitor (ESP32-C3 SuperMini + 0.96" OLED)

A vintage 1984 Macintosh-styled real-time desk clock and hardware temperature monitor built for the **ESP32-C3 SuperMini** and **0.96" I2C OLED (SSD1306, 128x64)**.

---

## ⚡ Hardware Wiring

| 0.96" OLED Pin | ESP32-C3 SuperMini Pin | Description |
|:---:|:---:|:---|
| **VCC** | **3V3** | 3.3V Power |
| **GND** | **GND** | Ground |
| **SDA** | **GPIO 8** | I2C Data |
| **SCL** | **GPIO 9** | I2C Clock |

---

## 🖥️ Screen Viewing Modes (Auto-Cycles every 10 seconds)

1. **Classic Macintosh System Window (`Mode 1`):**
   - Retro window with Apple logo (``) and "System Status" title.
   - Large digital clock: `22:35:00`
   - Date banner: `WED, NOV 04, 2026`
   - Real-time temperature readouts in both Celsius & Fahrenheit.

2. **Giant Retro Digital Clock (`Mode 2`):**
   - Large double-height digital numbers with flashing seconds and AM/PM tag.
   - Thermometer icon with `38.4 °C (101 °F)`.
   - CPU clock frequency (`160 MHz`) and available free RAM heap.

3. **Live Temperature History Graph (`Mode 3`):**
   - Real-time line chart showing temperature changes over the last 2 minutes!
   - Auto-scaling temperature bounds with top status clock.

---

## ⌨️ Interactive Serial Commands (115200 Baud)

Send commands from your terminal to interact with the clock:
- `time 22:45:00` $\rightarrow$ Set the current time
- `date 2026-11-04` $\rightarrow$ Set the current date
- `wifi "MyHomeWiFi" "password"` $\rightarrow$ Connect to Wi-Fi and sync time automatically via NTP!
- `mode` $\rightarrow$ Manually cycle to the next screen mode
- `temp` $\rightarrow$ Print live temperature diagnostics to the terminal

---

## 🚀 How to Build & Flash

```bash
cd "/Users/joy/Desktop/project/tea party/desk buggy/macintosh-clock-temp"

# Build firmware
"/Users/joy/Desktop/project/tea party/desk buggy/.venv/bin/python" -m platformio run

# Flash to ESP32-C3
"/Users/joy/Desktop/project/tea party/desk buggy/.venv/bin/python" -m platformio run -t upload

# Open Serial Monitor
python3 -m serial.tools.miniterm /dev/cu.usbmodem* 115200
```
