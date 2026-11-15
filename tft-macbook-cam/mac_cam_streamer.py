#!/usr/bin/env python3
"""
===============================================================================
 MacBook FaceTime HD Camera Streamer to ESP32 + 1.8" ST7735 TFT
 Streams live camera feed (160x128 16-bit RGB565) over Wi-Fi UDP or USB Serial
===============================================================================
"""

import sys
import time
import socket
import argparse
import glob

try:
    import cv2
    import numpy as np
except ImportError:
    print("[ERROR] OpenCV / NumPy not found.")
    print("Please install them using:")
    print("    pip install opencv-python numpy pyserial")
    sys.exit(1)

try:
    import serial
except ImportError:
    serial = None

SCREEN_W = 160
SCREEN_H = 128
CHUNK_ROWS = 4
CHUNK_BYTES = SCREEN_W * CHUNK_ROWS * 2  # 1280 bytes
TOTAL_CHUNKS = SCREEN_H // CHUNK_ROWS    # 32 chunks
UDP_HEADER_SIZE = 6

def bgr_to_rgb565(frame_bgr):
    """
    Vectorized conversion from BGR uint8 (H, W, 3) to RGB565 big-endian bytes
    for direct ST7735 SPI display rendering.
    """
    # Extract channels
    b = frame_bgr[:, :, 0].astype(np.uint16) >> 3  # 5 bits
    g = frame_bgr[:, :, 1].astype(np.uint16) >> 2  # 6 bits
    r = frame_bgr[:, :, 2].astype(np.uint16) >> 3  # 5 bits
    
    # Pack into 16-bit RGB565
    rgb565 = (r << 11) | (g << 5) | b
    
    # ST7735 expects big-endian (MSB first)
    return rgb565.byteswap().tobytes()

def crop_and_resize(frame, target_w=160, target_h=128):
    """
    Center-crops to match target aspect ratio and resizes to 160x128.
    Preserves natural facial proportions without squishing.
    """
    h, w = frame.shape[:2]
    target_aspect = target_w / target_h
    current_aspect = w / h

    if current_aspect > target_aspect:
        # Crop width
        new_w = int(h * target_aspect)
        start_x = (w - new_w) // 2
        cropped = frame[:, start_x:start_x + new_w]
    else:
        # Crop height
        new_h = int(w / target_aspect)
        start_y = (h - new_h) // 2
        cropped = frame[start_y:start_y + new_h, :]

    return cv2.resize(cropped, (target_w, target_h), interpolation=cv2.INTER_AREA)

def find_serial_ports():
    """Detect common ESP32 USB serial ports on macOS."""
    patterns = [
        "/dev/cu.usbserial-*",
        "/dev/cu.wchusbserial*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/cu.usbmodem*"
    ]
    ports = []
    for pat in patterns:
        ports.extend(glob.glob(pat))
    return ports

def main():
    parser = argparse.ArgumentParser(description="Stream MacBook camera to ESP32 TFT Display")
    parser.add_argument("--mode", choices=["wifi", "serial"], default="wifi",
                        help="Streaming transport mode: 'wifi' (default, 25-30 FPS) or 'serial'")
    parser.add_argument("--ip", type=str, default="",
                        help="ESP32 IP address for Wi-Fi streaming (e.g. 192.168.1.45)")
    parser.add_argument("--port", type=int, default=8888,
                        help="UDP port on ESP32 (default: 8888)")
    parser.add_argument("--serial-port", type=str, default="",
                        help="Serial port path (e.g. /dev/cu.usbserial-0001)")
    parser.add_argument("--baud", type=int, default=921600,
                        help="Serial baud rate (default: 921600)")
    parser.add_argument("--cam", type=int, default=0,
                        help="Camera device index (default: 0 for MacBook FaceTime HD)")
    parser.add_argument("--no-mirror", action="store_true",
                        help="Disable horizontal camera mirroring")
    args = parser.parse_args()

    print("==========================================================")
    print("   MacBook Live Camera Streamer -> ESP32 ST7735 TFT       ")
    print("==========================================================")

    # Initialize Camera
    print(f"[CAM] Opening camera device index {args.cam}...")
    cap = cv2.VideoCapture(args.cam)
    if not cap.isOpened():
        print(f"[ERROR] Could not open camera {args.cam}. Check Privacy & Security > Camera permissions in macOS.")
        return

    # Set camera resolution (MacBook FaceTime HD native capture)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    # Setup Transport
    sock = None
    ser = None

    if args.mode == "wifi":
        target_ip = args.ip
        if not target_ip:
            target_ip = input("Enter ESP32 IP address shown on TFT screen: ").strip()
            if not target_ip:
                print("[ERROR] ESP32 IP is required for Wi-Fi streaming.")
                return

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print(f"[WIFI] Streaming UDP video to {target_ip}:{args.port}")
    else:
        if serial is None:
            print("[ERROR] pyserial is not installed. Run: pip install pyserial")
            return

        port = args.serial_port
        if not port:
            available = find_serial_ports()
            if available:
                port = available[0]
                print(f"[SERIAL] Auto-detected ESP32 at: {port}")
            else:
                port = input("Enter Serial port (e.g. /dev/cu.usbserial-0001): ").strip()

        try:
            ser = serial.Serial(port, args.baud, timeout=0.1)
            print(f"[SERIAL] Connected to {port} at {args.baud} baud")
            time.sleep(1.0) # Allow ESP32 reset settling
        except Exception as e:
            print(f"[ERROR] Failed to connect to serial port: {e}")
            return

    mirror = not args.no_mirror
    filter_mode = 0  # 0=Normal, 1=Cyberpunk Cyan, 2=Matrix Green, 3=B&W
    filter_names = ["Normal", "Cyber Cyan", "Matrix Green", "Monochrome B&W"]

    frame_id = 0
    fps_counter = 0
    fps_start = time.time()
    current_fps = 0.0

    print("\n[STREAMING ACTIVE] Controls in preview window:")
    print("  'q' - Quit stream")
    print("  'm' - Toggle Mirror / Flip horizontal")
    print("  'c' - Cycle Color Filter (Normal, Cyan, Matrix, B&W)")
    print("----------------------------------------------------------\n")

    try:
        while True:
            ret, raw_frame = cap.read()
            if not ret:
                print("[WARN] Failed to read frame from camera.")
                time.sleep(0.02)
                continue

            # Mirror for natural selfie webcam view
            if mirror:
                raw_frame = cv2.flip(raw_frame, 1)

            # Center-crop & resize to 160x128
            frame_160x128 = crop_and_resize(raw_frame, SCREEN_W, SCREEN_H)

            # Optional Color Filters
            processed = frame_160x128.copy()
            if filter_mode == 1: # Cyberpunk Cyan
                processed[:, :, 2] = (processed[:, :, 2] * 0.4).astype(np.uint8) # Suppress Red
                processed[:, :, 0] = np.clip(processed[:, :, 0] * 1.3, 0, 255).astype(np.uint8)
            elif filter_mode == 2: # Matrix Green
                processed[:, :, 0] = 0 # No Blue
                processed[:, :, 2] = 0 # No Red
                processed[:, :, 1] = np.clip(processed[:, :, 1] * 1.4, 0, 255).astype(np.uint8)
            elif filter_mode == 3: # Monochrome
                gray = cv2.cvtColor(processed, cv2.COLOR_BGR2GRAY)
                processed = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)

            # Convert to ST7735 16-bit RGB565 bytes
            raw_rgb565 = bgr_to_rgb565(processed)

            # Send over Wi-Fi UDP
            if args.mode == "wifi" and sock:
                frame_id = (frame_id + 1) % 256
                # Split into 32 chunks of 1280 bytes
                for chunk_idx in range(TOTAL_CHUNKS):
                    offset = chunk_idx * CHUNK_BYTES
                    chunk_data = raw_rgb565[offset:offset + CHUNK_BYTES]
                    # Packet Header: [0x56, 0x49, 0x44] + frame_id + chunk_idx + total_chunks
                    header = bytes([0x56, 0x49, 0x44, frame_id, chunk_idx, TOTAL_CHUNKS])
                    sock.sendto(header + chunk_data, (target_ip, args.port))
            # Send over USB Serial
            elif ser:
                # Sync Header: 0xAA 0x55 0xAA 0x55 + 40960 bytes
                sync_header = bytes([0xAA, 0x55, 0xAA, 0x55])
                ser.write(sync_header + raw_rgb565)

            # FPS calculation
            fps_counter += 1
            if time.time() - fps_start >= 1.0:
                current_fps = fps_counter / (time.time() - fps_start)
                fps_counter = 0
                fps_start = time.time()

            # Preview Window on MacBook (Scaled up 3x for easy viewing)
            preview = cv2.resize(processed, (480, 384), interpolation=cv2.INTER_NEAREST)
            cv2.putText(preview, f"TFT Stream: {current_fps:.1f} FPS", (10, 26),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.putText(preview, f"Filter: {filter_names[filter_mode]} | Mirror: {mirror}", (10, 52),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
            cv2.putText(preview, "[M] Mirror  [C] Filter  [Q] Quit", (10, 365),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)

            cv2.imshow("MacBook Camera -> ESP32 TFT Streamer", preview)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            elif key == ord('m'):
                mirror = not mirror
            elif key == ord('c'):
                filter_mode = (filter_mode + 1) % len(filter_names)

    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        cv2.destroyAllWindows()
        if sock:
            sock.close()
        if ser:
            ser.close()
        print("\n[STREAM STOPPED] Camera stream closed cleanly.")

if __name__ == "__main__":
    main()

