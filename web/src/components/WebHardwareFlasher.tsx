'use client';

import React, { useState, useEffect, useRef } from 'react';
import { FirmwarePreset } from '../lib/firmwarePresets';
import { EspWebFlasher, FlasherStatus } from '../lib/espFlasher';
import { Trash2 } from 'lucide-react';

interface WebHardwareFlasherProps {
  preset: FirmwarePreset;
}

export const WebHardwareFlasher: React.FC<WebHardwareFlasherProps> = ({ preset }) => {
  const [flasher, setFlasher] = useState<EspWebFlasher | null>(null);
  const [isSupported, setIsSupported] = useState<boolean>(true);
  const [status, setStatus] = useState<FlasherStatus>({
    state: 'idle',
    progress: 0,
    message: 'Connect ESP32 via USB-C to begin.',
  });

  const [connectedChip, setConnectedChip] = useState<string | null>(null);
  const [connectedMac, setConnectedMac] = useState<string | null>(null);
  const [baudRate, setBaudRate] = useState<number>(460800);
  const [eraseAll, setEraseAll] = useState<boolean>(false);
  const [isFlashing, setIsFlashing] = useState<boolean>(false);

  // Serial Monitor
  const [isMonitorActive, setIsMonitorActive] = useState<boolean>(false);
  const [serialLogs, setSerialLogs] = useState<{ text: string; type: string; time: string }[]>([]);
  const [commandInput, setCommandInput] = useState<string>('');
  const logContainerRef = useRef<HTMLDivElement | null>(null);

  // Custom File Upload
  const [activeTab, setActiveTab] = useState<'preset' | 'custom'>('preset');
  const [customFile, setCustomFile] = useState<{ name: string; buffer: ArrayBuffer } | null>(null);
  const [customOffset, setCustomOffset] = useState<string>('0x10000');

  useEffect(() => {
    setIsSupported(EspWebFlasher.isSupported());

    const inst = new EspWebFlasher(
      (text, type = 'info') => {
        const time = new Date().toLocaleTimeString();
        setSerialLogs((prev) => [...prev.slice(-300), { text, type, time }]);
      },
      (newStatus) => {
        setStatus(newStatus);
        if (newStatus.chipName) setConnectedChip(newStatus.chipName);
        if (newStatus.macAddr) setConnectedMac(newStatus.macAddr);
      }
    );

    setFlasher(inst);

    return () => {
      inst.disconnect();
    };
  }, []);

  useEffect(() => {
    if (logContainerRef.current) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  }, [serialLogs]);

  const handleConnect = async () => {
    if (!flasher) return;
    const res = await flasher.connectBootloader(baudRate);
    if (res) {
      setConnectedChip(res.chipName);
      setConnectedMac(res.mac);
    }
  };

  const handleFlashPreset = async () => {
    if (!flasher || isFlashing) return;
    setIsFlashing(true);

    try {
      const fileBuffers: { data: ArrayBuffer; address: number; name: string }[] = [];

      for (const f of preset.files) {
        setStatus({
          state: 'flashing',
          progress: 5,
          message: `Reading ${f.name}...`,
        });

        const resp = await fetch(f.url);
        if (!resp.ok) {
          throw new Error(`Binary ${f.name} not found`);
        }
        const buf = await resp.arrayBuffer();
        fileBuffers.push({
          data: buf,
          address: f.offset,
          name: f.name,
        });
      }

      await flasher.flashFiles(fileBuffers, eraseAll);
    } catch (err: any) {
      setStatus({
        state: 'error',
        progress: 0,
        message: err.message || 'Flashing error',
      });
    } finally {
      setIsFlashing(false);
    }
  };

  const handleFlashCustom = async () => {
    if (!flasher || !customFile || isFlashing) return;
    setIsFlashing(true);

    try {
      const offset = parseInt(customOffset, 16);
      const files = [
        {
          data: customFile.buffer,
          address: isNaN(offset) ? 0x10000 : offset,
          name: customFile.name,
        },
      ];

      await flasher.flashFiles(files, eraseAll);
    } catch (err: any) {
      setStatus({
        state: 'error',
        progress: 0,
        message: err.message || 'Flash failed',
      });
    } finally {
      setIsFlashing(false);
    }
  };

  const handleToggleMonitor = async () => {
    if (!flasher) return;

    if (isMonitorActive) {
      await flasher.disconnect();
      setIsMonitorActive(false);
    } else {
      const ok = await flasher.startSerialMonitor(115200, (data) => {
        const time = new Date().toLocaleTimeString();
        setSerialLogs((prev) => [...prev.slice(-300), { text: data, type: 'serial', time }]);
      });
      setIsMonitorActive(ok);
    }
  };

  const handleSendCommand = (cmd?: string) => {
    const textToSend = cmd || commandInput;
    if (!textToSend || !flasher) return;

    flasher.writeSerial(textToSend);
    setSerialLogs((prev) => [
      ...prev,
      { text: `> ${textToSend}`, type: 'input', time: new Date().toLocaleTimeString() },
    ]);
    if (!cmd) setCommandInput('');
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = () => {
      if (reader.result instanceof ArrayBuffer) {
        setCustomFile({
          name: file.name,
          buffer: reader.result,
        });
      }
    };
    reader.readAsArrayBuffer(file);
  };

  if (!isSupported) {
    return (
      <div className="p-4 bg-[#111] border border-red-900 rounded text-red-400 font-mono text-xs">
        Web Serial API not available. Use Google Chrome or Microsoft Edge.
      </div>
    );
  }

  return (
    <div className="flex flex-col w-full bg-[#0d0d0d] border border-[#222] rounded-lg font-mono text-xs text-gray-300">
      {/* Port Connection Header */}
      <div className="flex flex-wrap items-center justify-between p-3 border-b border-[#222] gap-3 bg-[#111]">
        <div className="flex items-center gap-2.5">
          <div className={`w-2 h-2 rounded-full ${connectedChip ? 'bg-emerald-400' : 'bg-neutral-600'}`} />
          <span className="text-white font-medium">
            {connectedChip ? connectedChip : 'No Device Connected'}
          </span>
          {connectedMac && (
            <span className="text-gray-500 text-[11px]">[{connectedMac}]</span>
          )}
        </div>

        <div className="flex items-center gap-2">
          <select
            value={baudRate}
            onChange={(e) => setBaudRate(Number(e.target.value))}
            disabled={isFlashing}
            className="px-2 py-1 bg-[#1a1a1a] border border-[#333] rounded text-gray-300 text-[11px] focus:outline-none"
          >
            <option value={460800}>460800 baud</option>
            <option value={921600}>921600 baud</option>
            <option value={115200}>115200 baud</option>
          </select>

          <button
            onClick={handleConnect}
            disabled={isFlashing}
            className="px-3 py-1 bg-[#1f1f1f] hover:bg-[#2a2a2a] border border-[#383838] text-white rounded text-[11px] transition"
          >
            {connectedChip ? 'Re-Sync' : 'Connect Port'}
          </button>
        </div>
      </div>

      {/* Mode Selector */}
      <div className="flex items-center border-b border-[#222] bg-[#0d0d0d] px-3 pt-2 gap-4">
        <button
          onClick={() => setActiveTab('preset')}
          className={`pb-2 border-b-2 text-[11px] transition ${
            activeTab === 'preset'
              ? 'border-white text-white font-bold'
              : 'border-transparent text-gray-500 hover:text-gray-300'
          }`}
        >
          Preset: {preset.title}
        </button>

        <button
          onClick={() => setActiveTab('custom')}
          className={`pb-2 border-b-2 text-[11px] transition ${
            activeTab === 'custom'
              ? 'border-white text-white font-bold'
              : 'border-transparent text-gray-500 hover:text-gray-300'
          }`}
        >
          Custom .bin
        </button>
      </div>

      {/* Main Flash Panel */}
      <div className="p-4 flex flex-col gap-3">
        {activeTab === 'preset' ? (
          <>
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-2">
              {preset.files.map((file) => (
                <div key={file.name} className="p-2 bg-[#141414] border border-[#222] rounded text-[11px]">
                  <div className="text-gray-500">{file.name}</div>
                  <div className="text-gray-300 font-bold">0x{file.offset.toString(16).toUpperCase()}</div>
                </div>
              ))}
            </div>

            <div className="flex items-center justify-between pt-1">
              <label className="flex items-center gap-2 text-gray-400 text-[11px] cursor-pointer">
                <input
                  type="checkbox"
                  checked={eraseAll}
                  onChange={(e) => setEraseAll(e.target.checked)}
                  className="rounded bg-[#1a1a1a] border-[#333]"
                />
                Erase entire flash before write
              </label>
            </div>

            <button
              onClick={handleFlashPreset}
              disabled={isFlashing}
              className={`w-full py-2.5 rounded font-bold text-xs tracking-wider transition ${
                isFlashing
                  ? 'bg-neutral-800 text-neutral-500 cursor-not-allowed'
                  : 'bg-white text-black hover:bg-neutral-200'
              }`}
            >
              {isFlashing ? `FLASHING... (${status.progress}%)` : `FLASH FIRMWARE TO ${preset.chip.toUpperCase()}`}
            </button>
          </>
        ) : (
          <>
            <div className="flex flex-wrap items-center gap-2">
              <label className="flex-1 min-w-[180px] flex items-center justify-center p-3 border border-dashed border-[#333] hover:border-gray-500 rounded bg-[#141414] cursor-pointer">
                <input type="file" accept=".bin" onChange={handleFileChange} className="hidden" />
                <span className="text-gray-400 text-[11px]">
                  {customFile ? customFile.name : 'Select firmware.bin'}
                </span>
              </label>

              <div className="flex items-center gap-1.5 bg-[#141414] border border-[#222] px-2 py-1.5 rounded">
                <span className="text-gray-500 text-[11px]">Offset:</span>
                <input
                  type="text"
                  value={customOffset}
                  onChange={(e) => setCustomOffset(e.target.value)}
                  className="w-20 bg-transparent text-white text-center focus:outline-none text-[11px]"
                />
              </div>
            </div>

            <button
              onClick={handleFlashCustom}
              disabled={!customFile || isFlashing}
              className={`w-full py-2.5 rounded font-bold text-xs tracking-wider transition ${
                !customFile || isFlashing
                  ? 'bg-neutral-800 text-neutral-500 cursor-not-allowed'
                  : 'bg-white text-black hover:bg-neutral-200'
              }`}
            >
              FLASH CUSTOM BINARY
            </button>
          </>
        )}

        {/* Status Line */}
        {status.state !== 'idle' && (
          <div className="p-2.5 bg-[#141414] border border-[#262626] rounded flex flex-col gap-1.5 text-[11px]">
            <div className="flex justify-between items-center">
              <span className={status.state === 'error' ? 'text-red-400' : 'text-gray-300'}>
                {status.message}
              </span>
              <span className="text-white font-bold">{status.progress}%</span>
            </div>
            <div className="w-full h-1 bg-[#222] rounded overflow-hidden">
              <div
                className={`h-full transition-all duration-150 ${status.state === 'error' ? 'bg-red-500' : 'bg-white'}`}
                style={{ width: `${status.progress}%` }}
              />
            </div>
          </div>
        )}
      </div>

      {/* Integrated Serial Monitor */}
      <div className="border-t border-[#222] bg-[#0a0a0a] p-3 flex flex-col gap-2">
        <div className="flex items-center justify-between text-[11px]">
          <span className="text-gray-400 font-bold">SERIAL MONITOR (115200)</span>
          <div className="flex items-center gap-2">
            <button
              onClick={handleToggleMonitor}
              className="text-gray-400 hover:text-white underline text-[11px]"
            >
              {isMonitorActive ? 'Disconnect' : 'Connect'}
            </button>
            <button
              onClick={() => setSerialLogs([])}
              className="text-gray-600 hover:text-gray-400"
              title="Clear"
            >
              <Trash2 className="w-3 h-3" />
            </button>
          </div>
        </div>

        <div
          ref={logContainerRef}
          className="h-32 overflow-y-auto bg-black p-2 rounded border border-[#1a1a1a] text-[10px] leading-relaxed flex flex-col select-text font-mono"
        >
          {serialLogs.length === 0 ? (
            <span className="text-neutral-700">Serial logs will appear here when connected...</span>
          ) : (
            serialLogs.map((log, idx) => (
              <div
                key={idx}
                className={
                  log.type === 'error'
                    ? 'text-red-400'
                    : log.type === 'input'
                    ? 'text-amber-400'
                    : 'text-neutral-300'
                }
              >
                <span className="text-neutral-600 mr-1.5">[{log.time}]</span>
                {log.text}
              </div>
            ))
          )}
        </div>

        <form
          onSubmit={(e) => {
            e.preventDefault();
            handleSendCommand();
          }}
          className="flex items-center gap-1.5"
        >
          <input
            type="text"
            value={commandInput}
            onChange={(e) => setCommandInput(e.target.value)}
            placeholder="Send serial command..."
            className="flex-1 px-2.5 py-1.5 bg-[#141414] border border-[#242424] rounded text-white text-[11px] focus:outline-none focus:border-neutral-500"
          />
          <button
            type="submit"
            className="px-3 py-1.5 bg-[#222] hover:bg-[#333] text-white rounded text-[11px] font-bold"
          >
            Send
          </button>
        </form>
      </div>
    </div>
  );
};
