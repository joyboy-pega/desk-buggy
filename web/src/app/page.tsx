'use client';

import React, { useState } from 'react';
import { FIRMWARE_PRESETS, FirmwarePreset } from '../lib/firmwarePresets';
import { HardwareScreenSimulator } from '../components/HardwareScreenSimulator';
import { WebHardwareFlasher } from '../components/WebHardwareFlasher';
import { HardwarePinouts } from '../components/HardwarePinouts';

export default function HomePage() {
  const [selectedPreset, setSelectedPreset] = useState<FirmwarePreset>(FIRMWARE_PRESETS[0]);
  const [activeTab, setActiveTab] = useState<'studio' | 'simulator' | 'flasher' | 'pinouts'>('studio');
  const [deviceFilter, setDeviceFilter] = useState<'both' | 'tft' | 'oled'>('both');

  const tftPresets = FIRMWARE_PRESETS.filter((p) => p.category === 'tft');
  const oledPresets = FIRMWARE_PRESETS.filter((p) => p.category === 'oled');

  return (
    <main className="min-h-screen bg-[#070707] text-gray-200 font-mono text-xs flex flex-col">
      {/* Top Header */}
      <header className="border-b border-[#222] bg-[#0d0d0d] px-4 py-2.5 flex flex-wrap items-center justify-between gap-3">
        <div className="flex items-center gap-3">
          <span className="font-bold text-white tracking-widest uppercase">
            DESK BUGGY // HARDWARE STUDIO
          </span>
          <span className="text-neutral-500 text-[10px]">
            ESP32 TFT & ESP32-C3 OLED
          </span>
        </div>

        {/* View Switcher */}
        <div className="flex items-center bg-[#141414] border border-[#2a2a2a] rounded p-0.5">
          {[
            { id: 'studio', label: 'Workbench' },
            { id: 'simulator', label: '1:1 Screen' },
            { id: 'flasher', label: 'Flasher' },
            { id: 'pinouts', label: 'Pinouts' },
          ].map((tab) => {
            const active = activeTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id as any)}
                className={`px-3 py-1 rounded text-[11px] transition ${
                  active
                    ? 'bg-white text-black font-bold'
                    : 'text-neutral-400 hover:text-white'
                }`}
              >
                {tab.label}
              </button>
            );
          })}
        </div>
      </header>

      {/* Device Filter Pills */}
      <div className="border-b border-[#1c1c1c] bg-[#0a0a0a] px-4 py-2 flex items-center justify-between">
        <div className="text-[10px] text-neutral-400">
          SELECT DEVICE SECTION & CLICK PROJECT TO VIEW 1:1 SCREEN:
        </div>

        <div className="flex items-center gap-1 text-[10px]">
          <button
            onClick={() => setDeviceFilter('both')}
            className={`px-2 py-0.5 rounded ${deviceFilter === 'both' ? 'bg-white text-black font-bold' : 'bg-[#181818] text-neutral-400'}`}
          >
            Show Both Devices
          </button>
          <button
            onClick={() => setDeviceFilter('tft')}
            className={`px-2 py-0.5 rounded ${deviceFilter === 'tft' ? 'bg-white text-black font-bold' : 'bg-[#181818] text-neutral-400'}`}
          >
            ESP32 TFT Only
          </button>
          <button
            onClick={() => setDeviceFilter('oled')}
            className={`px-2 py-0.5 rounded ${deviceFilter === 'oled' ? 'bg-white text-black font-bold' : 'bg-[#181818] text-neutral-400'}`}
          >
            ESP32-C3 OLED Only
          </button>
        </div>
      </div>

      {/* TWO HARDWARE SECTIONS */}
      <div className="border-b border-[#1c1c1c] bg-[#0c0c0c] px-4 py-3 flex flex-col gap-3">
        {/* SECTION 1: ESP32 + 1.8" ST7735 COLOR TFT */}
        {(deviceFilter === 'both' || deviceFilter === 'tft') && (
          <div className="flex flex-col gap-1.5 p-2.5 bg-[#0e0e0e] border border-[#1e1e1e] rounded">
            <div className="flex items-center justify-between text-[11px]">
              <div className="flex items-center gap-2">
                <span className="w-2 h-2 rounded-full bg-red-500 inline-block" />
                <span className="font-bold text-white uppercase tracking-wider">
                  DEVICE 1: ESP32 + 1.8" ST7735 COLOR TFT
                </span>
                <span className="text-[9px] text-neutral-500">
                  (160×128 • SPI 27MHz • GPIO 5,2,4,23,18,32)
                </span>
              </div>
              <span className="text-[10px] text-neutral-500">{tftPresets.length} Projects</span>
            </div>

            <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-5 gap-2">
              {tftPresets.map((preset) => {
                const isSelected = selectedPreset.id === preset.id;
                return (
                  <button
                    key={preset.id}
                    onClick={() => setSelectedPreset(preset)}
                    className={`p-2 rounded border text-left text-[11px] transition ${
                      isSelected
                        ? 'bg-[#1f1f1f] border-red-500 text-white shadow font-bold'
                        : 'bg-[#121212] border-[#222] text-neutral-400 hover:text-neutral-200 hover:border-[#383838]'
                    }`}
                  >
                    <div className="truncate text-white font-medium">{preset.title}</div>
                    <div className="text-[9px] text-neutral-500 mt-0.5">
                      160×128 • {preset.fps} FPS
                    </div>
                  </button>
                );
              })}
            </div>
          </div>
        )}

        {/* SECTION 2: ESP32-C3 SUPERMINI + 0.96" SSD1306 OLED */}
        {(deviceFilter === 'both' || deviceFilter === 'oled') && (
          <div className="flex flex-col gap-1.5 p-2.5 bg-[#0e0e0e] border border-[#1e1e1e] rounded">
            <div className="flex items-center justify-between text-[11px]">
              <div className="flex items-center gap-2">
                <span className="w-2 h-2 rounded-full bg-cyan-400 inline-block" />
                <span className="font-bold text-white uppercase tracking-wider">
                  DEVICE 2: ESP32-C3 SUPERMINI + 0.96" SSD1306 OLED
                </span>
                <span className="text-[9px] text-neutral-500">
                  (128×64 • I2C Fast Mode+ 1.0MHz • SDA:8, SCL:9)
                </span>
              </div>
              <span className="text-[10px] text-neutral-500">{oledPresets.length} Projects</span>
            </div>

            <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-7 gap-2">
              {oledPresets.map((preset) => {
                const isSelected = selectedPreset.id === preset.id;
                return (
                  <button
                    key={preset.id}
                    onClick={() => setSelectedPreset(preset)}
                    className={`p-2 rounded border text-left text-[11px] transition ${
                      isSelected
                        ? 'bg-[#1f1f1f] border-cyan-400 text-white shadow font-bold'
                        : 'bg-[#121212] border-[#222] text-neutral-400 hover:text-neutral-200 hover:border-[#383838]'
                    }`}
                  >
                    <div className="truncate text-white font-medium">{preset.title}</div>
                    <div className="text-[9px] text-neutral-500 mt-0.5">
                      128×64 • {preset.fps} FPS
                    </div>
                  </button>
                );
              })}
            </div>
          </div>
        )}
      </div>

      {/* Active Project Banner */}
      <div className="bg-[#111] border-b border-[#1c1c1c] px-4 py-1.5 flex items-center justify-between text-[11px]">
        <div className="flex items-center gap-2">
          <span className="text-neutral-500">Active Viewing:</span>
          <span className="text-white font-bold">{selectedPreset.title}</span>
          <span className="text-neutral-500">[{selectedPreset.display}]</span>
        </div>
        <div className="text-neutral-500 text-[10px]">
          Target MCU: <span className="text-neutral-300 font-bold">{selectedPreset.chip}</span>
        </div>
      </div>

      {/* Main Interactive Workbench */}
      <section className="flex-1 p-4 max-w-7xl mx-auto w-full">
        {activeTab === 'studio' && (
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4 items-start">
            <HardwareScreenSimulator preset={selectedPreset} />
            <WebHardwareFlasher preset={selectedPreset} />
          </div>
        )}

        {activeTab === 'simulator' && (
          <div className="max-w-3xl mx-auto">
            <HardwareScreenSimulator preset={selectedPreset} />
          </div>
        )}

        {activeTab === 'flasher' && (
          <div className="max-w-2xl mx-auto">
            <WebHardwareFlasher preset={selectedPreset} />
          </div>
        )}

        {activeTab === 'pinouts' && (
          <div className="max-w-3xl mx-auto">
            <HardwarePinouts />
          </div>
        )}
      </section>

      {/* Footer */}
      <footer className="border-t border-[#1a1a1a] px-4 py-2.5 bg-[#080808] text-[10px] text-neutral-600 flex justify-between">
        <span>Cloudflare Pages Static Export</span>
        <span>Target: ESP32 & ESP32-C3 SuperMini</span>
      </footer>
    </main>
  );
}
