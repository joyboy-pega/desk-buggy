# DOOM 60 FPS — ESP32-C3 SuperMini + 0.96" OLED 🎮🔥

A real-time **3D DOOM Raycaster engine & PSX DOOM Fire simulator** running at a locked **60 FPS** on the **ESP32-C3 SuperMini** (160MHz RISC-V) and a 0.96" I2C Monochrome OLED (`SSD1306` 128x64).

---

## ⚡ Key Features

1. **True 60 FPS Performance**:
   - 1 MHz Fast Mode Plus I2C bus clock (`Wire.setClock(1000000)`).
   - High-performance DDA 3D raycasting casting 128 rays (full pixel-column resolution) with depth dithering and directional wall lighting.
   - Live hardware FPS counter displayed directly on the DOOM status bar (`60F`).

2. **3D DOOM Raycaster Engine (Mode 1)**:
   - 16x16 dungeon maze with corridors, rooms, pillars, and secret corners.
   - Textured depth shading with checkerboard dithering and side-lighting.
   - **Double-barrel Shotgun**: Real-time walking bob, explosive muzzle flash, recoil kickback, smoke puffs, and pump reload animations.
   - **3D Sprites**: Imps and explosive toxic hazard barrels projected with distance sorting and depth clipping against the 128-column Z-buffer.
   - **Autonomous Marine Bot AI**: The DOOM Marine navigates the dungeon autonomously, detects walls, seeks out demons, locks onto targets, and blasts them!
   - **Top-Down Radar Mini-Map**: Press `m` in serial monitor to toggle the live 2D mini-map overlay with player position and enemy blips.

3. **Authentic DOOM Status Bar HUD**:
   - `AMMO`: Live shotgun shell counter (auto-reloads when depleted).
   - `HEALTH`: 100% status readout.
   - **Animated Doomguy Face (16x12 pixel art)**:
     - Center look, glance left, glance right idle physics.
     - Evil grin (`>:D`) when firing or eliminating demons!
     - Wounded/ouch grimace when taking damage.
   - `ARMOR`: 100% defense gauge.
   - `FPS`: Live 60 FPS performance indicator.

4. **Procedural PSX DOOM Fire (Mode 2)**:
   - Authentic rising flame simulation algorithm with random cooling decay and horizontal wind drift.
   - 8-level dither heat mapping.
   - Classic brutalist **DOOM** logo floating over the blazing inferno.

5. **Doomguy Combat Showcase (Mode 3)**:
   - 2x high-resolution animated Doomguy facial expressions gallery and combat vitals.

---

## 🔌 Hardware Wiring

| OLED Pin (SSD1306) | ESP32-C3 SuperMini Pin | Description |
|---|---|---|
| **SDA** | **GPIO 8** | I2C Data line |
| **SCL** | **GPIO 9** | I2C Clock line |
| **VCC** | **3.3V** | Power (3.3V) |
| **GND** | **GND** | Ground |

---

## ⌨️ Serial Interactive Controls (115200 baud)

Open the Serial Monitor to interact with your DOOM Marine:

| Key / Command | Action |
|---|---|
| `w` | Step forward |
| `s` | Step backward |
| `a` | Turn left |
| `d` | Turn right |
| `SPACE` or `f` | **Fire Double-Barrel Shotgun!** |
| `m` | Toggle 2D Radar Mini-Map overlay |
| `auto` | Toggle Autonomous Bot AI (On / Off) |
| `1` | Switch to **3D DOOM Raycaster** mode |
| `2` | Switch to **PSX DOOM Fire Effect** mode |
| `3` | Switch to **Doomguy Showcase** mode |
| `mode` | Cycle through all display modes |

---

## 🚀 Building & Flashing

```bash
cd doom-oled

# Compile firmware
~/.platformio/penv/bin/pio run

# Flash to connected ESP32-C3
~/.platformio/penv/bin/pio run -t upload --upload-port /dev/cu.usbmodem1301

# Open Serial Monitor
~/.platformio/penv/bin/pio device monitor -b 115200
```

