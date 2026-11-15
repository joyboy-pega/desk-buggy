'use client';

import React, { useState, useRef } from 'react';
import { FIRMWARE_PRESETS, FirmwarePreset } from '../lib/firmwarePresets';
import { HardwareScreenSimulator } from '../components/HardwareScreenSimulator';
import { WebHardwareFlasher } from '../components/WebHardwareFlasher';
import { HardwarePinouts } from '../components/HardwarePinouts';
import { Cpu, Zap, Monitor, ArrowRight, X, ChevronDown } from 'lucide-react';

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

  return (
    <main className="min-h-screen bg-[#070707] text-gray-200 font-mono text-xs flex flex-col">
      {/* Minimal Top Header */}
      <header className="border-b border-[#222] bg-[#0d0d0d] px-4 sm:px-8 py-3 flex items-center justify-between">
        <div className="flex items-center gap-2.5">
          <div className="w-2.5 h-2.5 bg-white rounded-sm" />
          <span className="font-bold text-white tracking-widest uppercase text-sm">
            DESK BUGGY // HARDWARE STUDIO
          </span>
        </div>
        <div className="text-[11px] text-neutral-500">
          Cloudflare Pages • Web Serial API
        </div>
      </header>

      {/* 1. HERO SECTION */}
      <section className="border-b border-[#1f1f1f] bg-gradient-to-b from-[#0f0f0f] to-[#070707] px-4 sm:px-8 py-10">
        <div className="max-w-6xl mx-auto flex flex-col gap-4 text-center sm:text-left">
          <div className="flex flex-wrap items-center gap-2 justify-center sm:justify-start text-[10px] text-neutral-400">
            <span className="px-2 py-0.5 bg-[#1a1a1a] border border-[#2b2b2b] rounded text-white font-bold">
              DUAL-MCU SUITE
            </span>
            <span>ESP32 & ESP32-C3 SUPERMINI</span>
            <span>•</span>
            <span>1:1 PIXEL HARDWARE EMULATOR</span>
            <span>•</span>
            <span>IN-BROWSER ROM FLASHER</span>
          </div>

          <h1 className="text-2xl sm:text-3xl font-black text-white tracking-tight uppercase">
            Smart Desktop Companion & Retro Display Ecosystem
          </h1>

          <p className="text-neutral-400 text-xs sm:text-sm max-w-3xl leading-relaxed">
            Select your hardware device below. Click any firmware project to inspect its 1:1 pixel-perfect interactive screen replica and flash the compiled binary directly to your board over Web Serial.
          </p>
        </div>
      </section>

      {/* 2. TWO DEVICE BOXES (ESP32 TFT vs ESP32-C3 OLED) */}
      <section className="px-4 sm:px-8 py-8 max-w-6xl mx-auto w-full flex flex-col gap-6">
        <div className="flex items-center justify-between">
          <span className="text-[11px] text-neutral-400 font-bold uppercase tracking-wider">
            Hardware Platforms: Select Device & Project
          </span>
          <span className="text-[10px] text-neutral-600">
            {FIRMWARE_PRESETS.length} Total Firmware Images
          </span>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 items-stretch">
          {/* ======================================================== */}
          {/* BOX 1: ESP32 + 1.8" ST7735 COLOR TFT                    */}
          {/* ======================================================== */}
          <div className="bg-[#0c0c0c] border border-[#222] rounded-lg p-5 flex flex-col justify-between hover:border-[#383838] transition">
            <div className="flex flex-col gap-3">
              {/* Box 1 Header */}
              <div className="flex items-start justify-between border-b border-[#1f1f1f] pb-3">
                <div>
                  <div className="flex items-center gap-2">
                    <span className="w-2.5 h-2.5 rounded-full bg-red-500 inline-block" />
                    <h2 className="text-sm font-bold text-white uppercase tracking-wider">
                      ESP32 + 1.8" ST7735 TFT
                    </h2>
                  </div>
                  <p className="text-[11px] text-neutral-500 mt-0.5">
                    160×128 • 16-bit RGB565 • SPI 27MHz Bus
                  </p>
                </div>
                <span className="px-2 py-0.5 bg-[#181818] border border-[#292929] text-[10px] text-neutral-300 rounded font-bold">
                  {tftPresets.length} Projects
                </span>
              </div>

              {/* Hardware Pinout Specs */}
              <div className="p-2 bg-[#121212] border border-[#1c1c1c] rounded text-[10px] text-neutral-400 flex flex-wrap gap-x-3 gap-y-1">
                <span>MCU: <strong className="text-white">ESP32-WROOM-32</strong></span>
                <span>Pins: <strong className="text-white">CS:5, DC:2, RST:4, MOSI:23, SCK:18, BL:32</strong></span>
              </div>

              {/* Project Cards List */}
              <div className="flex flex-col gap-2 mt-2">
                {tftPresets.map((preset) => {
                  const isSelected = selectedPreset?.id === preset.id;
                  return (
                    <button
                      key={preset.id}
                      onClick={() => handleSelectProject(preset)}
                      className={`p-3 rounded border text-left transition flex items-center justify-between group ${
                        isSelected
                          ? 'bg-[#181818] border-red-500 text-white font-bold'
                          : 'bg-[#101010] border-[#1e1e1e] hover:border-[#333] text-neutral-300'
                      }`}
                    >
                      <div className="flex flex-col gap-0.5 pr-2">
                        <span className="text-xs text-white font-medium group-hover:text-red-400 transition">
                          {preset.title}
                        </span>
                        <span className="text-[10px] text-neutral-500">
                          {preset.subtitle}
                        </span>
                      </div>

                      <div className="flex items-center gap-2 shrink-0">
                        <span className="text-[9px] px-1.5 py-0.5 bg-[#161616] border border-[#262626] rounded text-neutral-400 font-mono">
                          {preset.fps} FPS
                        </span>
                        <ArrowRight className={`w-3.5 h-3.5 ${isSelected ? 'text-red-400' : 'text-neutral-600 group-hover:text-white'} transition`} />
                      </div>
                    </button>
                  );
                })}
              </div>
            </div>

            <div className="mt-4 pt-3 border-t border-[#1a1a1a] text-[10px] text-neutral-500 flex justify-between">
              <span>SPI Hardware Acceleration</span>
              <span>Double-Buffered GFXcanvas16</span>
            </div>
          </div>

          {/* ======================================================== */}
          {/* BOX 2: ESP32-C3 SUPERMINI + 0.96" SSD1306 OLED          */}
          {/* ======================================================== */}
          <div className="bg-[#0c0c0c] border border-[#222] rounded-lg p-5 flex flex-col justify-between hover:border-[#383838] transition">
            <div className="flex flex-col gap-3">
              {/* Box 2 Header */}
              <div className="flex items-start justify-between border-b border-[#1f1f1f] pb-3">
                <div>
                  <div className="flex items-center gap-2">
                    <span className="w-2.5 h-2.5 rounded-full bg-cyan-400 inline-block" />
                    <h2 className="text-sm font-bold text-white uppercase tracking-wider">
                      ESP32-C3 + 0.96" OLED
                    </h2>
                  </div>
                  <p className="text-[11px] text-neutral-500 mt-0.5">
                    128×64 • Monochrome • I2C Fast Mode+ (1.0MHz)
                  </p>
                </div>
                <span className="px-2 py-0.5 bg-[#181818] border border-[#292929] text-[10px] text-neutral-300 rounded font-bold">
                  {oledPresets.length} Projects
                </span>
              </div>

              {/* Hardware Pinout Specs */}
              <div className="p-2 bg-[#121212] border border-[#1c1c1c] rounded text-[10px] text-neutral-400 flex flex-wrap gap-x-3 gap-y-1">
                <span>MCU: <strong className="text-white">ESP32-C3 SuperMini</strong></span>
                <span>Pins: <strong className="text-white">SDA: GPIO 8, SCL: GPIO 9 (3.3V/GND)</strong></span>
              </div>

              {/* Project Cards List */}
              <div className="flex flex-col gap-2 mt-2">
                {oledPresets.map((preset) => {
                  const isSelected = selectedPreset?.id === preset.id;
                  return (
                    <button
                      key={preset.id}
                      onClick={() => handleSelectProject(preset)}
                      className={`p-3 rounded border text-left transition flex items-center justify-between group ${
                        isSelected
                          ? 'bg-[#181818] border-cyan-400 text-white font-bold'
                          : 'bg-[#101010] border-[#1e1e1e] hover:border-[#333] text-neutral-300'
                      }`}
                    >
                      <div className="flex flex-col gap-0.5 pr-2">
                        <span className="text-xs text-white font-medium group-hover:text-cyan-300 transition">
                          {preset.title}
                        </span>
                        <span className="text-[10px] text-neutral-500">
                          {preset.subtitle}
                        </span>
                      </div>

                      <div className="flex items-center gap-2 shrink-0">
                        <span className="text-[9px] px-1.5 py-0.5 bg-[#161616] border border-[#262626] rounded text-neutral-400 font-mono">
                          {preset.fps} FPS
                        </span>
                        <ArrowRight className={`w-3.5 h-3.5 ${isSelected ? 'text-cyan-400' : 'text-neutral-600 group-hover:text-white'} transition`} />
                      </div>
                    </button>
                  );
                })}
              </div>
            </div>

            <div className="mt-4 pt-3 border-t border-[#1a1a1a] text-[10px] text-neutral-500 flex justify-between">
              <span>I2C 1.0 MHz Fast Mode+</span>
              <span>Ultra-Low Power ~15mA</span>
            </div>
          </div>
        </div>
      </section>

      {/* 3. WORKBENCH SECTION (REVEALED WHEN A PROJECT IS CLICKED) */}
      <div ref={workbenchRef}>
        {selectedPreset ? (
          <section className="border-t border-[#222] bg-[#090909] px-4 sm:px-8 py-8">
            <div className="max-w-6xl mx-auto flex flex-col gap-5">
              {/* Active Project Banner */}
              <div className="flex flex-wrap items-center justify-between p-3.5 bg-[#111] border border-[#262626] rounded-lg gap-3">
                <div className="flex items-center gap-3">
                  <div
                    className="w-3 h-3 rounded-full"
                    style={{ backgroundColor: selectedPreset.category === 'tft' ? '#ef4444' : '#06b6d4' }}
                  />
                  <div>
                    <div className="flex items-center gap-2">
                      <span className="text-[10px] text-neutral-500 uppercase tracking-widest">
                        ACTIVE TARGET
                      </span>
                      <span className="text-xs font-bold text-white">
                        {selectedPreset.title}
                      </span>
                    </div>
                    <p className="text-[11px] text-neutral-400">
                      {selectedPreset.display} • Chip: {selectedPreset.chip} • {selectedPreset.fps} FPS Target
                    </p>
                  </div>
                </div>

                <div className="flex items-center gap-2">
                  <button
                    onClick={() => {
                      window.scrollTo({ top: 0, behavior: 'smooth' });
                    }}
                    className="px-2.5 py-1 bg-[#1a1a1a] hover:bg-[#252525] border border-[#333] text-neutral-300 rounded text-[11px]"
                  >
                    Change Project ↑
                  </button>
                  <button
                    onClick={() => setSelectedPreset(null)}
                    className="p-1 text-neutral-500 hover:text-white rounded"
                    title="Close Workbench"
                  >
                    <X className="w-4 h-4" />
                  </button>
                </div>
              </div>

              {/* Split View: 1:1 Screen Replica on Left, Web Serial Flasher on Right */}
              <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 items-start">
                {/* 1:1 Screen Replica Simulator */}
                <div className="flex flex-col gap-2">
                  <div className="text-[10px] text-neutral-500 uppercase tracking-wider font-bold">
                    1:1 Pixel-Perfect Interactive Screen
                  </div>
                  <HardwareScreenSimulator preset={selectedPreset} />
                </div>

                {/* Web Serial Hardware Flasher */}
                <div className="flex flex-col gap-2">
                  <div className="text-[10px] text-neutral-500 uppercase tracking-wider font-bold">
                    Web Serial Hardware Flasher & Terminal
                  </div>
                  <WebHardwareFlasher preset={selectedPreset} />
                </div>
              </div>

              {/* Hardware Pinouts Matrix */}
              <div className="mt-4">
                <div className="text-[10px] text-neutral-500 uppercase tracking-wider font-bold mb-2">
                  Wiring Pinout Reference for {selectedPreset.chip}
                </div>
                <HardwarePinouts />
              </div>
            </div>
          </section>
        ) : (
          <div className="border-t border-[#1a1a1a] bg-[#090909] py-8 text-center text-neutral-500 text-xs">
            ▲ Click any project in the boxes above to launch the 1:1 screen replica and firmware flasher.
          </div>
        )}
      </div>

      {/* Footer */}
      <footer className="mt-auto border-t border-[#1a1a1a] px-4 sm:px-8 py-3 bg-[#080808] text-[10px] text-neutral-600 flex justify-between">
        <span>Cloudflare Pages Static Deployment Ready</span>
        <span>ESP32 (ST7735 TFT) & ESP32-C3 (SSD1306 OLED)</span>
      </footer>
    </main>
  );
}
