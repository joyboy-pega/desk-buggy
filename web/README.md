# ⚡ Desk Buggy Web Studio

> **Web Serial Hardware Flasher & 1:1 Pixel-Perfect Screen Simulator** for ESP32 and ESP32-C3 SuperMini embedded display ecosystems.

---

## 🌟 Key Features

1. **⚡ Push Firmware Directly to Hardware via Web Serial API**
   - Connect your ESP32 or ESP32-C3 board via USB in Chrome, Edge, or Opera.
   - Built with official `esptool-js` with auto chip detection, crystal frequency query, and MAC address reading.
   - 1-Click flashing for all 12 preset firmware builds (`bootloader.bin`, `partitions.bin`, `firmware.bin` at target offsets `0x0000` / `0x1000`, `0x8000`, `0x10000`).
   - Custom `.bin` drag-and-drop flasher with custom memory offset input.
   - Real-time multi-stage flashing progress bar, MD5 verification, and hard MCU reset.

2. **🖥️ 1:1 Pixel-Perfect Hardware Screen Replicas & Interactive Simulators**
   - **Nothing OS Telemetry HUD v2.0 (1.8" ST7735 160x128 Color TFT)**:
     - Real dot-matrix 3x5 typography engine
     - Live IST clock, 500ms colon blink, Pune location & coordinates
     - Ambient & ESP32 silicon core temperature fluctuation
     - Nothing OS 9-segment progress bar & live PWM backlight slider
   - **DOOM 60 FPS Raycaster (0.96" SSD1306 128x64 OLED)**:
     - Real-time 128-column DDA raycaster through 16x16 dungeon map
     - Animated double-barrel shotgun with recoil & muzzle flash
     - Mini-map radar overlay toggle (`M` key)
     - Interactive WASD movement & F / Space fire
     - Switchable PSX DOOM Fire cellular automata mode
   - **Macintosh 1984 Desk Buddy (0.96" SSD1306 OLED)**:
     - 4 moods: Cursive "hello." vector stroke drawing, System 1.0 window desktop, Happy Mac, Sad Mac
   - **Desk Buggy Baymax (0.96" SSD1306 OLED)**:
     - Dual circular eyes with bridge line, sinusoidal breathing, organic blinks, medical scan laser HUD sweep
   - **Chibi Cute Face (0.96" SSD1306 OLED)**:
     - Sparkly anime eyes, blush hashes, cat ear twitches, sub-pixel head bobbing, floating hearts
   - **Sleeping Pikachu Companion (0.96" SSD1306 OLED)**:
     - Curled up snoozing, deep belly breathing expansion, floating Zzz bubbles, cheek electric sparks
   - **Macintosh Clock & Silicon Temp HUD (0.96" SSD1306 OLED)**:
     - Vintage bordered window, live clock, 64-point rolling real-time temperature graph
   - **Hangry Mood Scroll Badge (1.8" ST7735 TFT)**:
     - Signature pastel pink `#FFB6C1` background, 50 FPS marquee ticker `:( hangry`
   - **Smart Weather Station (OLED & TFT)**:
     - Celestial sky arc with sun/moon traversal, meteorological conditions

3. **📟 Web Serial Terminal & Hardware Hotkeys**
   - Live bidirectional serial monitor at 115200, 460800, or 921600 baud.
   - Interactive physical control hotkeys (`+`, `-`, `0-9`, `WASD`, `SPACE/F`, `wifi`, `temp`, `scan`).

---

## 🚀 Deploying to Cloudflare Pages

This application is built with Next.js using `output: 'export'`, which outputs a 100% static, edge-optimized bundle in `web/out/`.

### Option A: One-Command CLI Deployment (Fastest)

```bash
cd web
npm run build
npx wrangler pages deploy out --project-name=desk-buggy
```

### Option B: Cloudflare Pages Git Integration

1. Push this repository to GitHub or GitLab.
2. In the **Cloudflare Dashboard**, navigate to **Compute (Workers & Pages)** > **Create application** > **Pages** > **Connect to Git**.
3. Select your repository and configure the build settings:
   - **Framework preset**: `Next.js (Static Export)`
   - **Root directory**: `web`
   - **Build command**: `npm run build`
   - **Build output directory**: `out`
4. Click **Save and Deploy**. Cloudflare will deploy it globally to all edge locations!

---

## 💻 Local Development

```bash
cd web
npm install
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) in **Google Chrome** or **Microsoft Edge** to enable Web Serial hardware access.
