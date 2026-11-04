# 🎮 Animation & Rendering Engines Design Specification

Comprehensive mathematical formulas, state machine logic, and algorithmic specifications for the four display engines in this repository.

---

## 🤍 Engine 1: BMO & Baymax Dual-Character Engine

Implemented in [`src/main.cpp`](file:///Users/joy/Desktop/project/tea%20party/desk%20buggy/src/main.cpp) for ESP32-WROOM-32 + 1.8" ST7735 TFT.

### 1. Organic Breathing Physics
To prevent the characters from feeling static, a sinusoidal breathing equation calculates vertical body and facial offset at runtime:

$$y_{\text{offset}}(t) = A \cdot \sin\left(\frac{2\pi \cdot t}{T_{\text{period}}}\right)$$

- **Baymax Amplitude ($A$):** 3 pixels, Period $T = 3200\text{ ms}$ (slow, deep medical robot breath).
- **BMO Amplitude ($A$):** 2 pixels, Period $T = 2200\text{ ms}$ (faster, playful console bounce).

### 2. Multi-Stage Organic Eyelids (Baymax)
Baymax's eyes are rendered without artificial circular cutouts:
- **Pupils:** Twin solid black circles with radius $R = 10\text{ px}$, center-to-center distance $D = 54\text{ px}$.
- **Bridge Line:** Horizontal black bar ($W = 34\text{ px}$, $H = 3\text{ px}$) connecting the inner eye edges.
- **Eyelid Slopes:**
  - *Skeptical / Squint:* Inward-slanted upper lids ($\theta = \pm 18^\circ$).
  - *Sad / Empathetic:* Outward-slanted upper lids ($\theta = \mp 18^\circ$).
  - *Sleepy / Tired:* Horizontal droop obscuring the top 60% of the pupil with a sinusoidal sway ($x_{\text{wobble}} = 4 \cdot \sin(t)$).
  - *Natural Blink:* Smooth 3-frame transition:
    $$\text{Open} \longrightarrow \text{Half-Lid (50\%)} \longrightarrow \text{Closed Line} \longrightarrow \text{Half-Lid (50\%)} \longrightarrow \text{Open}$$

### 3. Touch Gesture Discriminator (AS608 Fingerprint)
```text
[Finger Placed on Sensor]
           │
           ▼
[Record Timestamp t1] ─── Single Tap Timer Started (600ms window)
           │
     ┌─────┴─────────────────────────┐
     ▼                               ▼
[Second Tap within 600ms]      [Timer Expires (No 2nd Tap)]
     │                               │
     ▼                               ▼
[DOUBLE TAP TRIGGERED]         [SINGLE TAP TRIGGERED]
Toggle Character               Baymax: Fist Bump ("Ba-la-la-la-la!")
(BMO ⟷ Baymax)                 BMO: Chuckle Laugh & Chirp
```

---

## 🖥️ Engine 2: Macintosh Desk Buddy (60 FPS Engine)

Implemented in [`macintosh-desk-buddy/src/main.cpp`](file:///Users/joy/Desktop/project/tea%20party/desk%20buggy/macintosh-desk-buddy/src/main.cpp) for ESP32-C3 SuperMini + 0.96" SSD1306 OLED.

### 1. 1984 System 1.0 Window Compositor
- **Title Bar:** 12-pixel height, horizontal 1-pixel pinstripes (`y % 2 == 0`).
- **Close Box:** $7 \times 7$ square at $X = 3$, $Y = 2$ with inner clear cutout.
- **Drop Shadows:** 2-pixel black/white offset shadow around active windows.
- **Fonts & Typography:** Custom 5x7 and 7x9 bitmap fonts replicating Chicago 12pt.

### 2. Cursive `"hello."` Spline Interpolator
During startup, the famous 1984 Mac introduction `"hello."` is traced dynamically using parametric spline segments:
$$P(u) = (1-u)^3 P_0 + 3(1-u)^2 u P_1 + 3(1-u) u^2 P_2 + u^3 P_3 \quad \text{for } u \in [0, 1]$$
Rendered point-by-point at 60 FPS with an animated ink cursor tip.

### 3. State Machine Mood Architecture
```text
   ┌────────────────────────────────────────────────────────┐
   │                   BOOT: Cursive "hello."               │
   └───────────────────────────┬────────────────────────────┘
                               │
                               ▼
        ┌──────────────────────────────────────────────┐
        │ 0: HAPPY MAC (Classic smiling monitor)       │
        └──────┬────────────────────────────────┬──────┘
               │                                │
               ▼                                ▼
┌──────────────────────────────┐ ┌──────────────────────────────┐
│ 1: WINK (Playful dot + curve)│ │ 2: THINKING (Watch cursor)   │
└──────────────┬───────────────┘ └──────────────┬───────────────┘
               │                                │
               ▼                                ▼
┌──────────────────────────────┐ ┌──────────────────────────────┐
│ 3: SYSTEM 1.0 DESKTOP        │ │ 4: SLEEPY (Sinusoidal Zzz)   │
└──────────────────────────────┘ └──────────────────────────────┘
```

---

## 🌡️ Engine 3: Macintosh Clock & Temperature Monitor

Implemented in [`macintosh-clock-temp/src/main.cpp`](file:///Users/joy/Desktop/project/tea%20party/desk%20buggy/macintosh-clock-temp/src/main.cpp) for ESP32-C3 SuperMini + 0.96" SSD1306 OLED.

### 1. Silicon Core Temperature Filtering
The ESP32-C3 on-chip temperature sensor can fluctuate due to short CPU processing bursts. An Exponential Moving Average (EMA) filter stabilizes readings:

$$T_{\text{filtered}}[n] = \alpha \cdot T_{\text{raw}}[n] + (1 - \alpha) \cdot T_{\text{filtered}}[n-1]$$

With smoothing coefficient $\alpha = 0.12$.

### 2. Dynamic 64-Sample Scrolling Graph
- **Sampling Rate:** 1 sample every 2.0 seconds (128 seconds total history window).
- **Auto-Ranging:** Computes $T_{\min} = \min_{i}(T_i) - 1.0$ and $T_{\max} = \max_{i}(T_i) + 1.0$.
- **Pixel Mapping:**
  $$Y_{\text{pixel}}(i) = Y_{\text{bottom}} - \left( \frac{T_i - T_{\min}}{T_{\max} - T_{\min}} \right) \cdot H_{\text{graph}}$$

---

## 🔫 Engine 4: DOOM 60 FPS 3D Raycaster & PSX Fire Engine

Implemented in [`doom-oled/src/main.cpp`](file:///Users/joy/Desktop/project/tea%20party/desk%20buggy/doom-oled/src/main.cpp) for ESP32-C3 SuperMini + 0.96" SSD1306 OLED.

### 1. 128-Ray DDA Raycasting Algorithm
For every screen column $x \in [0, 127]$:
1. Camera space coordinate: $\text{cameraX} = \frac{2x}{W} - 1 \in [-1, 1]$.
2. Ray direction: $\vec{R} = \vec{\text{dir}} + \vec{\text{plane}} \cdot \text{cameraX}$.
3. DDA steps cell-by-cell until hit:
   $$d_{\perp} = \begin{cases} \text{sideDistX} - \Delta\text{DistX} & \text{if X-wall} \\ \text{sideDistY} - \Delta\text{DistY} & \text{if Y-wall} \end{cases}$$
4. Wall line height on screen:
   $$h_{\text{wall}} = \left\lfloor \frac{H_{\text{view}}}{d_{\perp}} \right\rfloor$$
5. Store $d_{\perp}$ in 1D array `zBuffer[x]` for sprite depth testing.

### 2. Depth Dithering Matrix
To simulate 3D lighting on a 1-bit monochrome display:
- **Distance $< 2.0\text{ m}$:** Solid white vertical bar (or alternating vertical stripes for Y-side shading).
- **Distance $2.0\text{ m} - 4.2\text{ m}$:** 50% checkerboard dither:
  $$\text{pixel\_on} = ((x + y) \pmod 2 == 0)$$
- **Distance $4.2\text{ m} - 7.0\text{ m}$:** 25% sparse stipple:
  $$\text{pixel\_on} = (x \pmod 2 == 0) \land (y \pmod 2 == 0)$$
- **Distance $> 7.0\text{ m}$:** Top and bottom outline rim pixels only.

### 3. 3D Sprite Projection & Z-Buffer Occlusion
Given sprite position $\vec{S} = (S_x, S_y)$:
1. Transform using inverse camera matrix:
   $$\begin{bmatrix} \text{transX} \\ \text{transY} \end{bmatrix} = \frac{1}{\det} \begin{bmatrix} \text{dirY} & -\text{dirX} \\ -\text{planeY} & \text{planeX} \end{bmatrix} \begin{bmatrix} S_x - P_x \\ S_y - P_y \end{bmatrix}$$
2. Screen center column:
   $$X_{\text{screen}} = \frac{W}{2} \cdot \left(1 + \frac{\text{transX}}{\text{transY}}\right)$$
3. If $\text{transY} < \text{zBuffer}[X_{\text{col}}]$, the sprite column is in front of the wall and gets rendered.

### 4. Procedural PSX DOOM Fire Cellular Automata
Buffer: $128 \times 48$ intensity cells ($0 \le \text{heat} \le 7$).
- **Heat Source:** Bottom row initialized to maximum value ($\text{heat} = 7$).
- **Upward Convection & Wind Drift:**
  For each pixel $(x, y)$:
  $$\text{decay} = \text{random}(0, 1), \quad \text{wind} = \text{random}(0, 3) - 1$$
  $$\text{fire}[y-1, x + \text{wind}] = \max(0, \, \text{fire}[y, x] - \text{decay})$$
- Intensities mapped to 8 distinct monochrome dither densities running at a solid **60 FPS**.
