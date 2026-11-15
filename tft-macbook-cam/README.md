# MacBook FaceTime Camera Streamer -> ESP32 1.8" TFT Display (160x128)

Stream live video from your MacBook's built-in FaceTime HD camera directly to your ESP32-WROOM-32 with a 1.8" ST7735 color TFT display in real time!

---

## ⚡ Key Highlights

- **Dual Mode Transmission**:
  - **Wi-Fi UDP Stream (25–30 FPS)**: Ultra-fast, low latency, completely wireless.
  - **USB Serial Stream (921,600 baud)**: Plug-and-play over USB cable without Wi-Fi.
- **Aspect Ratio Corrected**: Automatically center-crops the 16:9 MacBook webcam into 160×128 so your face isn't stretched or distorted.
- **Natural Mirroring**: Mirrors the camera view naturally (toggle anytime with `M`).
- **Live Mac Preview Window**: Shows real-time stream status, FPS, and hotkey controls.
- **Built-in Creative Filters**: Normal, Cyberpunk Cyan, Matrix Green, and Retro B&W (cycle with `C`).
- **Standby HUD**: Displays connection status, Wi-Fi IP address, and listening port.

---

## 🔌 Hardware Pinout (ESP32-WROOM-32 to 1.8" ST7735)

| TFT Pin | ESP32 GPIO | Description |
|---|---|---|
| **CS** | GPIO 5 | Chip Select |
| **DC / A0** | GPIO 2 | Data / Command |
| **RST** | GPIO 4 | Hardware Reset |
| **SDA / MOSI** | GPIO 23 | Hardware VSPI MOSI |
| **SCK / SCLK** | GPIO 18 | Hardware VSPI SCK |
| **LED / BL** | GPIO 32 | Backlight PWM (Brightness Control) |
| **VCC** | 3.3V or 5V | Power Supply |
| **GND** | GND | Ground |

---

## 🚀 Quick Start Guide

### Step 1: Install Python Dependencies on MacBook

Open Terminal and run:
```bash
pip install opencv-python numpy pyserial
```

### Step 2: Flash the ESP32 Firmware

From the `tft-macbook-cam` directory:
```bash
pio run -t upload
```
*(Or from the project root: `pio run -e esp32-tft-macbook-cam -t upload`)*

When the ESP32 boots, the screen will show:
- Wi-Fi connection status and your ESP32 IP address (e.g. `192.168.1.45:8888`).

---

### Step 3: Start Streaming from MacBook

#### Method A: Wireless Wi-Fi (Recommended - 25-30 FPS)
```bash
python3 mac_cam_streamer.py --ip <ESP32_IP_ADDRESS>
```
*Example:*
```bash
python3 mac_cam_streamer.py --ip 192.168.1.45
```

#### Method B: USB Serial (Over USB Cable)
```bash
python3 mac_cam_streamer.py --mode serial
```

---

## 🎮 Live Preview & Controls (on MacBook)

While streaming, a preview window opens on your Mac. You can use these hotkeys:
- **`M`**: Toggle horizontal camera mirror (selfie mode on/off)
- **`C`**: Cycle color filters:
  - `0`: Normal Color
  - `1`: Cyberpunk Cyan
  - `2`: Matrix Green
  - `3`: Monochrome Retro B&W
- **`Q`**: Quit stream cleanly

---

## 🛠️ ESP32 Serial Monitor Hotkeys (921600 baud)
- `+` / `-`: Increase / Decrease display brightness
- `O`: Toggle On-Screen FPS badge
- `R`: Rotate screen 180°

