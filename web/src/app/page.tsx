'use client';

import React, { useState, useRef, useEffect } from 'react';
import { FIRMWARE_PRESETS, FirmwarePreset } from '../lib/firmwarePresets';
import { HardwareScreenSimulator } from '../components/HardwareScreenSimulator';
import { WebHardwareFlasher } from '../components/WebHardwareFlasher';
import { HardwarePinouts } from '../components/HardwarePinouts';
import { ArrowRight, X } from 'lucide-react';

export default function HomePage() {
  const [selectedPreset, setSelectedPreset] = useState<FirmwarePreset | null>(null);
  const workbenchRef = useRef<HTMLDivElement | null>(null);

  const tftPresets = FIRMWARE_PRESETS.filter((p) => p.category === 'tft');
  const oledPresets = FIRMWARE_PRESETS.filter((p) => p.category === 'oled');

  const handleSelectProject = (preset: FirmwarePreset) => {
    setSelectedPreset(preset);
    setTimeout(() => {
      workbenchRef.current?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    }, 50);
  };

  // Keyboard navigation
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Don't intercept if user is typing in input/textarea
      const tag = (e.target as HTMLElement)?.tagName?.toLowerCase();
      if (tag === 'input' || tag === 'textarea') return;

      if (e.key === 'Escape') {
        setSelectedPreset(null);
      } else if (e.key === 'ArrowDown' || e.key === 'j') {
        if (!selectedPreset) {
          handleSelectProject(FIRMWARE_PRESETS[0]);
        } else {
          const idx = FIRMWARE_PRESETS.findIndex((p) => p.id === selectedPreset.id);
          const next = FIRMWARE_PRESETS[(idx + 1) % FIRMWARE_PRESETS.length];
          handleSelectProject(next);
        }
      } else if (e.key === 'ArrowUp' || e.key === 'k') {
        if (!selectedPreset) {
          handleSelectProject(FIRMWARE_PRESETS[FIRMWARE_PRESETS.length - 1]);
        } else {
          const idx = FIRMWARE_PRESETS.findIndex((p) => p.id === selectedPreset.id);
          const prev = FIRMWARE_PRESETS[(idx - 1 + FIRMWARE_PRESETS.length) % FIRMWARE_PRESETS.length];
          handleSelectProject(prev);
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [selectedPreset]);

  return (
    <main className="min-h-screen bg-[#070707] text-gray-200 font-mono text-xs flex flex-col">

      {/* ── HEADER ─────────────────────────────────────────────────────── */}
      <header className="border-b border-[#222] bg-[#0a0a0a] px-4 sm:px-8 py-3 flex items-center justify-between shrink-0">
        <div className="flex items-center gap-2.5">
          <div className="w-2 h-2 bg-white rounded-sm" />
          <span className="font-bold text-white tracking-widest uppercase text-sm">
            DESK BUGGY
          </span>
          <span className="text-neutral-600 text-[11px]">// HARDWARE STUDIO</span>
        </div>
        <div className="flex items-center gap-3 text-[10px] text-neutral-600">
          <span>Web Serial API</span>
          <span className="w-px h-3 bg-[#333]" />
          <span>{FIRMWARE_PRESETS.length} firmware images</span>
        </div>
      </header>

      {/* ── HERO ───────────────────────────────────────────────────────── */}
      <section className="border-b border-[#1a1a1a] px-4 sm:px-8 py-8">
        <div className="max-w-6xl mx-auto flex flex-col gap-3">
          <div className="flex flex-wrap items-center gap-x-4 gap-y-1 text-[10px] text-neutral-500">
            <span className="text-white font-bold tracking-widest uppercase">DUAL-MCU SUITE</span>
            <span>ESP32 + ST7735 1.8" TFT</span>
            <span className="text-neutral-700">·</span>
            <span>ESP32-C3 + SSD1306 0.96" OLED</span>
            <span className="text-neutral-700">·</span>
            <span>1:1 pixel emulator</span>
            <span className="text-neutral-700">·</span>
            <span>in-browser ROM flasher</span>
          </div>
          <p className="text-neutral-500 text-[11px] max-w-2xl leading-relaxed">
            Pick a device below. Click any project to open its 1:1 screen replica and flash the binary to your board over Web Serial — no drivers, no IDE.
          </p>
        </div>
      </section>

      {/* ── DEVICE SELECTOR ────────────────────────────────────────────── */}
      <section className="px-4 sm:px-8 py-6 max-w-6xl mx-auto w-full flex flex-col gap-4">

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-5 items-stretch">

          {/* BOX 1 — ESP32 TFT */}
          <div className={`bg-[#0b0b0b] border rounded-lg p-4 flex flex-col gap-3 transition-colors ${
            selectedPreset?.category === 'tft' ? 'border-red-500/40' : 'border-[#1e1e1e] hover:border-[#2e2e2e]'
          }`}>
            {/* Device header */}
            <div className="flex items-start justify-between pb-3 border-b border-[#191919]">
              <div>
                <div className="flex items-center gap-2 mb-0.5">
                  <span className="w-2 h-2 rounded-full bg-red-500 shrink-0" />
                  <h2 className="text-[13px] font-bold text-white uppercase tracking-wider">
                    ESP32 + ST7735 TFT
                  </h2>
                </div>
                <p className="text-[10px] text-neutral-600 pl-4">160×128 · 16-bit RGB565 · SPI</p>
              </div>
              <span className="text-[10px] text-neutral-500 tabular-nums">{tftPresets.length} projects</span>
            </div>

            {/* Pin spec strip */}
            <div className="text-[10px] text-neutral-600 leading-relaxed">
              <span className="text-neutral-500">CS:</span>5&nbsp;
              <span className="text-neutral-500">DC:</span>2&nbsp;
              <span className="text-neutral-500">RST:</span>4&nbsp;
              <span className="text-neutral-500">MOSI:</span>23&nbsp;
              <span className="text-neutral-500">SCK:</span>18&nbsp;
              <span className="text-neutral-500">BL:</span>32
            </div>

            {/* Project list */}
            <div className="flex flex-col gap-1.5">
              {tftPresets.map((preset) => {
                const isSelected = selectedPreset?.id === preset.id;
                return (
                  <button
                    key={preset.id}
                    onClick={() => handleSelectProject(preset)}
                    className={`px-3 py-2.5 rounded border text-left flex items-center justify-between group transition-colors ${
                      isSelected
                        ? 'bg-[#1a0e0e] border-red-500/60'
                        : 'bg-[#0e0e0e] border-[#1c1c1c] hover:border-[#2c2c2c]'
                    }`}
                  >
                    <div className="flex flex-col gap-0.5 min-w-0 pr-2">
                      <span className={`text-[12px] font-medium truncate transition-colors ${
                        isSelected ? 'text-red-400' : 'text-neutral-200 group-hover:text-white'
                      }`}>
                        {preset.title}
                      </span>
                      <span className="text-[10px] text-neutral-600 truncate">{preset.subtitle}</span>
                    </div>
                    <div className="flex items-center gap-2 shrink-0">
                      <span className="text-[9px] text-neutral-600 tabular-nums">{preset.fps}fps</span>
                      <ArrowRight className={`w-3 h-3 transition-colors ${
                        isSelected ? 'text-red-500' : 'text-neutral-700 group-hover:text-neutral-400'
                      }`} />
                    </div>
                  </button>
                );
              })}
            </div>
          </div>

          {/* BOX 2 — ESP32-C3 OLED */}
          <div className={`bg-[#0b0b0b] border rounded-lg p-4 flex flex-col gap-3 transition-colors ${
            selectedPreset?.category === 'oled' ? 'border-cyan-500/40' : 'border-[#1e1e1e] hover:border-[#2e2e2e]'
          }`}>
            {/* Device header */}
            <div className="flex items-start justify-between pb-3 border-b border-[#191919]">
              <div>
                <div className="flex items-center gap-2 mb-0.5">
                  <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                  <h2 className="text-[13px] font-bold text-white uppercase tracking-wider">
                    ESP32-C3 + SSD1306 OLED
                  </h2>
                </div>
                <p className="text-[10px] text-neutral-600 pl-4">128×64 · Monochrome · I2C</p>
              </div>
              <span className="text-[10px] text-neutral-500 tabular-nums">{oledPresets.length} projects</span>
            </div>

            {/* Pin spec strip */}
            <div className="text-[10px] text-neutral-600 leading-relaxed">
              <span className="text-neutral-500">SDA:</span> GPIO 8&nbsp;&nbsp;
              <span className="text-neutral-500">SCL:</span> GPIO 9&nbsp;&nbsp;
              <span className="text-neutral-500">VCC:</span> 3.3V&nbsp;&nbsp;
              <span className="text-neutral-500">GND:</span> GND
            </div>

            {/* Project list */}
            <div className="flex flex-col gap-1.5">
              {oledPresets.map((preset) => {
                const isSelected = selectedPreset?.id === preset.id;
                return (
                  <button
                    key={preset.id}
                    onClick={() => handleSelectProject(preset)}
                    className={`px-3 py-2.5 rounded border text-left flex items-center justify-between group transition-colors ${
                      isSelected
                        ? 'bg-[#071416] border-cyan-500/60'
                        : 'bg-[#0e0e0e] border-[#1c1c1c] hover:border-[#2c2c2c]'
                    }`}
                  >
                    <div className="flex flex-col gap-0.5 min-w-0 pr-2">
                      <span className={`text-[12px] font-medium truncate transition-colors ${
                        isSelected ? 'text-cyan-400' : 'text-neutral-200 group-hover:text-white'
                      }`}>
                        {preset.title}
                      </span>
                      <span className="text-[10px] text-neutral-600 truncate">{preset.subtitle}</span>
                    </div>
                    <div className="flex items-center gap-2 shrink-0">
                      <span className="text-[9px] text-neutral-600 tabular-nums">{preset.fps}fps</span>
                      <ArrowRight className={`w-3 h-3 transition-colors ${
                        isSelected ? 'text-cyan-400' : 'text-neutral-700 group-hover:text-neutral-400'
                      }`} />
                    </div>
                  </button>
                );
              })}
            </div>
          </div>

        </div>
      </section>

      {/* ── WORKBENCH ──────────────────────────────────────────────────── */}
      <div ref={workbenchRef}>
        {selectedPreset ? (
          <section className="border-t border-[#1e1e1e] bg-[#080808] px-4 sm:px-8 py-6">
            <div className="max-w-6xl mx-auto flex flex-col gap-5">

              {/* Active target bar */}
              <div className="flex items-center justify-between px-3 py-2 bg-[#0f0f0f] border border-[#242424] rounded">
                <div className="flex items-center gap-2.5 min-w-0">
                  <span
                    className="w-2 h-2 rounded-full shrink-0"
                    style={{ backgroundColor: selectedPreset.category === 'tft' ? '#ef4444' : '#06b6d4' }}
                  />
                  <span className="text-[10px] text-neutral-500 uppercase tracking-widest shrink-0">TARGET</span>
                  <span className="text-[12px] font-bold text-white truncate">{selectedPreset.title}</span>
                  <span className="text-[10px] text-neutral-600 shrink-0 hidden sm:inline">
                    {selectedPreset.chip} · {selectedPreset.display} · {selectedPreset.fps}fps
                  </span>
                </div>
                <div className="flex items-center gap-3">
                  <span className="text-[10px] text-neutral-600 hidden md:inline">
                    <kbd className="px-1 py-0.5 bg-[#181818] border border-[#2c2c2c] rounded text-neutral-400">↑/↓</kbd> switch · <kbd className="px-1 py-0.5 bg-[#181818] border border-[#2c2c2c] rounded text-neutral-400">ESC</kbd> close
                  </span>
                  <button
                    onClick={() => setSelectedPreset(null)}
                    className="p-1 text-neutral-600 hover:text-neutral-200 rounded transition-colors shrink-0"
                    title="Close workbench (ESC)"
                  >
                    <X className="w-3.5 h-3.5" />
                  </button>
                </div>
              </div>

              {/* Simulator + Flasher side-by-side */}
              <div className="grid grid-cols-1 lg:grid-cols-2 gap-5 items-start">
                <div className="flex flex-col gap-2">
                  <div className="text-[10px] text-neutral-600 uppercase tracking-wider">1:1 Screen</div>
                  <HardwareScreenSimulator preset={selectedPreset} />
                </div>
                <div className="flex flex-col gap-2">
                  <div className="text-[10px] text-neutral-600 uppercase tracking-wider">Flasher / Terminal</div>
                  <WebHardwareFlasher preset={selectedPreset} />
                </div>
              </div>

              {/* Pinouts */}
              <div className="flex flex-col gap-2">
                <div className="text-[10px] text-neutral-600 uppercase tracking-wider">
                  Pinout — {selectedPreset.chip}
                </div>
                <HardwarePinouts />
              </div>

            </div>
          </section>
        ) : (
          <div className="border-t border-[#141414] py-6 text-center text-[11px] text-neutral-700">
            ↑ click a project to open workbench
          </div>
        )}
      </div>

      {/* ── FOOTER ─────────────────────────────────────────────────────── */}
      <footer className="mt-auto border-t border-[#141414] px-4 sm:px-8 py-2.5 bg-[#070707] text-[10px] text-neutral-700 flex justify-between">
        <span>desk-buggy · joyboy-pega/desk-buggy</span>
        <span>ESP32 ST7735 · ESP32-C3 SSD1306</span>
      </footer>

    </main>
  );
}
