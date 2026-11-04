import subprocess
import time
import sys

import glob

ports = glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.SLAB_USBtoUART*") + glob.glob("/dev/cu.wchusbserial*")
port = ports[0] if ports else "/dev/cu.usbserial-0001"
esptool_bin = ".venv/bin/python"

print("=" * 60)
print("READY TO FLASH ESP32 BLINK FIRMWARE")
print("=" * 60)
print("Instructions:")
print("1. Press and HOLD the 'BOOT' (or 'IO0') button on the board.")
print("2. While holding BOOT, press and release the 'EN' (or 'RST') button once.")
print("3. Keep holding BOOT for 1 second, then release it.")
print("=" * 60)
print("Waiting for ESP32 bootloader to respond on", port, "...")

found = False
for attempt in range(1, 30):
    cmd = [
        esptool_bin, "-m", "esptool",
        "--chip", "esp32",
        "--port", port,
        "--baud", "115200",
        "--connect-attempts", "1",
        "chip_id"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode == 0:
        print(f"\n[+] ESP32 detected on attempt {attempt}!")
        found = True
        break
    else:
        print(f"Attempt {attempt}/30: waiting for board into download mode (Hold BOOT & tap EN)...", end="\r", flush=True)
        time.sleep(1)

if not found:
    print("\n[-] Timeout: Could not connect to ESP32. Please check cable or hold BOOT button.")
    sys.exit(1)

print("\nStarting upload with PlatformIO...")
upload_res = subprocess.run([".venv/bin/pio", "run", "-t", "upload", "--upload-port", port])
sys.exit(upload_res.returncode)
