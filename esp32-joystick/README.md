# ESP32 Dual-Axis Joystick Controller & Telemetry Studio

Live hardware telemetry studio and Web Serial visualizer for analog thumb joystick (KY-023 / PS2 module) connected to an ESP32-WROOM-32.

---

## 🔌 Hardware Wiring

| Joystick Pin | ESP32 Pin | Signal Type | Function & Notes |
|:---|:---|:---|:---|
| **VRx (X-Axis)** | **GPIO 34** | Analog (ADC1_CH6) | 12-bit ADC (0–4095), center ~2048. Wi-Fi safe. |
| **VRy (Y-Axis)** | **GPIO 35** | Analog (ADC1_CH7) | 12-bit ADC (0–4095), center ~2048. Wi-Fi safe. |
| **SW (Pushbutton)** | **GPIO 25** | Digital (`INPUT_PULLUP`) | Active LOW: Pressed = 0, Released = 1. |
| **+5V / VCC** | **3.3V** | Power | Connect to 3.3V to protect ESP32 ADC limits. |
| **GND** | **GND** | Ground | Common system ground. |

---

## 🚀 How to Run

### 1. Flash Firmware to ESP32

Using the virtual environment PlatformIO:
```bash
.venv/bin/pio run -d esp32-joystick --target upload
```
Or use the **Web Flasher** in `web/` to flash directly from Google Chrome / Microsoft Edge over USB!

---

### 2. Open the Live Web Visualizer

Open [joystick.html](file:///Users/joy/Desktop/project/tea%20party/desk%20buggy/joystick.html) directly in **Google Chrome**, **Microsoft Edge**, **Brave**, or **Opera**:

- **Method A (USB Cable - Instant <5ms latency)**:
  1. Plug ESP32 into Mac/PC with USB cable.
  2. Click **"🔌 Connect USB (Web Serial)"**.
  3. Select your ESP32's Serial Port (e.g., `/dev/cu.usbserial...` or `COMx`).
  4. Move your physical joystick and press the SW button!

- **Method B (Wi-Fi Access Point)**:
  1. The ESP32 broadcasts a Wi-Fi network:
     - **SSID**: `ESP32-Joystick`
     - **Password**: `12345678`
  2. Connect your phone or laptop to `ESP32-Joystick`.
  3. Open `http://192.168.4.1/` in your browser. The ESP32 serves the live visualizer directly from its internal flash!

- **Method C (Simulator)**:
  Click **"🎮 Simulator: ON"** in `joystick.html` to test with mouse or touch without any hardware connected.

---

## 🌟 Visualizer Features

1. **3D/2D Joystick Gimbal**:
   - Realistic circular bezel with 25%, 50%, 75%, and 100% boundary rings.
   - 3D lighting, tilt angle dynamics, specular highlights, and shadow displacement.
   - 8-point cardinal compass (N, NE, E, SE, S, SW, W, NW).
2. **SW Push Button Indicator**:
   - Mechanical glowing button with audio synthesis click sound (Web Audio API).
   - Real-time click counter and hold duration timer.
3. **Real-Time Dual-Channel Oscilloscope**:
   - 60 FPS scrolling waveforms of VRx (cyan) and VRy (orange) signals.
4. **Interactive Desk Buggy RC Playground**:
   - Steer, accelerate, reverse, and drift a virtual Desk Buggy using your physical joystick!
   - Press the SW button for Nitro Turbo Boost!
5. **Gauges & Telemetry**:
   - Raw ADC (0 - 4095), Voltages (0.00V - 3.30V), Normalized (-1.00 to +1.00), Angle (0° - 360°), and Packet Rate (Hz).
6. **Calibration**:
   - Invert X / Invert Y / Swap Axes toggles.
   - One-click auto zero center calibration.
