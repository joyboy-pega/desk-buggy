# Minimal Biometric Attendance Terminal & Simulator

A high-performance embedded biometric attendance terminal designed for the **ESP32** and **1.8" ST7735 Color TFT Display (160×128)**, featuring clean high-contrast visuals, animated boot sequence, WiFi/Offline support, and instant touch verification.

---

## ⚡ Key Highlights

- **Multi-Stage Boot Sequence**:
  - `WELCOME TO` -> **ARIGATOO**
  - `DEVELOPED BY` -> **INFRANEX DEV**
  - Automatic WiFi detection: shows `WIFI CONNECTED` or seamlessly falls back to `NO WIFI • OFFLINE`.
- **Minimalist High-Contrast UI**:
  - Double-buffered 16-bit canvas (`GFXcanvas16`) for 100% flicker-free 50 FPS rendering.
  - Large digital clock with date, pulsing status indicator, and animated biometric reticle.
  - **Attendance Marked Screen**: Green accent frame, worker name, timestamp, and check-in/out toggle.
  - **Attendance Rejected Screen**: Red frame, `NOT MATCHED`, `TRY AGAIN`.
- **Instant Touch Verification**:
  - High-speed 50ms polling on the optical fingerprint sensor.
  - Placing any finger on the sensor immediately shows **`✓ MARKED`** with non-blocking audio feedback.
- **Interactive Serial Monitor CLI**:
  - Enroll new workers via Serial (`enroll <name>`).
  - Simulate matched punch (`1` or `m`).
  - Simulate error/rejected punch (`0` or `e`).
  - Toggle 180° rotation on the fly (`r`).
  - Toggle WiFi online/offline (`w`).
  - List all enrolled workers in flash (`list` or `l`).

---

## 🔌 Hardware Wiring

| ST7735 TFT (1.8" SPI) | ESP32 GPIO | Description |
|:---|:---|:---|
| **CS** | **GPIO 5** | Chip Select |
| **DC / A0** | **GPIO 2** | Data / Command |
| **RST** | **GPIO 4** | Reset |
| **SDA / MOSI** | **GPIO 23** | VSPI Data Out |
| **SCK / SCL** | **GPIO 18** | VSPI Clock |
| **LED / BL** | **GPIO 32** | Backlight Control |
| **VCC / GND** | **3.3V / GND** | Power (3.3V Only) |

| Fingerprint Sensor (Optional) | ESP32 GPIO | Description |
|:---|:---|:---|
| **Sensor TX** | **GPIO 16** | ESP32 RX2 |
| **Sensor RX** | **GPIO 17** | ESP32 TX2 |
| **VCC / GND** | **3.3V / 5V & GND** | Power & Ground |

| Other Peripherals | ESP32 GPIO | Description |
|:---|:---|:---|
| **Buzzer** | **GPIO 13** | Audio Feedback Beeps |
| **BOOT Button** | **GPIO 0** | Physical Simulator Button |

---

## 🕹️ Serial Monitor CLI Commands

Open Serial Monitor at **115200 baud**:

| Command | Action |
|:---|:---|
| `1` or `m` | **Simulate Matched Fingerprint**: punches attendance for enrolled worker. |
| `0` or `e` | **Simulate Error / Mismatch**: triggers red rejection card & error tone. |
| `a <name>` or `enroll <name>` | **Enroll New Worker**: interactive 2-step enrollment saved to NVS flash! |
| `w` | **Toggle WiFi**: switches between Online and `NO WIFI` offline mode. |
| `l` or `list` | **List Enrolled Workers**: prints database slots and check-in statuses. |
| `?` or `help` | **Help Menu**: prints command reference. |

---

## 🚀 How to Flash

```bash
# Using PlatformIO
.venv/bin/pio run --target upload
```
