'use client';

import React, { useEffect, useRef, useState } from 'react';
import { FirmwarePreset } from '../lib/firmwarePresets';
import { 
  Play, Pause, RotateCcw, ZoomIn, ZoomOut, Maximize2, 
  Sparkles, Monitor, Cpu, Radio, Shield, Zap, Flame, Eye,
  Sliders, Compass
} from 'lucide-react';

interface HardwareScreenSimulatorProps {
  preset: FirmwarePreset;
  onSendCommand?: (command: string) => void;
}

export const HardwareScreenSimulator: React.FC<HardwareScreenSimulatorProps> = ({
  preset,
  onSendCommand,
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [isPlaying, setIsPlaying] = useState<boolean>(true);
  const [scale, setScale] = useState<number>(3); // 3x default scale
  const [shaderMode, setShaderMode] = useState<'matrix' | 'glow' | 'pixel'>('matrix');
  const [bezelStyle, setBezelStyle] = useState<'modern' | 'vintage' | 'oled-board' | 'bare'>('modern');
  const [fps, setFps] = useState<number>(preset.fps);

  // Interactive firmware controls
  const [brightness, setBrightness] = useState<number>(128); // 0-255
  const [doomState, setDoomState] = useState<'raycaster' | 'fire'>('raycaster');
  const [doomShowMap, setDoomShowMap] = useState<boolean>(false);
  const [macMood, setMacMood] = useState<number>(1);
  const [baymaxLaser, setBaymaxLaser] = useState<boolean>(false);
  const [chibiExpr, setChibiExpr] = useState<number>(0);
  const [pikaAwake, setPikaAwake] = useState<boolean>(false);

  // References for animation state
  const animFrameRef = useRef<number>(0);
  const stateRef = useRef<{
    time: number;
    lastTick: number;
    playerX: number;
    playerY: number;
    playerAngle: number;
    shotgunRecoil: number;
    shotgunFlash: boolean;
    fireBuffer: number[][];
    pikaBreathe: number;
    baymaxBlink: number;
    helloProgress: number;
    macCursorX: number;
    macCursorY: number;
    tempHistory: number[];
    marqueeOffset: number;
  }>({
    time: 0,
    lastTick: performance.now(),
    playerX: 3.5,
    playerY: 3.5,
    playerAngle: 0.2,
    shotgunRecoil: 0,
    shotgunFlash: false,
    fireBuffer: Array.from({ length: 64 }, () => Array(128).fill(0)),
    pikaBreathe: 0,
    baymaxBlink: 9,
    helloProgress: 0,
    macCursorX: 20,
    macCursorY: 25,
    tempHistory: Array.from({ length: 64 }, (_, i) => 36 + Math.sin(i / 5) * 2),
    marqueeOffset: 160,
  });

  // DOOM 16x16 map
  const DOOM_MAP = [
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
    [1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1],
    [1,0,1,1,0,0,1,0,1,1,1,1,0,1,0,1],
    [1,0,1,0,0,0,0,0,0,0,0,1,0,1,0,1],
    [1,0,1,0,1,1,1,1,1,0,0,1,0,1,0,1],
    [1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,1],
    [1,0,0,0,1,0,0,0,1,0,1,1,1,0,0,1],
    [1,1,1,0,0,0,0,0,0,0,1,0,1,0,0,1],
    [1,0,0,0,0,1,1,0,0,0,1,0,1,0,0,1],
    [1,0,1,1,0,1,1,0,1,0,0,0,0,0,0,1],
    [1,0,1,0,0,0,0,0,1,0,1,1,1,1,0,1],
    [1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,1],
    [1,0,0,0,1,1,0,0,0,0,1,0,0,1,0,1],
    [1,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1],
    [1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1],
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
  ];

  // Dot Matrix 3x5 font table
  const DOT_DIGITS = [
    0b111101101101111, // 0
    0b010010010010010, // 1
    0b111001111100111, // 2
    0b111001111001111, // 3
    0b101101111001001, // 4
    0b111100111001111, // 5
    0b111100111101111, // 6
    0b111001001001001, // 7
    0b111101111101111, // 8
    0b111101111001111, // 9
    0b000010000010000, // :
  ];

  // Handle DOOM Shotgun Fire
  const fireDoomShotgun = () => {
    stateRef.current.shotgunRecoil = 12;
    stateRef.current.shotgunFlash = true;
    setTimeout(() => {
      stateRef.current.shotgunFlash = false;
    }, 90);
    if (onSendCommand) onSendCommand('f');
  };

  // DOOM Keyboard Listeners
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (preset.id !== 'oled-doom') return;
      const s = stateRef.current;
      const moveSpeed = 0.25;
      const rotSpeed = 0.15;

      if (e.key === 'w' || e.key === 'ArrowUp') {
        const nx = s.playerX + Math.cos(s.playerAngle) * moveSpeed;
        const ny = s.playerY + Math.sin(s.playerAngle) * moveSpeed;
        if (DOOM_MAP[Math.floor(ny)]?.[Math.floor(nx)] === 0) {
          s.playerX = nx;
          s.playerY = ny;
        }
      } else if (e.key === 's' || e.key === 'ArrowDown') {
        const nx = s.playerX - Math.cos(s.playerAngle) * moveSpeed;
        const ny = s.playerY - Math.sin(s.playerAngle) * moveSpeed;
        if (DOOM_MAP[Math.floor(ny)]?.[Math.floor(nx)] === 0) {
          s.playerX = nx;
          s.playerY = ny;
        }
      } else if (e.key === 'a' || e.key === 'ArrowLeft') {
        s.playerAngle -= rotSpeed;
      } else if (e.key === 'd' || e.key === 'ArrowRight') {
        s.playerAngle += rotSpeed;
      } else if (e.key === ' ' || e.key === 'f') {
        fireDoomShotgun();
      } else if (e.key === 'm') {
        setDoomShowMap((prev) => !prev);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [preset.id]);

  // Main Canvas Render Loop
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let frameId: number;
    let lastTime = performance.now();
    let frameCounter = 0;
    let fpsTimer = performance.now();

    const render = (now: number) => {
      frameCounter++;
      if (now - fpsTimer >= 1000) {
        setFps(frameCounter);
        frameCounter = 0;
        fpsTimer = now;
      }

      const delta = (now - lastTime) / 1000;
      lastTime = now;
      const s = stateRef.current;
      s.time += delta;

      // Clear Canvas
      ctx.fillStyle = '#000000';
      ctx.fillRect(0, 0, preset.width, preset.height);

      // Render preset specific screen
      switch (preset.id) {
        case 'esp32-nothing-hud':
          renderNothingHUD(ctx, s.time, brightness);
          break;
        case 'oled-doom':
          if (doomState === 'raycaster') {
            renderDoomRaycaster(ctx, s);
          } else {
            renderDoomFire(ctx, s);
          }
          break;
        case 'oled-mac-buddy':
          renderMacBuddy(ctx, s.time, macMood);
          break;
        case 'oled-desk-buggy':
          renderBaymax(ctx, s.time, baymaxLaser);
          break;
        case 'oled-mac-clock':
          renderMacClockGraph(ctx, s.time, s.tempHistory);
          break;
        case 'oled-chibi-face':
          renderChibiFace(ctx, s.time, chibiExpr);
          break;
        case 'oled-pikachu':
          renderPikachu(ctx, s.time, pikaAwake);
          break;
        case 'oled-weather':
          renderOledWeather(ctx, s.time);
          break;
        case 'tft-anime-telemetry':
          renderAnimeWatch(ctx, s.time);
          break;
        case 'tft-scroll-badge':
          renderScrollBadge(ctx, s);
          break;
        case 'tft-weather':
          renderTftWeather(ctx, s.time);
          break;
        case 'tft-macbook-cam':
          renderMacCamHUD(ctx, s.time);
          break;
        default:
          renderDefaultFallback(ctx, preset.title);
      }

      if (isPlaying) {
        frameId = requestAnimationFrame(render);
      }
    };

    frameId = requestAnimationFrame(render);
    return () => cancelAnimationFrame(frameId);
  }, [preset, isPlaying, brightness, doomState, doomShowMap, macMood, baymaxLaser, chibiExpr, pikaAwake]);

  // --------------------------------------------------------------------------
  // 1. NOTHING OS TELEMETRY HUD (ST7735 160x128 1:1 REPLICA)
  // --------------------------------------------------------------------------
  const renderNothingHUD = (ctx: CanvasRenderingContext2D, time: number, bright: number) => {
    const W = 160;
    const H = 128;

    // Apply brightness dimming factor
    const alpha = Math.max(0.15, bright / 255);
    ctx.save();
    ctx.globalAlpha = alpha;

    // Background
    ctx.fillStyle = '#08080a';
    ctx.fillRect(0, 0, W, H);

    // 1. Header (Y: 0-16)
    // Red pulsing indicator
    const pulse = 1.5 + Math.sin(time * 6) * 0.5;
    ctx.fillStyle = '#E01A22';
    ctx.beginPath();
    ctx.arc(7, 8, pulse, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#FFFFFF';
    ctx.font = '7px monospace';
    ctx.fillText('NOTHING (R)', 15, 10);

    // Brightness pill
    ctx.strokeStyle = '#222226';
    ctx.lineWidth = 1;
    ctx.strokeRect(92, 3, 62, 10);
    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText(`${Math.round((bright * 100) / 255)}% | WIFI`, 96, 10);

    // Header dotted divider
    ctx.fillStyle = '#222226';
    for (let x = 4; x < W - 4; x += 4) {
      ctx.fillRect(x, 16, 1, 1);
    }

    // 2. Main Card: Clock & Weather (Y: 19 - 68)
    ctx.fillStyle = '#111113';
    ctx.strokeStyle = '#242428';
    ctx.strokeRect(4, 19, 152, 49);
    ctx.fillRect(5, 20, 150, 47);

    // TIME (IST) Label
    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText('TIME (IST)', 10, 28);

    // Current Time
    const now = new Date();
    const hrs = String(now.getHours()).padStart(2, '0');
    const mins = String(now.getMinutes()).padStart(2, '0');
    const secs = String(now.getSeconds()).padStart(2, '0');
    const colonVisible = Math.floor(time * 2) % 2 === 0;

    // 3x5 Dot Matrix Time rendering
    const timeStr = `${hrs}:${mins}:${secs}`;
    let curX = 10;
    const dotY = 33;
    const dotSize = 2;
    const dotGap = 1;

    for (let i = 0; i < timeStr.length; i++) {
      const char = timeStr[i];
      if (char === ':') {
        if (colonVisible) {
          drawDotMatrixChar(ctx, curX, dotY, 10, '#FFFFFF', dotSize, dotGap);
        }
        curX += 6;
      } else {
        const digit = parseInt(char, 10);
        drawDotMatrixChar(ctx, curX, dotY, digit, '#FFFFFF', dotSize, dotGap);
        curX += 9;
      }
    }

    // Date & City subtext
    ctx.fillStyle = '#FFFFFF';
    ctx.font = '6px monospace';
    ctx.fillText('SUN 15 NOV', 10, 58);
    ctx.fillStyle = '#E01A22';
    ctx.fillText(' • ', 54, 58);
    ctx.fillStyle = '#7C7C82';
    ctx.fillText('PUNE', 66, 58);

    // Vertical card divider
    ctx.fillStyle = '#222226';
    for (let y = 24; y < 64; y += 3) {
      ctx.fillRect(106, y, 1, 1);
    }

    // Weather on right side of card
    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText('WEATHER', 112, 28);

    ctx.fillStyle = '#FFFFFF';
    ctx.font = 'bold 15px monospace';
    ctx.fillText('28°C', 111, 46);

    ctx.fillStyle = '#E01A22';
    ctx.font = '6px monospace';
    ctx.fillText('CLEAR', 112, 58);

    // 3. Lower Left Card: Location & Coordinates (Y: 72 - 124)
    ctx.fillStyle = '#111113';
    ctx.strokeRect(4, 72, 73, 52);
    ctx.fillRect(5, 73, 71, 50);

    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText('LOCATION', 9, 81);

    ctx.fillStyle = '#FFFFFF';
    ctx.font = '7px monospace';
    ctx.fillText('PUNE, MH', 9, 93);

    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText('18.5 N 73.8 E', 9, 104);

    ctx.fillStyle = '#E01A22';
    ctx.beginPath();
    ctx.arc(12, 116, 2, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#FFFFFF';
    ctx.font = '6px monospace';
    ctx.fillText('MH, INDIA', 18, 118);

    // 4. Lower Right Card: ESP32 Telemetry (Y: 72 - 124)
    ctx.strokeRect(83, 72, 73, 52);
    ctx.fillRect(84, 73, 71, 50);

    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText('ESP32 CORE', 88, 81);

    const coreTemp = (38.2 + Math.sin(time / 2) * 0.4).toFixed(1);
    ctx.fillStyle = '#FFFFFF';
    ctx.font = '7px monospace';
    ctx.fillText(`TEMP ${coreTemp}°C`, 88, 93);

    ctx.fillStyle = '#7C7C82';
    ctx.font = '6px monospace';
    ctx.fillText('HEAP 242 KB', 88, 104);

    // Nothing OS 9-dot animated progress bar
    const filledDots = (Math.floor(time) % 9) + 1;
    for (let d = 0; d < 9; d++) {
      ctx.fillStyle = d < filledDots ? '#E01A22' : '#2C2C32';
      ctx.fillRect(88 + d * 7, 112, 4, 3);
    }

    ctx.restore();
  };

  const drawDotMatrixChar = (
    ctx: CanvasRenderingContext2D,
    x: number,
    y: number,
    idx: number,
    color: string,
    dotSize: number,
    gap: number
  ) => {
    if (idx < 0 || idx >= DOT_DIGITS.length) return;
    const pattern = DOT_DIGITS[idx];
    ctx.fillStyle = color;
    for (let row = 0; row < 5; row++) {
      for (let col = 0; col < 3; col++) {
        const bitIdx = 14 - (row * 3 + col);
        if ((pattern >> bitIdx) & 0x01) {
          ctx.fillRect(x + col * (dotSize + gap), y + row * (dotSize + gap), dotSize, dotSize);
        }
      }
    }
  };

  // --------------------------------------------------------------------------
  // 2. DOOM 60 FPS RAYCASTER (SSD1306 128x64 OLED 1:1 REPLICA)
  // --------------------------------------------------------------------------
  const renderDoomRaycaster = (ctx: CanvasRenderingContext2D, s: any) => {
    const W = 128;
    const H = 64;
    const viewH = 52;

    // Sky & Floor
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    // 128 Ray DDA Engine
    const fov = 1.047; // 60 degrees in radians
    const halfFov = fov / 2;

    for (let col = 0; col < W; col++) {
      const rayAngle = s.playerAngle - halfFov + (col / W) * fov;
      const cosAngle = Math.cos(rayAngle);
      const sinAngle = Math.sin(rayAngle);

      let dist = 0.05;
      let hit = false;
      let side = 0;

      while (!hit && dist < 12) {
        dist += 0.04;
        const checkX = Math.floor(s.playerX + cosAngle * dist);
        const checkY = Math.floor(s.playerY + sinAngle * dist);

        if (checkX >= 0 && checkX < 16 && checkY >= 0 && checkY < 16) {
          if (DOOM_MAP[checkY][checkX] > 0) {
            hit = true;
          }
        }
      }

      // Fisheye correction
      const correctedDist = dist * Math.cos(rayAngle - s.playerAngle);
      const wallHeight = Math.min(viewH, Math.floor(viewH / (correctedDist || 0.1)));
      const top = Math.floor((viewH - wallHeight) / 2);
      const bottom = top + wallHeight;

      // Draw Wall column with depth dithering
      ctx.fillStyle = '#00F0FF'; // OLED Cyan
      for (let y = top; y < bottom; y++) {
        // Distance dithering pattern
        if (correctedDist > 4.5 && (col + y) % 2 === 0) continue;
        if (correctedDist > 7 && (col % 2 !== 0 || y % 2 !== 0)) continue;
        ctx.fillRect(col, y, 1, 1);
      }
    }

    // Double-Barrel Shotgun Sprite (Bottom Center)
    const gunY = 32 + (s.shotgunRecoil || 0);
    ctx.fillStyle = '#00F0FF';
    // Gun barrels
    ctx.fillRect(58, gunY, 12, 20);
    ctx.fillRect(56, gunY + 6, 16, 14);
    // Dark barrel bores
    ctx.fillStyle = '#000000';
    ctx.fillRect(60, gunY + 1, 3, 4);
    ctx.fillRect(65, gunY + 1, 3, 4);

    // Muzzle Flash
    if (s.shotgunFlash) {
      ctx.fillStyle = '#FFFFFF';
      ctx.beginPath();
      ctx.arc(64, gunY - 4, 8 + Math.random() * 4, 0, Math.PI * 2);
      ctx.fill();
    }

    // Shotgun recoil recovery
    if (s.shotgunRecoil > 0) {
      s.shotgunRecoil = Math.max(0, s.shotgunRecoil - 1.2);
    }

    // Mini-map Overlay
    if (doomShowMap) {
      ctx.fillStyle = 'rgba(0, 0, 0, 0.85)';
      ctx.fillRect(2, 2, 34, 34);
      ctx.strokeStyle = '#00F0FF';
      ctx.strokeRect(2, 2, 34, 34);

      for (let my = 0; my < 16; my++) {
        for (let mx = 0; mx < 16; mx++) {
          if (DOOM_MAP[my][mx] > 0) {
            ctx.fillStyle = '#00F0FF';
            ctx.fillRect(3 + mx * 2, 3 + my * 2, 2, 2);
          }
        }
      }
      // Player dot & directional beam
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(3 + s.playerX * 2, 3 + s.playerY * 2, 2, 2);
    }

    // HUD Bar at Bottom (Y: 53-64)
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 52, W, 12);
    ctx.strokeStyle = '#00F0FF';
    ctx.beginPath();
    ctx.moveTo(0, 52);
    ctx.lineTo(W, 52);
    ctx.stroke();

    ctx.fillStyle = '#00F0FF';
    ctx.font = '6px monospace';
    ctx.fillText('AMMO 48', 4, 61);
    ctx.fillText('HEALTH 100%', 44, 61);
    ctx.fillText('60 FPS', 96, 61);
  };

  // DOOM Fire simulation
  const renderDoomFire = (ctx: CanvasRenderingContext2D, s: any) => {
    const W = 128;
    const H = 64;
    const fire = s.fireBuffer;

    // Populate bottom row with hot coals
    for (let x = 0; x < W; x++) {
      fire[H - 1][x] = Math.random() > 0.15 ? 15 : 0;
    }

    // Propagate flames upwards
    for (let y = 0; y < H - 1; y++) {
      for (let x = 0; x < W; x++) {
        const decay = Math.floor(Math.random() * 2);
        const srcX = (x + Math.floor(Math.random() * 3) - 1 + W) % W;
        const val = fire[y + 1][srcX] - decay;
        fire[y][x] = Math.max(0, val);
      }
    }

    // Render OLED Fire pixels
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    ctx.fillStyle = '#00F0FF';
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < W; x++) {
        const intensity = fire[y][x];
        if (intensity > 6) {
          ctx.fillRect(x, y, 1, 1);
        } else if (intensity > 3 && (x + y) % 2 === 0) {
          ctx.fillRect(x, y, 1, 1);
        }
      }
    }

    ctx.fillStyle = '#000000';
    ctx.fillRect(20, 24, 88, 16);
    ctx.strokeStyle = '#00F0FF';
    ctx.strokeRect(20, 24, 88, 16);
    ctx.fillStyle = '#00F0FF';
    ctx.font = 'bold 8px monospace';
    ctx.fillText('PSX DOOM FIRE 60FPS', 24, 35);
  };

  // --------------------------------------------------------------------------
  // 3. MACINTOSH 1984 DESK BUDDY (SSD1306 128x64 OLED)
  // --------------------------------------------------------------------------
  const renderMacBuddy = (ctx: CanvasRenderingContext2D, time: number, mood: number) => {
    const W = 128;
    const H = 64;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    if (mood === 1) {
      // Iconic cursive "hello." animated stroke drawing
      ctx.strokeStyle = '#FFFFFF';
      ctx.lineWidth = 2;
      ctx.beginPath();
      // 'h'
      ctx.moveTo(24, 46);
      ctx.lineTo(24, 22);
      ctx.quadraticCurveTo(24, 32, 34, 32);
      ctx.lineTo(34, 46);
      // 'e'
      ctx.moveTo(38, 38);
      ctx.arc(43, 38, 5, 0, Math.PI * 1.8);
      // 'l' 'l'
      ctx.moveTo(52, 22);
      ctx.lineTo(52, 46);
      ctx.moveTo(59, 22);
      ctx.lineTo(59, 46);
      // 'o'
      ctx.moveTo(74, 38);
      ctx.arc(69, 38, 5, 0, Math.PI * 2);
      // '.'
      ctx.moveTo(77, 45);
      ctx.arc(77, 45, 1.5, 0, Math.PI * 2);
      ctx.stroke();

      ctx.fillStyle = '#FFFFFF';
      ctx.font = '6px monospace';
      ctx.fillText('1984 MACINTOSH', 28, 58);
    } else if (mood === 2) {
      // System 1.0 Desktop with menu bar & window
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(0, 0, W, 8); // Menu Bar
      ctx.fillStyle = '#000000';
      ctx.font = '6px sans-serif';
      ctx.fillText(' File  Edit  View  Special', 4, 6);

      // Window Frame
      const winX = 22 + Math.sin(time * 0.8) * 12;
      const winY = 14 + Math.cos(time * 0.6) * 4;
      ctx.strokeStyle = '#FFFFFF';
      ctx.strokeRect(winX, winY, 78, 42);
      // Title bar
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(winX, winY, 78, 8);
      ctx.fillStyle = '#000000';
      ctx.fillText('Desk Buddy v1.0', winX + 10, winY + 6);
      // Close box
      ctx.strokeRect(winX + 2, winY + 2, 4, 4);

      // Window content: Floppy Disk & smile
      ctx.fillStyle = '#FFFFFF';
      ctx.strokeRect(winX + 28, winY + 14, 22, 20);
      ctx.fillRect(winX + 32, winY + 16, 14, 8);
    } else if (mood === 3) {
      // Happy Mac CRT with chassis
      const blink = Math.sin(time * 3) > 0.95;
      // Mac Chassis
      ctx.strokeStyle = '#FFFFFF';
      ctx.strokeRect(36, 6, 56, 52);
      // CRT Screen
      ctx.strokeRect(44, 12, 40, 30);
      // Screen face
      if (blink) {
        ctx.fillRect(52, 24, 6, 1);
        ctx.fillRect(68, 24, 6, 1);
      } else {
        ctx.fillRect(52, 22, 4, 4);
        ctx.fillRect(68, 22, 4, 4);
      }
      // Smile
      ctx.beginPath();
      ctx.arc(62, 28, 6, 0.2, Math.PI - 0.2);
      ctx.stroke();
      // Floppy drive slot
      ctx.fillRect(48, 47, 32, 2);
    } else {
      // Sad Mac with crash code
      ctx.strokeStyle = '#FFFFFF';
      ctx.strokeRect(36, 6, 56, 52);
      ctx.strokeRect(44, 12, 40, 30);
      // X Eyes
      ctx.font = '8px monospace';
      ctx.fillStyle = '#FFFFFF';
      ctx.fillText('X', 52, 25);
      ctx.fillText('X', 68, 25);
      // Frown
      ctx.beginPath();
      ctx.arc(62, 34, 6, Math.PI + 0.2, -0.2);
      ctx.stroke();
      // Error code
      ctx.fillText('0F0001', 50, 48);
    }
  };

  // --------------------------------------------------------------------------
  // 4. DESK BUGGY BAYMAX (SSD1306 128x64 OLED)
  // --------------------------------------------------------------------------
  const renderBaymax = (ctx: CanvasRenderingContext2D, time: number, laser: boolean) => {
    const W = 128;
    const H = 64;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    // Natural sinusoidal breathing
    const breath = Math.sin(time * 2) * 1.5;
    const eyeY = 32 + breath;
    const leftX = 42;
    const rightX = 86;
    const radius = 9;

    // Organic blink cycle
    const blinkCycle = Math.sin(time * 1.8);
    let blinkH = radius;
    if (blinkCycle > 0.92) {
      blinkH = 1; // Eyelids closed
    } else if (blinkCycle > 0.85) {
      blinkH = 4; // Half blink
    }

    ctx.fillStyle = '#FFFFFF';
    ctx.strokeStyle = '#FFFFFF';

    if (blinkH >= radius) {
      // Fully open circular eyes
      ctx.beginPath();
      ctx.arc(leftX, eyeY, radius, 0, Math.PI * 2);
      ctx.arc(rightX, eyeY, radius, 0, Math.PI * 2);
      ctx.fill();

      // Connecting bridge bar between eyes
      ctx.fillRect(leftX, eyeY - 2, rightX - leftX, 4);
    } else if (blinkH > 1) {
      // Half-closed eyelids
      ctx.fillRect(leftX - radius, eyeY - blinkH, radius * 2, blinkH * 2);
      ctx.fillRect(rightX - radius, eyeY - blinkH, radius * 2, blinkH * 2);
      ctx.fillRect(leftX, eyeY - 1, rightX - leftX, 3);
    } else {
      // Slit eyes
      ctx.fillRect(leftX - radius, eyeY - 1, (rightX - leftX) + radius * 2, 2);
    }

    // Health Medical Scan Laser Line
    if (laser) {
      const scanY = 10 + (Math.sin(time * 8) * 0.5 + 0.5) * 44;
      ctx.fillStyle = '#00F0FF';
      ctx.fillRect(0, scanY, W, 2);
      ctx.font = '6px monospace';
      ctx.fillText('DIAGNOSTIC HEALTH SCAN...', 16, 12);
    }

    // Status footer
    ctx.fillStyle = '#FFFFFF';
    ctx.font = '6px monospace';
    ctx.fillText('ESP32-C3 SUPERMINI OK', 24, 60);
  };

  // --------------------------------------------------------------------------
  // 5. CHIBI CUTE FACE MOTION (SSD1306 128x64 OLED)
  // --------------------------------------------------------------------------
  const renderChibiFace = (ctx: CanvasRenderingContext2D, time: number, expr: number) => {
    const W = 128;
    const H = 64;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    const bob = Math.sin(time * 3) * 1.5;
    const eyeY = 30 + bob;

    // Cat ears with twitch physics
    const twitch = Math.sin(time * 6) > 0.85 ? 3 : 0;
    ctx.strokeStyle = '#FFFFFF';
    ctx.lineWidth = 1.5;
    // Left ear
    ctx.beginPath();
    ctx.moveTo(22, 16 + bob);
    ctx.lineTo(32, 6 + bob - twitch);
    ctx.lineTo(40, 18 + bob);
    ctx.stroke();
    // Right ear
    ctx.beginPath();
    ctx.moveTo(88, 18 + bob);
    ctx.lineTo(96, 6 + bob + twitch);
    ctx.lineTo(106, 16 + bob);
    ctx.stroke();

    // Eyes
    ctx.fillStyle = '#FFFFFF';
    if (expr === 1) {
      // Joy Arch ^ ^
      ctx.beginPath();
      ctx.arc(42, eyeY + 4, 10, Math.PI, 0);
      ctx.arc(86, eyeY + 4, 10, Math.PI, 0);
      ctx.stroke();
    } else {
      // Sparkly anime eyes
      ctx.beginPath();
      ctx.ellipse(42, eyeY, 8, 12, 0, 0, Math.PI * 2);
      ctx.ellipse(86, eyeY, 8, 12, 0, 0, Math.PI * 2);
      ctx.fill();

      // Eye Sparkle Glints (Black cutouts)
      ctx.fillStyle = '#000000';
      ctx.beginPath();
      ctx.arc(44, eyeY - 4, 3.5, 0, Math.PI * 2);
      ctx.arc(40, eyeY + 4, 2, 0, Math.PI * 2);
      ctx.arc(88, eyeY - 4, 3.5, 0, Math.PI * 2);
      ctx.arc(84, eyeY + 4, 2, 0, Math.PI * 2);
      ctx.fill();
    }

    // Blushing cheek hash marks
    ctx.strokeStyle = '#FFFFFF';
    ctx.lineWidth = 1;
    for (let c = 0; c < 3; c++) {
      ctx.beginPath();
      ctx.moveTo(24 + c * 3, eyeY + 10);
      ctx.lineTo(26 + c * 3, eyeY + 14);
      ctx.moveTo(96 + c * 3, eyeY + 10);
      ctx.lineTo(98 + c * 3, eyeY + 14);
      ctx.stroke();
    }

    // Cat Mouth :3
    ctx.beginPath();
    ctx.arc(61, eyeY + 12, 3, 0, Math.PI);
    ctx.arc(67, eyeY + 12, 3, 0, Math.PI);
    ctx.stroke();

    // Floating heart particle
    const heartY = 20 - (time * 12 % 30);
    ctx.fillStyle = '#FFFFFF';
    ctx.font = '8px sans-serif';
    ctx.fillText('♥', 74, heartY);
  };

  // --------------------------------------------------------------------------
  // 6. SLEEPING PIKACHU COMPANION (SSD1306 128x64 OLED)
  // --------------------------------------------------------------------------
  const renderPikachu = (ctx: CanvasRenderingContext2D, time: number, awake: boolean) => {
    const W = 128;
    const H = 64;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    // Deep rhythmic belly breathing
    const breathe = Math.sin(time * 2) * 2;

    ctx.strokeStyle = '#FFFFFF';
    ctx.lineWidth = 1.5;

    // Curled body
    ctx.beginPath();
    ctx.ellipse(64, 40 + breathe * 0.5, 34 + breathe, 18 + breathe * 0.6, 0, 0, Math.PI * 2);
    ctx.stroke();

    // Head
    ctx.beginPath();
    ctx.arc(38, 36, 14, 0, Math.PI * 2);
    ctx.stroke();

    // Laying floppy ears
    ctx.beginPath();
    ctx.moveTo(30, 24);
    ctx.lineTo(12, 28);
    ctx.lineTo(24, 32);
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(34, 23);
    ctx.lineTo(44, 12);
    ctx.lineTo(46, 24);
    ctx.stroke();

    // Tail on right
    ctx.beginPath();
    ctx.moveTo(96, 36);
    ctx.lineTo(108, 28);
    ctx.lineTo(104, 24);
    ctx.lineTo(118, 14);
    ctx.stroke();

    if (awake) {
      // Wide awake round eyes
      ctx.fillStyle = '#FFFFFF';
      ctx.beginPath();
      ctx.arc(34, 34, 3, 0, Math.PI * 2);
      ctx.fill();
    } else {
      // Closed peaceful sleeping eye curve
      ctx.beginPath();
      ctx.arc(34, 34, 3, 0.2, Math.PI - 0.2);
      ctx.stroke();
    }

    // Rosy electric cheek spark
    ctx.fillStyle = '#FFFFFF';
    ctx.beginPath();
    ctx.arc(30, 42, 3, 0, Math.PI * 2);
    ctx.fill();

    // Floating Zzz bubbles
    const zOffset = (time * 10) % 24;
    ctx.font = '8px monospace';
    ctx.fillText('z', 72, 24 - zOffset * 0.6);
    ctx.font = '10px monospace';
    ctx.fillText('Z', 84, 18 - zOffset * 0.8);
    ctx.font = '12px monospace';
    ctx.fillText('Z', 98, 12 - zOffset);
  };

  // --------------------------------------------------------------------------
  // 7. MACINTOSH CLOCK & ROLLING TEMP GRAPH (SSD1306 128x64 OLED)
  // --------------------------------------------------------------------------
  const renderMacClockGraph = (ctx: CanvasRenderingContext2D, time: number, tempHist: number[]) => {
    const W = 128;
    const H = 64;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    // Retro Mac OS Window Box
    ctx.strokeStyle = '#FFFFFF';
    ctx.strokeRect(2, 2, 124, 60);
    // Header
    ctx.fillStyle = '#FFFFFF';
    ctx.fillRect(2, 2, 124, 8);
    ctx.fillStyle = '#000000';
    ctx.font = '6px monospace';
    ctx.fillText('TEMP TELEMETRY HUD', 18, 8);

    // Live Digital Time
    const now = new Date();
    const timeStr = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
    ctx.fillStyle = '#FFFFFF';
    ctx.font = 'bold 12px monospace';
    ctx.fillText(timeStr, 8, 24);

    const curTemp = (36.8 + Math.sin(time / 2) * 1.5).toFixed(1);
    ctx.font = '7px monospace';
    ctx.fillText(`CORE ${curTemp}°C`, 78, 22);

    // 64-Point Scrolling Graph Box (Y: 28 to 58)
    ctx.strokeStyle = '#FFFFFF';
    ctx.strokeRect(8, 28, 112, 28);

    // Horizontal baseline
    ctx.fillStyle = '#333333';
    ctx.fillRect(9, 42, 110, 1);

    // Draw historical temperature sparkline
    ctx.strokeStyle = '#FFFFFF';
    ctx.beginPath();
    for (let i = 0; i < 56; i++) {
      const val = 36 + Math.sin((time * 2 + i) / 4) * 2;
      const py = 50 - ((val - 33) / 8) * 18;
      const px = 10 + i * 2;
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    ctx.fillStyle = '#FFFFFF';
    ctx.font = '5px monospace';
    ctx.fillText('CPU 160MHz', 12, 54);
    ctx.fillText('HEAP 288K', 76, 54);
  };

  // --------------------------------------------------------------------------
  // 8. HANGRY MOOD SCROLL BADGE (ST7735 160x128 TFT)
  // --------------------------------------------------------------------------
  const renderScrollBadge = (ctx: CanvasRenderingContext2D, s: any) => {
    const W = 160;
    const H = 128;

    // Pastel Pink Signature Background (#FFB6C1)
    ctx.fillStyle = '#FFB6C1';
    ctx.fillRect(0, 0, W, H);

    // Header Badge
    ctx.fillStyle = '#1A1A1A';
    ctx.font = 'bold 8px monospace';
    ctx.fillText('DESK BUGGY • 50 FPS', 12, 18);

    ctx.strokeStyle = '#1A1A1A';
    ctx.lineWidth = 1.5;
    ctx.strokeRect(122, 10, 26, 10);
    ctx.fillRect(124, 12, 18, 6);

    // Marquee Scrolling Text: :( hangry
    s.marqueeOffset -= 1.8;
    if (s.marqueeOffset < -180) s.marqueeOffset = W;

    ctx.fillStyle = '#111111';
    ctx.font = 'bold 24px monospace';
    ctx.fillText(':( hangry', s.marqueeOffset, 68);

    // Footer Subtitle Pill
    ctx.fillStyle = '#111111';
    ctx.fillRect(12, 94, 136, 22);
    ctx.fillStyle = '#FFFFFF';
    ctx.font = '8px monospace';
    ctx.fillText('ST7735 DOUBLE-BUFFERED', 18, 108);
  };

  // --------------------------------------------------------------------------
  // 9. SMART WEATHER OLED (SSD1306 128x64 OLED)
  // --------------------------------------------------------------------------
  const renderOledWeather = (ctx: CanvasRenderingContext2D, time: number) => {
    const W = 128;
    const H = 64;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, W, H);

    // Celestial Sky Arc
    ctx.strokeStyle = '#FFFFFF';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(64, 46, 36, Math.PI, 0);
    ctx.stroke();

    // Sun traversing the sky
    const sunAngle = Math.PI - (time * 0.4 % Math.PI);
    const sunX = 64 + Math.cos(sunAngle) * 36;
    const sunY = 46 - Math.sin(sunAngle) * 36;
    ctx.fillStyle = '#FFFFFF';
    ctx.beginPath();
    ctx.arc(sunX, sunY, 3, 0, Math.PI * 2);
    ctx.fill();

    // Weather Metrics
    ctx.font = 'bold 16px monospace';
    ctx.fillText('28°C', 10, 32);

    ctx.font = '6px monospace';
    ctx.fillText('PUNE, INDIA', 10, 44);
    ctx.fillText('HUMIDITY 54%', 10, 56);
    ctx.fillText('WIND 12 KM/H', 72, 56);
  };

  // --------------------------------------------------------------------------
  // 10. COLOR TFT WEATHER STATION (ST7735 160x128 TFT)
  // --------------------------------------------------------------------------
  const renderTftWeather = (ctx: CanvasRenderingContext2D, time: number) => {
    const W = 160;
    const H = 128;

    // Sky gradient
    ctx.fillStyle = '#1E3A8A';
    ctx.fillRect(0, 0, W, H);

    // Cloud & Sun
    ctx.fillStyle = '#F59E0B';
    ctx.beginPath();
    ctx.arc(120, 36, 16, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#E0F2FE';
    ctx.beginPath();
    ctx.arc(44, 40, 16, 0, Math.PI * 2);
    ctx.arc(66, 34, 20, 0, Math.PI * 2);
    ctx.arc(88, 40, 14, 0, Math.PI * 2);
    ctx.fill();

    // Big Temperature
    ctx.fillStyle = '#FFFFFF';
    ctx.font = 'bold 28px monospace';
    ctx.fillText('28°C', 20, 84);

    ctx.font = '10px monospace';
    ctx.fillText('PUNE, MH • SUNNY', 20, 102);

    // Bottom Stats Bar
    ctx.fillStyle = 'rgba(0, 0, 0, 0.4)';
    ctx.fillRect(10, 110, 140, 14);
    ctx.fillStyle = '#38BDF8';
    ctx.font = '7px monospace';
    ctx.fillText('HUMIDITY 54%  |  BARO 1013 hPa', 16, 120);
  };

  // --------------------------------------------------------------------------
  // 11. ANIME WATCH HUD (ST7735 160x128 TFT)
  // --------------------------------------------------------------------------
  const renderAnimeWatch = (ctx: CanvasRenderingContext2D, time: number) => {
    const W = 160;
    const H = 128;

    ctx.fillStyle = '#1A102F';
    ctx.fillRect(0, 0, W, H);

    // Pixel Avatar breathing
    const breath = Math.sin(time * 3) * 2;
    // Avatar Silhouette Box
    ctx.fillStyle = '#F472B6';
    ctx.fillRect(16, 24 + breath, 52, 64);
    // Face & Eyes
    ctx.fillStyle = '#FFF1F2';
    ctx.fillRect(24, 36 + breath, 36, 36);
    ctx.fillStyle = '#831843';
    ctx.fillRect(30, 48 + breath, 6, 8);
    ctx.fillRect(46, 48 + breath, 6, 8);
    // Blush
    ctx.fillStyle = '#FB7185';
    ctx.fillRect(28, 58 + breath, 8, 3);
    ctx.fillRect(46, 58 + breath, 8, 3);

    // Right Telemetry HUD
    ctx.fillStyle = '#A855F7';
    ctx.font = 'bold 8px monospace';
    ctx.fillText('TELEMETRY HUD', 78, 24);

    const now = new Date();
    const timeStr = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
    ctx.fillStyle = '#FFFFFF';
    ctx.font = 'bold 18px monospace';
    ctx.fillText(timeStr, 78, 48);

    ctx.fillStyle = '#C084FC';
    ctx.font = '7px monospace';
    ctx.fillText('GITHUB: @joyboy', 78, 64);
    ctx.fillText('STREAK: 14 DAYS', 78, 76);
    ctx.fillText('STATUS: CODING', 78, 88);

    // Battery Bar
    ctx.strokeStyle = '#C084FC';
    ctx.strokeRect(78, 100, 68, 10);
    ctx.fillStyle = '#10B981';
    ctx.fillRect(80, 102, 54, 6);
  };

  // --------------------------------------------------------------------------
  // 12. MACBOOK FACETIME CAM HUD (ST7735 160x128 TFT)
  // --------------------------------------------------------------------------
  const renderMacCamHUD = (ctx: CanvasRenderingContext2D, time: number) => {
    const W = 160;
    const H = 128;

    ctx.fillStyle = '#0F172A';
    ctx.fillRect(0, 0, W, H);

    // Crosshairs & Target Grid
    ctx.strokeStyle = '#38BDF8';
    ctx.lineWidth = 1;
    // Central Box
    ctx.strokeRect(36, 24, 88, 72);
    // Corner marks
    ctx.beginPath();
    ctx.moveTo(36, 34); ctx.lineTo(36, 24); ctx.lineTo(46, 24);
    ctx.moveTo(114, 24); ctx.lineTo(124, 24); ctx.lineTo(124, 34);
    ctx.moveTo(36, 86); ctx.lineTo(36, 96); ctx.lineTo(46, 96);
    ctx.moveTo(114, 96); ctx.lineTo(124, 96); ctx.lineTo(124, 86);
    ctx.stroke();

    // Center Cross
    ctx.beginPath();
    ctx.moveTo(80, 56); ctx.lineTo(80, 64);
    ctx.moveTo(76, 60); ctx.lineTo(84, 60);
    ctx.stroke();

    // Stream telemetry
    ctx.fillStyle = '#E01A22';
    ctx.beginPath();
    ctx.arc(12, 12, 3, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#FFFFFF';
    ctx.font = '6px monospace';
    ctx.fillText('LIVE UART 921600 BAUD', 20, 14);
    ctx.fillText('RES: 160x120 RGB565', 20, 118);
  };

  const renderDefaultFallback = (ctx: CanvasRenderingContext2D, title: string) => {
    ctx.fillStyle = '#111';
    ctx.fillRect(0, 0, preset.width, preset.height);
    ctx.fillStyle = '#FFF';
    ctx.font = '8px monospace';
    ctx.fillText(title, 10, 30);
  };

  return (
    <div className="flex flex-col items-center w-full bg-[#0d0d0d] border border-[#222] rounded-lg font-mono text-xs">
      {/* Top Spec Bar */}
      <div className="flex flex-wrap items-center justify-between w-full p-3 border-b border-[#222] bg-[#111] gap-2">
        <div className="flex items-center gap-2">
          <span className="text-white font-bold">{preset.title}</span>
          <span className="text-[10px] text-neutral-500">[{preset.width}x{preset.height} • {preset.chip}]</span>
        </div>

        <div className="flex items-center gap-2 text-[11px]">
          {/* Zoom */}
          <div className="flex items-center bg-[#1a1a1a] border border-[#333] rounded">
            {[2, 3, 4].map((z) => (
              <button
                key={z}
                onClick={() => setScale(z)}
                className={`px-2 py-0.5 rounded text-[11px] ${
                  scale === z ? 'bg-white text-black font-bold' : 'text-neutral-400 hover:text-white'
                }`}
              >
                {z}x
              </button>
            ))}
          </div>

          {/* Shader */}
          <div className="flex items-center bg-[#1a1a1a] border border-[#333] rounded">
            <button
              onClick={() => setShaderMode('matrix')}
              className={`px-2 py-0.5 text-[11px] rounded ${
                shaderMode === 'matrix' ? 'bg-white text-black font-bold' : 'text-neutral-400 hover:text-white'
              }`}
            >
              Grid
            </button>
            <button
              onClick={() => setShaderMode('pixel')}
              className={`px-2 py-0.5 text-[11px] rounded ${
                shaderMode === 'pixel' ? 'bg-white text-black font-bold' : 'text-neutral-400 hover:text-white'
              }`}
            >
              Clean
            </button>
          </div>

          {/* Pause / Play */}
          <button
            onClick={() => setIsPlaying(!isPlaying)}
            className="px-2 py-0.5 bg-[#1a1a1a] border border-[#333] text-neutral-300 hover:text-white rounded text-[11px]"
          >
            {isPlaying ? 'Pause' : 'Play'}
          </button>
        </div>
      </div>

      {/* 1:1 Hardware Display Canvas */}
      <div className="relative my-4 flex items-center justify-center p-4 sm:p-6 bg-[#080808] border border-[#1a1a1a] rounded">
        <div className="relative p-4 rounded bg-[#111] border border-[#2a2a2a]">
          <div className="flex justify-between text-[9px] text-neutral-500 uppercase tracking-widest pb-1.5 px-0.5">
            <span>{preset.category === 'tft' ? 'ST7735 SPI 160x128' : 'SSD1306 I2C 128x64'}</span>
            <span>{fps} FPS</span>
          </div>

          {/* Screen Glass */}
          <div
            className="relative overflow-hidden rounded border-2 border-black bg-black"
            style={{
              width: preset.width * scale,
              height: preset.height * scale,
            }}
          >
            <canvas
              ref={canvasRef}
              width={preset.width}
              height={preset.height}
              className="w-full h-full block"
              style={{
                imageRendering: 'pixelated',
              }}
            />

            {shaderMode === 'matrix' && (
              <div
                className="absolute inset-0 pointer-events-none opacity-20"
                style={{
                  backgroundImage:
                    'linear-gradient(rgba(0, 0, 0, 0) 50%, rgba(0, 0, 0, 0.4) 50%), linear-gradient(90deg, rgba(255, 0, 0, 0.04), rgba(0, 255, 0, 0.02), rgba(0, 0, 255, 0.04))',
                  backgroundSize: '100% 2px, 3px 100%',
                }}
              />
            )}
          </div>

          <div className="mt-2 flex justify-between text-[8px] text-neutral-600 px-1">
            {preset.category === 'tft' ? (
              <span>VCC • GND • CS:5 • RST:4 • A0:2 • SDA:23 • SCK:18 • BL:32</span>
            ) : (
              <span>GND • VCC:3.3V • SCL:GPIO9 • SDA:GPIO8</span>
            )}
          </div>
        </div>
      </div>

      {/* Live Controls */}
      <div className="w-full p-2.5 border-t border-[#222] bg-[#0c0c0c] flex flex-wrap items-center justify-between gap-2">
        <div className="flex items-center gap-2">
          {preset.id === 'esp32-nothing-hud' && (
            <div className="flex items-center gap-2 text-[11px]">
              <span className="text-neutral-400">Backlight:</span>
              <input
                type="range"
                min="10"
                max="255"
                value={brightness}
                onChange={(e) => {
                  const val = Number(e.target.value);
                  setBrightness(val);
                  if (onSendCommand) onSendCommand(String(Math.floor((val / 255) * 9)));
                }}
                className="w-20 h-1 bg-[#252525] rounded appearance-none cursor-pointer accent-white"
              />
              <span className="text-neutral-200">{Math.round((brightness * 100) / 255)}%</span>
            </div>
          )}

          {preset.id === 'oled-doom' && (
            <div className="flex items-center gap-1.5">
              <button
                onClick={fireDoomShotgun}
                className="px-2.5 py-1 bg-red-700 hover:bg-red-600 text-white rounded text-[11px] font-bold"
              >
                FIRE SHOTGUN (F)
              </button>
              <button
                onClick={() => setDoomShowMap(!doomShowMap)}
                className="px-2 py-1 bg-[#1c1c1c] hover:bg-[#282828] text-neutral-300 rounded text-[11px]"
              >
                {doomShowMap ? 'Hide Radar' : 'Radar (M)'}
              </button>
              <button
                onClick={() => setDoomState(doomState === 'raycaster' ? 'fire' : 'raycaster')}
                className="px-2 py-1 bg-[#1c1c1c] hover:bg-[#282828] text-neutral-300 rounded text-[11px]"
              >
                {doomState === 'raycaster' ? 'DOOM Fire' : '3D Raycaster'}
              </button>
            </div>
          )}

          {preset.id === 'oled-mac-buddy' && (
            <div className="flex items-center gap-1">
              {[
                { id: 1, label: '"hello."' },
                { id: 2, label: 'Desktop' },
                { id: 3, label: 'Happy Mac' },
                { id: 4, label: 'Sad Mac' },
              ].map((m) => (
                <button
                  key={m.id}
                  onClick={() => setMacMood(m.id)}
                  className={`px-2 py-0.5 rounded text-[11px] ${
                    macMood === m.id ? 'bg-white text-black font-bold' : 'bg-[#1a1a1a] text-neutral-400'
                  }`}
                >
                  {m.label}
                </button>
              ))}
            </div>
          )}

          {preset.id === 'oled-desk-buggy' && (
            <button
              onClick={() => setBaymaxLaser(!baymaxLaser)}
              className={`px-2.5 py-1 rounded text-[11px] ${
                baymaxLaser ? 'bg-emerald-600 text-white font-bold' : 'bg-[#1c1c1c] text-neutral-300'
              }`}
            >
              {baymaxLaser ? 'Laser Active' : 'Health Scan (S)'}
            </button>
          )}

          {preset.id === 'oled-pikachu' && (
            <button
              onClick={() => setPikaAwake(!pikaAwake)}
              className="px-2.5 py-1 bg-amber-600 hover:bg-amber-500 text-black font-bold rounded text-[11px]"
            >
              {pikaAwake ? 'Sleep (S)' : 'Wake (W)'}
            </button>
          )}
        </div>

        <div className="flex items-center gap-1">
          {preset.commands.map((cmd) => (
            <button
              key={cmd.key}
              onClick={() => {
                if (onSendCommand) onSendCommand(cmd.action);
              }}
              className="px-2 py-0.5 bg-[#181818] hover:bg-[#242424] text-neutral-400 hover:text-white rounded border border-[#2b2b2b] text-[10px]"
            >
              {cmd.label}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
};
