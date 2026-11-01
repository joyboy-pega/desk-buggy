#!/usr/bin/env python3
import subprocess
import sys
import time
import glob

def find_esp_ports():
    ports = glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.SLAB_USBtoUART*") + glob.glob("/dev/cu.wchusbserial*")
    return ports

print("=" * 60)
print("ESP32 AUTO FLASH & DIAGNOSTIC MONITOR")
print("=" * 60)
print("Waiting for ESP32 board to power ON and send data...")

last_msg = ""
while True:
    ports = find_esp_ports()
    if not ports:
        msg = "Waiting for ESP32 USB cable to be connected..."
        if msg != last_msg:
            print(msg)
            last_msg = msg
        time.sleep(1)
        continue

    port = ports[0]
    
    # Try esptool chip_id
    cmd = [
        sys.executable, "-m", "esptool",
        "--port", port,
        "--baud", "115200",
        "--connect-attempts", "1",
        "chip_id"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode == 0:
        print(f"\n[+] SUCCESS: ESP32 detected on {port}!")
        print(res.stdout.strip())
        upload_res = subprocess.run([".venv/bin/pio", "run", "-t", "upload", "--upload-port", port])
        if upload_res.returncode == 0:
            print("\n" + "=" * 60)
            print("[+] FLASHING COMPLETE! Your LED blink program is now running!")
            print("=" * 60)
            sys.exit(0)
        else:
            print("\n[-] Upload failed. Retrying...")
    else:
        print(f"Waiting for ESP32 power & bootloader on {port}... (Tip: Check power LED, hold BOOT if needed)", end="\r", flush=True)
    time.sleep(1)
