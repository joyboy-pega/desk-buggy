# ESP32 Dual-Axis Joystick & BLE Wireless Mouse Documentation

## 1. Overview
The **Desk Buggy Joystick System** provides two distinct operating modes for analog thumb joysticks (KY-023 / PS2 / R103 / B103 modules) connected to an **ESP32-WROOM-32**:

1. **Bluetooth LE Wireless Mouse Mode**:
   - Native HID mouse pairing with macOS, Windows, Linux, Android, and iOS.
   - Non-linear acceleration curve for 1-pixel precision control.
   - Single click (left), Double click (right), and Hold-to-drag.

2. **Live Telemetry Visualizer Mode**:
   - 50 Hz serial telemetry stream.
   - Web Serial interface with oscilloscope, 3D gimbal, and RC playground.
   - Built-in standalone Wi-Fi AP server.

---

## 2. Electrical Pinout & Wiring

| Joystick Pin | ESP32 GPIO | Signal Type | Electrical Notes |
|:---|:---|:---|:---|
| **VRx (X-Axis)** | **GPIO 34** | Analog (ADC1_CH6) | 12-bit ADC (0–4095). ADC1 is safe to use while Wi-Fi/BT is active. |
| **VRy (Y-Axis)** | **GPIO 35** | Analog (ADC1_CH7) | 12-bit ADC (0–4095). ADC1 is safe to use while Wi-Fi/BT is active. |
| **SW (Pushbutton)** | **GPIO 25** | Digital Input | Internal pullup enabled. Active LOW. |
| **+5V / VCC** | **3.3V** | Power | Connect to 3.3V rail. Do not connect to 5V to prevent exceeding ADC voltage rating. |
| **GND** | **GND** | Ground | Common system ground. |

---

## 3. Bluetooth Mouse macOS Quick Start

1. Compile and flash using PlatformIO:
   ```bash
   cd esp32-joystick
   ~/.platformio/penv/bin/pio run --target upload
   ```
2. Open **System Settings → Bluetooth** on your Mac.
3. Select **`MacBook Joystick Mouse`** and click **Connect**.
4. The ESP32's built-in blue LED (GPIO 2) illuminates solid blue upon successful connection.
