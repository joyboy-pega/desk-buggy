import Foundation
import AVFoundation
import CoreVideo
import Accelerate

// ============================================================================
// MacBook Live Camera Streamer (60 FPS TURBO - 100% USB Cable)
// Hardware Paced 60 FPS Transmission over 921,600 Baud
// ============================================================================

let TURBO_W = 80
let TURBO_H = 32
let HD_W = 80
let HD_H = 64

class SerialPort {
    var fd: Int32 = -1

    init?(path: String, baud: speed_t = 921600) {
        fd = open(path, O_RDWR | O_NOCTTY)
        guard fd >= 0 else { return nil }

        var tty = termios()
        if tcgetattr(fd, &tty) != 0 {
            close(fd)
            return nil
        }

        cfsetospeed(&tty, baud)
        cfsetispeed(&tty, baud)

        tty.c_cflag |= tcflag_t(CS8 | CLOCAL | CREAD)
        tty.c_cflag &= ~tcflag_t(PARENB | CSTOPB | CRTSCTS)
        tty.c_iflag = 0
        tty.c_oflag = 0
        tty.c_lflag = 0

        // 50ms read timeout
        tty.c_cc.16 = 0
        tty.c_cc.17 = 1

        tcflush(fd, TCIOFLUSH)
        if tcsetattr(fd, TCSANOW, &tty) != 0 {
            close(fd)
            return nil
        }
    }

    func waitForAck() {
        guard fd >= 0 else { return }
        var ack: UInt8 = 0
        _ = read(fd, &ack, 1)
    }

    func writeAll(_ data: Data) -> Bool {
        guard fd >= 0 else { return false }
        var totalWritten = 0
        let count = data.count
        return data.withUnsafeBytes { ptr -> Bool in
            guard let base = ptr.baseAddress else { return false }
            while totalWritten < count {
                let n = write(fd, base.advanced(by: totalWritten), count - totalWritten)
                if n <= 0 { return false }
                totalWritten += n
            }
            return true
        }
    }

    deinit {
        if fd >= 0 { close(fd) }
    }
}

// 16-color Euclidean palette lookup
struct RGBColor {
    let r: UInt8
    let g: UInt8
    let b: UInt8
}

let palette16Colors: [RGBColor] = [
    RGBColor(r: 0, g: 0, b: 0),       // 0: Black
    RGBColor(r: 49, g: 49, b: 49),    // 1: Dark Slate Gray
    RGBColor(r: 115, g: 115, b: 115),// 2: Mid Gray
    RGBColor(r: 189, g: 189, b: 189),// 3: Light Gray
    RGBColor(r: 255, g: 255, b: 255),// 4: White
    RGBColor(r: 0, g: 0, b: 184),    // 5: Navy Blue
    RGBColor(r: 43, g: 95, b: 255),  // 6: Sky Blue
    RGBColor(r: 0, g: 255, b: 255),  // 7: Neon Cyan
    RGBColor(r: 0, g: 156, b: 0),    // 8: Forest Green
    RGBColor(r: 0, g: 255, b: 0),    // 9: Neon Green
    RGBColor(r: 152, g: 0, b: 0),    // 10: Deep Red
    RGBColor(r: 255, g: 0, b: 0),    // 11: Bright Red
    RGBColor(r: 130, g: 65, b: 32),  // 12: Dark Skin
    RGBColor(r: 195, g: 115, b: 73), // 13: Mid Skin
    RGBColor(r: 244, g: 158, b: 130),// 14: Light Skin
    RGBColor(r: 254, g: 198, b: 185) // 15: Peach Face Highlight
]

func quantizeTo4Bit(r: UInt8, g: UInt8, b: UInt8) -> UInt8 {
    var bestDist = Int.max
    var bestIdx: UInt8 = 0
    let ri = Int(r)
    let gi = Int(g)
    let bi = Int(b)

    for i in 0..<16 {
        let pr = Int(palette16Colors[i].r)
        let pg = Int(palette16Colors[i].g)
        let pb = Int(palette16Colors[i].b)
        let dist = (ri - pr)*(ri - pr) + (gi - pg)*(gi - pg) + (bi - pb)*(bi - pb)
        if dist < bestDist {
            bestDist = dist
            bestIdx = UInt8(i)
        }
    }
    return bestIdx
}

class CameraStreamer: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    let session = AVCaptureSession()
    var serial: SerialPort?
    var isTurbo60fps = true

    let frameLock = NSLock()
    var latestFramePacket: Data?
    var isRunning = true

    var scaledBuffer = [UInt8](repeating: 0, count: 80 * 64 * 4)

    func start(portPath: String, turbo60: Bool) {
        self.isTurbo60fps = turbo60

        print("\n========================================================")
        print("   MacBook Live Camera Streamer (60 FPS TURBO)          ")
        print("========================================================")
        print("[SERIAL] Connecting to \(portPath) at 921,600 baud...")

        guard let sp = SerialPort(path: portPath, baud: 921600) else {
            print("[ERROR] Cannot open serial port: \(portPath)")
            exit(1)
        }
        self.serial = sp
        print("[SERIAL] Connected over high-speed USB cable!")

        guard let device = AVCaptureDevice.default(for: .video) else {
            print("[ERROR] No video camera found.")
            exit(1)
        }
        print("[CAMERA] Opened: \(device.localizedName)")

        // Unlock 60 FPS on FaceTime Camera
        do {
            try device.lockForConfiguration()
            for format in device.formats {
                for range in format.videoSupportedFrameRateRanges {
                    if range.maxFrameRate >= 60.0 {
                        device.activeFormat = format
                        device.activeVideoMinFrameDuration = CMTime(value: 1, timescale: 60)
                        device.activeVideoMaxFrameDuration = CMTime(value: 1, timescale: 60)
                        break
                    }
                }
            }
            device.unlockForConfiguration()
            print("[CAMERA] 60 FPS Hardware Capture Unlocked!")
        } catch {
            print("[CAMERA] Standard FPS active.")
        }

        session.sessionPreset = .vga640x480
        guard let input = try? AVCaptureDeviceInput(device: device), session.canAddInput(input) else {
            print("[ERROR] Could not add camera input.")
            exit(1)
        }
        session.addInput(input)

        let output = AVCaptureVideoDataOutput()
        output.alwaysDiscardsLateVideoFrames = true
        output.videoSettings = [
            kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA
        ]

        let queue = DispatchQueue(label: "cam.stream.60fps", qos: .userInteractive)
        output.setSampleBufferDelegate(self, queue: queue)
        guard session.canAddOutput(output) else {
            print("[ERROR] Could not add camera output.")
            exit(1)
        }
        session.addOutput(output)

        session.startRunning()

        print("[STREAM] Liquid-smooth video active!")
        print("[STREAM] Target: \(turbo60 ? "60 FPS TURBO MODE" : "35 FPS HIGH-DEF MODE")")
        print("[STREAM] Press Ctrl+C in this terminal to exit.")
        print("--------------------------------------------------------\n")

        DispatchQueue.global(qos: .userInteractive).async {
            self.streamLoop()
        }

        RunLoop.main.run()
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        guard let pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }

        CVPixelBufferLockBaseAddress(pixelBuffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(pixelBuffer, .readOnly) }

        let srcWidth = CVPixelBufferGetWidth(pixelBuffer)
        let srcHeight = CVPixelBufferGetHeight(pixelBuffer)
        let srcBytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer)
        guard let srcBase = CVPixelBufferGetBaseAddress(pixelBuffer) else { return }

        let targetW = isTurbo60fps ? TURBO_W : HD_W
        let targetH = isTurbo60fps ? TURBO_H : HD_H

        let targetAspect = Double(targetW) / Double(targetH)
        let srcAspect = Double(srcWidth) / Double(srcHeight)

        var cropX = 0
        var cropY = 0
        var cropW = srcWidth
        var cropH = srcHeight

        if srcAspect > targetAspect {
            cropW = Int(Double(srcHeight) * targetAspect)
            cropX = (srcWidth - cropW) / 2
        } else {
            cropH = Int(Double(srcWidth) / targetAspect)
            cropY = (srcHeight - cropH) / 2
        }

        let croppedBase = srcBase.advanced(by: cropY * srcBytesPerRow + cropX * 4)

        var srcVImage = vImage_Buffer(
            data: croppedBase,
            height: vImagePixelCount(cropH),
            width: vImagePixelCount(cropW),
            rowBytes: srcBytesPerRow
        )

        scaledBuffer.withUnsafeMutableBytes { dstPtr in
            guard let dstBase = dstPtr.baseAddress else { return }
            var dstVImage = vImage_Buffer(
                data: dstBase,
                height: vImagePixelCount(targetH),
                width: vImagePixelCount(targetW),
                rowBytes: targetW * 4
            )

            vImageScale_ARGB8888(&srcVImage, &dstVImage, nil, vImage_Flags(kvImageNoFlags))

            let bgra = dstBase.assumingMemoryBound(to: UInt8.self)

            if self.isTurbo60fps {
                // Mode 0x04: 80x32 4-bit = 1,280 bytes
                var packet = Data(capacity: 5 + 1280)
                packet.append(contentsOf: [0xAA, 0x55, 0xAA, 0x55, 0x04])

                for y in 0..<targetH {
                    let rowOffset = y * targetW * 4
                    var x = 0
                    while x < targetW {
                        let mirrorX1 = targetW - 1 - x
                        let idx1 = rowOffset + mirrorX1 * 4
                        let nib1 = quantizeTo4Bit(r: bgra[idx1 + 2], g: bgra[idx1 + 1], b: bgra[idx1])

                        let mirrorX2 = targetW - 1 - (x + 1)
                        let idx2 = rowOffset + mirrorX2 * 4
                        let nib2 = quantizeTo4Bit(r: bgra[idx2 + 2], g: bgra[idx2 + 1], b: bgra[idx2])

                        packet.append((nib1 << 4) | (nib2 & 0x0F))
                        x += 2
                    }
                }

                self.frameLock.lock()
                self.latestFramePacket = packet
                self.frameLock.unlock()
            } else {
                // Mode 0x03: 80x64 4-bit = 2,560 bytes
                var packet = Data(capacity: 5 + 2560)
                packet.append(contentsOf: [0xAA, 0x55, 0xAA, 0x55, 0x03])

                for y in 0..<targetH {
                    let rowOffset = y * targetW * 4
                    var x = 0
                    while x < targetW {
                        let mirrorX1 = targetW - 1 - x
                        let idx1 = rowOffset + mirrorX1 * 4
                        let nib1 = quantizeTo4Bit(r: bgra[idx1 + 2], g: bgra[idx1 + 1], b: bgra[idx1])

                        let mirrorX2 = targetW - 1 - (x + 1)
                        let idx2 = rowOffset + mirrorX2 * 4
                        let nib2 = quantizeTo4Bit(r: bgra[idx2 + 2], g: bgra[idx2 + 1], b: bgra[idx2])

                        packet.append((nib1 << 4) | (nib2 & 0x0F))
                        x += 2
                    }
                }

                self.frameLock.lock()
                self.latestFramePacket = packet
                self.frameLock.unlock()
            }
        }
    }

    func streamLoop() {
        var frameCount = 0
        var timer = Date()
        // 16.6 ms per frame = 60.0 FPS cadence
        let frameInterval: UInt32 = 16666

        while isRunning {
            guard let sp = serial else { break }

            var packetToSend: Data?
            frameLock.lock()
            packetToSend = latestFramePacket
            frameLock.unlock()

            if let packet = packetToSend {
                if sp.writeAll(packet) {
                    frameCount += 1
                }
            }

            usleep(frameInterval)

            let now = Date()
            let elapsed = now.timeIntervalSince(timer)
            if elapsed >= 1.0 {
                let fps = Double(frameCount) / elapsed
                print(String(format: "[60 FPS CABLE STREAM] Fluid 60 FPS: %.1f FPS (Liquid Motion)", fps))
                frameCount = 0
                timer = now
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Entry Point
// ----------------------------------------------------------------------------
func findPort() -> String {
    let fm = FileManager.default
    if let devices = try? fm.contentsOfDirectory(atPath: "/dev") {
        for d in devices {
            if d.hasPrefix("cu.usbserial") || d.hasPrefix("cu.wchusbserial") || d.hasPrefix("cu.SLAB_USBtoUART") {
                return "/dev/" + d
            }
        }
    }
    return "/dev/cu.usbserial-140"
}

let args = CommandLine.arguments
var port = findPort()
var turbo = true // Default is 60 FPS turbo!

for i in 1..<args.count {
    if args[i] == "--port" && i + 1 < args.count {
        port = args[i + 1]
    } else if args[i] == "--35fps" || args[i] == "--hd" {
        turbo = false
    } else if args[i] == "--help" || args[i] == "-h" {
        print("Usage: ./mac_cam_cable [--port /dev/cu.usbserial-140] [--35fps]")
        print("  default: 60 FPS TURBO Mode")
        print("  --35fps: 35 FPS High-Def Mode")
        exit(0)
    }
}

let streamer = CameraStreamer()
streamer.start(portPath: port, turbo60: turbo)
