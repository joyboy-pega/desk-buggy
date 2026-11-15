'use client';

import React, { useState } from 'react';
import { FIRMWARE_PRESETS, FirmwarePreset } from '../lib/firmwarePresets';
import { HardwareScreenSimulator } from '../components/HardwareScreenSimulator';
import { WebHardwareFlasher } from '../components/WebHardwareFlasher';
import { HardwarePinouts } from '../components/HardwarePinouts';

export default function HomePage() {
  const [selectedPreset, setSelectedPreset] = useState<FirmwarePreset>(FIRMWARE_PRESETS[0]);
  const [activeTab, setActiveTab] = useState<'studio' | 'simulator' | 'flasher' | 'pinouts'>('studio');
  const [filterCategory, setFilterCategory] = useState<'all' | 'oled' | 'tft'>('all');

  const filteredPresets = FIRMWARE_PRESETS.filter((p) => {
    if (filterCategory === 'all') return true;
    return p.category === filterCategory;
  });

  return (
    <main className="min-h-screen bg-[#070707] text-gray-200 font-mono text-xs flex flex-col">
      {/* Minimal Top Header */}
      <header className="border-b border-[#222] bg-[#0d0d0d] px-4 py-2.5 flex flex-wrap items-center justify-between gap-3">
        <div className="flex items-center gap-3">
          <span className="font-bold text-white tracking-widest uppercase">
            DESK BUGGY // HARDWARE STUDIO
          </span>
          <span className="text-neutral-500 text-[10px]">
            ESP32 & ESP32-C3 WebSerial
          </span>
        </div>

        {/* View Switcher */}
        <div className="flex items-center bg-[#141414] border border-[#2a2a2a] rounded p-0.5">
          {[
            { id: 'studio', label: 'Workbench' },
            { id: 'simulator', label: 'Screen Replica' },
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

      {/* Preset Selector */}
      <div className="border-b border-[#1c1c1c] bg-[#0a0a0a] px-4 py-2.5 flex flex-col gap-2">
        <div className="flex items-center justify-between">
          <span className="text-[10px] text-neutral-500 uppercase tracking-wider">
            Firmware Target: <span className="text-white font-bold">{selectedPreset.title}</span>
          </span>

          <div className="flex items-center gap-1 text-[10px]">
            <button
              onClick={() => setFilterCategory('all')}
              className={`px-2 py-0.5 rounded ${filterCategory === 'all' ? 'bg-[#222] text-white' : 'text-neutral-500'}`}
            >
              All
            </button>
            <button
              onClick={() => setFilterCategory('oled')}
              className={`px-2 py-0.5 rounded ${filterCategory === 'oled' ? 'bg-[#222] text-white' : 'text-neutral-500'}`}
            >
              0.96" OLED
            </button>
            <button
              onClick={() => setFilterCategory('tft')}
              className={`px-2 py-0.5 rounded ${filterCategory === 'tft' ? 'bg-[#222] text-white' : 'text-neutral-500'}`}
            >
              1.8" TFT
            </button>
          </div>
        </div>

        {/* Preset Horizontal Strip */}
        <div className="flex items-center gap-2 overflow-x-auto pb-1 scrollbar-none">
          {filteredPresets.map((preset) => {
            const isSelected = selectedPreset.id === preset.id;
            return (
              <button
                key={preset.id}
                onClick={() => setSelectedPreset(preset)}
                className={`flex-shrink-0 px-3 py-1.5 rounded border text-left text-[11px] transition ${
                  isSelected
                    ? 'bg-[#181818] border-white text-white font-bold'
                    : 'bg-[#0f0f0f] border-[#202020] text-neutral-400 hover:text-neutral-200 hover:border-[#333]'
                }`}
              >
                <div>{preset.title}</div>
                <div className="text-[9px] text-neutral-500">
                  {preset.chip} • {preset.width}x{preset.height}
                </div>
              </button>
            );
          })}
        </div>
      </div>

      {/* Workspace */}
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

      {/* Clean Industrial Footer */}
      <footer className="border-t border-[#1a1a1a] px-4 py-2.5 bg-[#080808] text-[10px] text-neutral-600 flex justify-between">
        <span>Cloudflare Pages Static Export</span>
        <span>Target: ESP32 / ESP32-C3 SuperMini</span>
      </footer>
    </main>
  );
}
