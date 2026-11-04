# 🖥️ ESP32-C3 SuperMini Macintosh Desk Buddy (0.96" OLED)

A retro 1984 Macintosh-themed interactive desk companion built for the **ESP32-C3 SuperMini** and **0.96" I2C OLED (SSD1306, 128x64)**.

---

## ⚡ Hardware Wiring

| 0.96" OLED Pin | ESP32-C3 SuperMini Pin | Description |
|:---:|:---:|:---|
| **VCC** | **3V3** | 3.3V Power |
| **GND** | **GND** | Ground |
| **SDA** | **GPIO 8** | I2C Data *(Default C3 SDA)* |
| **SCL** | **GPIO 9** | I2C Clock *(Default C3 SCL)* |

---

## 🎭 Animated Macintosh Personalities

1. **`hello.` 1984 Intro:** Iconic cursive Macintosh greeting with miniature Mac chassis.
2. **Happy Mac Face:** The classic CRT bezel with smooth blinking eyes and a warm smile.
3. **Winking Mac:** Playful wink with a cheek smirk and animated sparkle.
4. **Thinking Mac:** Eyes look up in deep thought while the retro Macintosh wristwatch rotates.
5. **System 1.0 Retro Desktop:** Complete with top Apple menu bar (``), floppy drive, and a gliding arrow cursor.
6. **Surprised Mac:** Wide open eyes and round mouth (`:O`).
7. **Sleepy Mac:** Relaxed eye arcs with floating animated `Zzz...`

---

## ⌨️ Serial Interactive Mood Triggers (115200 Baud)

You can trigger any mood manually by typing into the serial monitor:
- `happy` $\rightarrow$ Switch to Happy Mac
- `wink` $\rightarrow$ Trigger Wink with sparkle
- `think` $\rightarrow$ Enter Thinking mode with rotating wristwatch
- `desktop` $\rightarrow$ Show System 1.0 classic desktop
- `sleep` $\rightarrow$ Put Desk Buddy to sleep (`Zzz...`)
- `surprise` $\rightarrow$ Surprised expression
- `hello` $\rightarrow$ Re-run the classic 1984 "hello." intro

---

## 🚀 How to Build & Flash

```bash
cd "/Users/joy/Desktop/project/tea party/macintosh-desk-buddy"

# Build firmware
"/Users/joy/Desktop/project/tea party/desk buggy/.venv/bin/python" -m platformio run

# Flash to ESP32-C3
"/Users/joy/Desktop/project/tea party/desk buggy/.venv/bin/python" -m platformio run -t upload

# Open Serial Monitor
python3 -m serial.tools.miniterm /dev/cu.usbmodem* 115200
```

