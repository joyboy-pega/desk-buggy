/**
 * Web Serial ESP Flasher & Serial Monitor
 * Powered by esptool-js and the Web Serial API
 */

export interface FlasherStatus {
  state: 'idle' | 'connecting' | 'syncing' | 'erasing' | 'flashing' | 'verifying' | 'done' | 'error';
  progress: number;
  message: string;
  chipName?: string;
  macAddr?: string;
}

export type LogCallback = (text: string, type?: 'info' | 'error' | 'success' | 'serial') => void;
export type StatusCallback = (status: FlasherStatus) => void;

export class EspWebFlasher {
  private port: any = null;
  private transport: any = null;
  private esploader: any = null;
  private reader: any = null;
  private writer: any = null;
  private isReadingSerial: boolean = false;

  constructor(
    private onLog: LogCallback,
    private onStatus: StatusCallback
  ) {}

  public static isSupported(): boolean {
    return typeof window !== 'undefined' && 'serial' in navigator;
  }

  /**
   * Request user to pick a serial port
   */
  public async requestPort(): Promise<boolean> {
    if (!EspWebFlasher.isSupported()) {
      this.onLog('Web Serial API is not supported in this browser. Please use Google Chrome, Edge, or Opera.', 'error');
      return false;
    }

    try {
      this.port = await (navigator as any).serial.requestPort({
        filters: [
          // Espressif USB VID
          { usbVendorId: 0x303a },
          // Silicon Labs CP210x
          { usbVendorId: 0x10c4 },
          // WCH CH340 / CH341
          { usbVendorId: 0x1a86 },
          // FTDI
          { usbVendorId: 0x0403 },
        ],
      });
      return !!this.port;
    } catch (err: any) {
      if (err.name !== 'NotFoundError') {
        this.onLog(`Failed to select serial port: ${err.message}`, 'error');
      }
      return false;
    }
  }

  /**
   * Connect to ESP32 / ESP32-C3 bootloader and retrieve chip details
   */
  public async connectBootloader(baudRate: number = 460800): Promise<{ chipName: string; mac: string } | null> {
    if (!this.port) {
      const ok = await this.requestPort();
      if (!ok) return null;
    }

    this.onStatus({ state: 'connecting', progress: 5, message: 'Opening serial port...' });
    this.onLog('Connecting to microcontroller bootloader...', 'info');

    try {
      // Dynamically load esptool-js in browser client
      const { ESPLoader, Transport } = await import('esptool-js');

      this.transport = new Transport(this.port, true);

      const terminal = {
        clean: () => {},
        writeLine: (data: string) => this.onLog(data, 'info'),
        write: (data: string) => this.onLog(data, 'info'),
      };

      this.esploader = new ESPLoader({
        transport: this.transport,
        baudrate: baudRate,
        romBaudrate: 115200,
        terminal: terminal,
        enableTracing: false,
      });

      this.onStatus({ state: 'syncing', progress: 15, message: 'Syncing with ESP ROM bootloader...' });
      this.onLog('Syncing with ESP ROM bootloader (entering download mode)...', 'info');

      const chip = await this.esploader.main();
      this.onLog(`Connected chip: ${chip}`, 'success');

      let mac = 'Unknown';
      try {
        const macArray = await this.esploader.chip.readMac(this.esploader);
        if (macArray) {
          mac = Array.from(macArray as number[])
            .map((b: number) => b.toString(16).padStart(2, '0').toUpperCase())
            .join(':');
          this.onLog(`MAC Address: ${mac}`, 'info');
        }
      } catch (e) {
        // Fallback MAC read if supported
      }

      this.onStatus({
        state: 'idle',
        progress: 25,
        message: `Ready. Detected ${chip}`,
        chipName: chip,
        macAddr: mac,
      });

      return { chipName: chip, mac };
    } catch (err: any) {
      this.onLog(`Connection error: ${err.message || err}`, 'error');
      this.onStatus({ state: 'error', progress: 0, message: `Failed: ${err.message || err}` });
      await this.disconnect();
      return null;
    }
  }

  /**
   * Flash binary files to hardware
   */
  public async flashFiles(
    files: { data: ArrayBuffer; address: number; name: string }[],
    eraseAll: boolean = false
  ): Promise<boolean> {
    if (!this.esploader) {
      const conn = await this.connectBootloader();
      if (!conn) return false;
    }

    try {
      this.onStatus({ state: 'flashing', progress: 30, message: 'Preparing firmware images...' });
      this.onLog(`Preparing ${files.length} binary partition(s) for flashing...`, 'info');

      const fileArray: { data: string; address: number }[] = [];
      for (const f of files) {
        this.onLog(`File: ${f.name} (Size: ${(f.data.byteLength / 1024).toFixed(1)} KB) -> Offset: 0x${f.address.toString(16)}`, 'info');
        fileArray.push({
          data: this.arrayBufferToBinaryString(f.data),
          address: f.address,
        });
      }

      this.onStatus({ state: 'flashing', progress: 40, message: eraseAll ? 'Erasing chip flash...' : 'Writing flash memory...' });

      await this.esploader.writeFlash({
        fileArray: fileArray,
        flashSize: 'keep',
        flashMode: 'keep',
        flashFreq: 'keep',
        eraseAll: eraseAll,
        compress: true,
        reportProgress: (fileIndex: number, written: number, total: number) => {
          const filePct = (written / total) * 100;
          const overallProgress = Math.round(40 + (fileIndex / files.length) * 55 + (written / total) * (55 / files.length));
          this.onStatus({
            state: 'flashing',
            progress: Math.min(overallProgress, 98),
            message: `Writing ${files[fileIndex].name}: ${filePct.toFixed(0)}% (${(written / 1024).toFixed(0)} / ${(total / 1024).toFixed(0)} KB)`,
          });
        },
      });

      this.onLog('Flashing completed successfully! Resetting MCU...', 'success');
      this.onStatus({ state: 'verifying', progress: 99, message: 'Hard resetting board...' });

      try {
        await this.esploader.after();
      } catch (e) {
        // Reset call
      }

      this.onStatus({ state: 'done', progress: 100, message: 'Firmware successfully pushed to hardware!' });
      return true;
    } catch (err: any) {
      this.onLog(`Flashing failed: ${err.message || err}`, 'error');
      this.onStatus({ state: 'error', progress: 0, message: `Flash failed: ${err.message || err}` });
      return false;
    } finally {
      await this.disconnect();
    }
  }

  /**
   * Helper to convert ArrayBuffer to binary string required by esptool-js
   */
  private arrayBufferToBinaryString(buffer: ArrayBuffer): string {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    const len = bytes.byteLength;
    const chunkSize = 8192;
    for (let i = 0; i < len; i += chunkSize) {
      const slice = bytes.subarray(i, Math.min(i + chunkSize, len));
      binary += String.fromCharCode.apply(null, Array.from(slice));
    }
    return binary;
  }

  /**
   * Start live Serial Monitor
   */
  public async startSerialMonitor(baudRate: number = 115200, onData: (data: string) => void): Promise<boolean> {
    if (!this.port) {
      const ok = await this.requestPort();
      if (!ok) return false;
    }

    try {
      if (!this.port.readable) {
        await this.port.open({ baudRate });
      }
      this.isReadingSerial = true;
      this.onLog(`Serial monitor connected at ${baudRate} baud`, 'success');

      // Start read loop
      (async () => {
        const textDecoder = new TextDecoderStream();
        const readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
        const reader = textDecoder.readable.getReader();
        this.reader = reader;

        try {
          while (this.isReadingSerial) {
            const { value, done } = await reader.read();
            if (done) break;
            if (value) {
              onData(value);
            }
          }
        } catch (e) {
          // Stream error or closed
        } finally {
          reader.releaseLock();
        }
      })();

      return true;
    } catch (err: any) {
      this.onLog(`Failed to open serial monitor: ${err.message}`, 'error');
      return false;
    }
  }

  /**
   * Send data to hardware via Serial
   */
  public async writeSerial(data: string): Promise<void> {
    if (!this.port || !this.port.writable) {
      this.onLog('Serial port is not writable', 'error');
      return;
    }

    try {
      const textEncoder = new TextEncoder();
      const writer = this.port.writable.getWriter();
      await writer.write(textEncoder.encode(data));
      writer.releaseLock();
    } catch (err: any) {
      this.onLog(`Failed to write serial command: ${err.message}`, 'error');
    }
  }

  /**
   * Disconnect port and clean up
   */
  public async disconnect(): Promise<void> {
    this.isReadingSerial = false;
    try {
      if (this.reader) {
        await this.reader.cancel();
        this.reader = null;
      }
      if (this.transport) {
        await this.transport.disconnect();
        this.transport = null;
      }
      if (this.port && this.port.readable) {
        await this.port.close();
      }
    } catch (e) {
      // Ignore disconnect cleanup errors
    } finally {
      this.port = null;
      this.esploader = null;
      this.onLog('Serial port disconnected.', 'info');
    }
  }
}
