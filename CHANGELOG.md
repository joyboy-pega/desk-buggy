# Changelog

All notable changes to the Desk Buggy project will be documented in this file.

## [1.2.0] - 2026-11-27

### Added
- **Biometric Attendance Terminal & Simulator (`tft-nothing-attendance/`)**:
  - Minimalist high-contrast biometric terminal for 1.8" ST7735 Color TFT and ESP32.
  - Multi-stage boot sequence: Welcome to Arigatoo, Developed by Infranex Dev, and WiFi provisioning / offline mode.
  - Large-format digital clock with live ticking seconds and live network status.
  - Instant touch verification: placing any finger on the optical sensor (or pressing BOOT button) immediately displays `✓ MARKED`.
  - Non-blocking asynchronous buzzer engine preventing screen freezes during audio beeps.
  - Dynamic 180-degree screen rotation toggle (`r` command) persisted to NVS flash.
  - Flash-backed employee database (Joyboy, Alice, Pega) with runtime enrollment support.
  - Full serial monitor CLI suite (`1`, `0`, `a`, `w`, `r`, `l`, `?`).

## [1.1.0] - 2026-11-17

### Added
- **ESP32 Dual-Axis Joystick Controller & BLE Mouse**:
  - Native Bluetooth LE wireless mouse mode for macOS, Windows, and Linux (`MacBook Joystick Mouse`).
  - Smooth non-linear acceleration curve and 12-bit ADC oversampling for jitter-free cursor tracking.
  - Smart pushbutton handling: Single click (left), Double tap (right), and Hold-to-drag.
  - Active LOW SW button with internal pull-up on GPIO 25.
  - Wi-Fi and Bluetooth-safe analog inputs on ADC1 (GPIO 34 and GPIO 35).
- **Cyberpunk Joystick Telemetry Studio (`joystick.html`)**:
  - 3D/2D joystick gimbal visualizer with real-time vector compass.
  - Dual-channel 60 FPS oscilloscope for VRx and VRy signals.
  - Interactive virtual Desk Buggy RC playground with turbo boost.
  - Direct Web Serial connection and standalone Wi-Fi AP server.
- **Hardware Studio Web Updates**:
  - Registered `esp32-joystick` preset in `firmwarePresets.ts`.
  - Added dedicated Joystick wiring matrix to `HardwarePinouts.tsx`.
  - Added real-time radar HUD simulator to `HardwareScreenSimulator.tsx`.
  - Header shortcut to Live Joystick Studio in `page.tsx`.
- **Documentation & Tools**:
  - Comprehensive electrical and pinout guide in `docs/JOYSTICK.md`.
  - CLI terminal visualizer script in `tools/joystick_serial_monitor.py`.
