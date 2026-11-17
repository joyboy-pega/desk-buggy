# Changelog

All notable changes to the Desk Buggy project will be documented in this file.

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
