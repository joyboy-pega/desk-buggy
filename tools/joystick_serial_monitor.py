#!/usr/bin/env python3
"""
tools/joystick_serial_monitor.py
Real-time terminal visualizer for ESP32 Dual-Axis Joystick Telemetry.
Reads 50Hz serial packets and renders ASCII bar gauges in terminal.
"""

import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial required: pip install pyserial")
    sys.exit(1)


def find_esp32_port():
    ports = list_ports.comports()
    for p in ports:
        if "usbserial" in p.device.lower() or "slab" in p.device.lower() or "wch" in p.device.lower():
            return p.device
    return None


def render_bar(val, min_val=0, max_val=4095, width=20):
    norm = max(0.0, min(1.0, (val - min_val) / float(max_val - min_val)))
    filled = int(round(norm * width))
    return f"[{'#' * filled}{'.' * (width - filled)}]"


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_esp32_port()
    if not port:
        print("No ESP32 serial port detected. Specify port as argument:")
        print("  python3 tools/joystick_serial_monitor.py /dev/cu.usbserial-xxx")
        sys.exit(1)

    print(f"Connecting to ESP32 on {port} (115200 baud)...")
    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(1.0)
    print("Connected! Reading telemetry (Press Ctrl+C to stop)...")

    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line:
                print(f"\r{line}", end="", flush=True)
    except KeyboardInterrupt:
        print("\nExiting.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
