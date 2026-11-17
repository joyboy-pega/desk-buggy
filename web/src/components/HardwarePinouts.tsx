'use client';

import React, { useState } from 'react';

export const HardwarePinouts: React.FC = () => {
  const [boardType, setBoardType] = useState<'esp32-tft' | 'esp32c3-oled' | 'esp32-joystick'>('esp32-joystick');

  return (
    <div className="flex flex-col w-full bg-[#0d0d0d] border border-[#222] rounded-lg font-mono text-xs">
      <div className="flex flex-wrap items-center justify-between p-3 border-b border-[#222] bg-[#111] gap-2">
        <span className="text-white font-bold">HARDWARE WIRING MATRIX</span>

        <div className="flex items-center bg-[#1a1a1a] border border-[#333] rounded">
          <button
            onClick={() => setBoardType('esp32-joystick')}
            className={`px-2.5 py-1 text-[11px] rounded ${
              boardType === 'esp32-joystick' ? 'bg-[#00f0ff] text-black font-bold' : 'text-neutral-400 hover:text-white'
            }`}
          >
            🕹️ ESP32 + Joystick
          </button>
          <button
            onClick={() => setBoardType('esp32-tft')}
            className={`px-2.5 py-1 text-[11px] rounded ${
              boardType === 'esp32-tft' ? 'bg-white text-black font-bold' : 'text-neutral-400 hover:text-white'
            }`}
          >
            ESP32 + ST7735 TFT
          </button>
          <button
            onClick={() => setBoardType('esp32c3-oled')}
            className={`px-2.5 py-1 text-[11px] rounded ${
              boardType === 'esp32c3-oled' ? 'bg-white text-black font-bold' : 'text-neutral-400 hover:text-white'
            }`}
          >
            ESP32-C3 + SSD1306 OLED
          </button>
        </div>
      </div>

      <div className="p-3">
        {boardType === 'esp32-joystick' ? (
          <table className="w-full text-left text-[11px] border border-[#222]">
            <thead className="bg-[#141414] text-neutral-400 border-b border-[#222]">
              <tr>
                <th className="p-2 text-[#00f0ff]">Joystick Module Pin</th>
                <th className="p-2 text-white">ESP32 Pin</th>
                <th className="p-2">Signal Type</th>
                <th className="p-2">Function & Notes</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-[#1c1c1c] bg-[#0c0c0c] text-neutral-300">
              <tr>
                <td className="p-2 font-bold text-[#00f0ff]">VRx (X-Axis)</td>
                <td className="p-2 font-bold text-white">GPIO 34</td>
                <td className="p-2 text-amber-400">Analog (ADC1_CH6)</td>
                <td className="p-2 text-neutral-400">12-bit ADC (0 - 4095), center ~2048. Wi-Fi safe (ADC1)</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-[#00f0ff]">VRy (Y-Axis)</td>
                <td className="p-2 font-bold text-white">GPIO 35</td>
                <td className="p-2 text-amber-400">Analog (ADC1_CH7)</td>
                <td className="p-2 text-neutral-400">12-bit ADC (0 - 4095), center ~2048. Wi-Fi safe (ADC1)</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-pink-400">SW (Push Button)</td>
                <td className="p-2 font-bold text-white">GPIO 25</td>
                <td className="p-2 text-green-400">Digital (INPUT_PULLUP)</td>
                <td className="p-2 text-neutral-400">Active LOW: Pressed = 0, Released = 1. Internal pullup enabled</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">+5V / VCC</td>
                <td className="p-2 font-bold text-white">3.3V</td>
                <td className="p-2 text-red-400">Power</td>
                <td className="p-2 text-neutral-400">Power from 3.3V rail to protect ESP32 3.3V ADC limit</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">GND</td>
                <td className="p-2 font-bold text-white">GND</td>
                <td className="p-2 text-blue-400">Ground</td>
                <td className="p-2 text-neutral-400">Common system ground</td>
              </tr>
            </tbody>
          </table>
        ) : boardType === 'esp32-tft' ? (
          <table className="w-full text-left text-[11px] border border-[#222]">
            <thead className="bg-[#141414] text-neutral-400 border-b border-[#222]">
              <tr>
                <th className="p-2 text-white">ST7735 Pin</th>
                <th className="p-2 text-white">ESP32 Pin</th>
                <th className="p-2">Function</th>
                <th className="p-2">Notes</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-[#1c1c1c] bg-[#0c0c0c] text-neutral-300">
              <tr>
                <td className="p-2 font-bold text-white">CS</td>
                <td className="p-2 text-white">GPIO 5</td>
                <td className="p-2">SPI Chip Select</td>
                <td className="p-2 text-neutral-500">VSPI CS</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">A0 / DC</td>
                <td className="p-2 text-white">GPIO 2</td>
                <td className="p-2">Data / Command</td>
                <td className="p-2 text-neutral-500">Command/Data toggle</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">RESET</td>
                <td className="p-2 text-white">GPIO 4</td>
                <td className="p-2">Hardware Reset</td>
                <td className="p-2 text-neutral-500">Pulsed low on setup</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">SDA / MOSI</td>
                <td className="p-2 text-white">GPIO 23</td>
                <td className="p-2">SPI MOSI</td>
                <td className="p-2 text-neutral-500">27 MHz SPI data</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">SCK</td>
                <td className="p-2 text-white">GPIO 18</td>
                <td className="p-2">SPI Clock</td>
                <td className="p-2 text-neutral-500">27 MHz SPI clock</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">BL / LED</td>
                <td className="p-2 text-white">GPIO 32</td>
                <td className="p-2">Backlight PWM</td>
                <td className="p-2 text-neutral-500">AnalogWrite 0-255</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">VCC</td>
                <td className="p-2 text-white">3.3V</td>
                <td className="p-2">Power</td>
                <td className="p-2 text-neutral-500">3.3V power rail</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">GND</td>
                <td className="p-2 text-white">GND</td>
                <td className="p-2">Ground</td>
                <td className="p-2 text-neutral-500">Common system GND</td>
              </tr>
            </tbody>
          </table>
        ) : (
          <table className="w-full text-left text-[11px] border border-[#222]">
            <thead className="bg-[#141414] text-neutral-400 border-b border-[#222]">
              <tr>
                <th className="p-2 text-white">SSD1306 Pin</th>
                <th className="p-2 text-white">ESP32-C3 SuperMini</th>
                <th className="p-2">Bus Function</th>
                <th className="p-2">Notes</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-[#1c1c1c] bg-[#0c0c0c] text-neutral-300">
              <tr>
                <td className="p-2 font-bold text-white">SDA</td>
                <td className="p-2 text-white">GPIO 8</td>
                <td className="p-2">I2C Data</td>
                <td className="p-2 text-neutral-500">Fast Mode+ (1.0 MHz)</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">SCL</td>
                <td className="p-2 text-white">GPIO 9</td>
                <td className="p-2">I2C Clock</td>
                <td className="p-2 text-neutral-500">Fast Mode+ (1.0 MHz)</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">VCC</td>
                <td className="p-2 text-white">3.3V</td>
                <td className="p-2">Power</td>
                <td className="p-2 text-neutral-500">Draws ~15-20 mA</td>
              </tr>
              <tr>
                <td className="p-2 font-bold text-white">GND</td>
                <td className="p-2 text-white">GND</td>
                <td className="p-2">Ground</td>
                <td className="p-2 text-neutral-500">Common system ground</td>
              </tr>
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
};
