# UnifiedFloppyTool v4.1.0 Release Notes

**"Bei uns geht kein Bit verloren"**

## Highlights

### 🔧 Hardware Protocol Audit – Complete Rewrite
All hardware backend protocols have been audited against original manufacturer
specifications and corrected. This is the most significant reliability improvement
since the project's inception.

**SuperCard Pro** – Rewritten from SDK v1.7 + samdisk reference:
- Binary commands `0x80–0xD2` (previously fabricated ASCII protocol)
- Correct checksum (init `0x4A`), big-endian encoding
- 512K onboard RAM model: `READFLUX→GETFLUXINFO→SENDRAM_USB` read flow
- FTDI FT240-X USB FIFO (was incorrectly 38400 baud serial)

**FC5025** – Rewritten from DeviceSide driver source v1309:
- SCSI-like CBW/CSW protocol (signature `CFBC`) with correct opcodes
- `SEEK=0xC0`, `FLAGS=0xC2`, `READ_ID=0xC7`, `READ_FLEXIBLE=0xC6`
- Correctly marked as **read-only** device (no write support in firmware)

**KryoFlux** – Architecture correction:
- Replaced fabricated USB commands with official DTC subprocess wrapper
- KryoFlux stream format parser (OOB blocks) retained and verified

**Pauline** – Architecture correction:
- Replaced fabricated binary protocol with HTTP/SSH control
- Matches real architecture: embedded Linux on DE10-nano FPGA

**Greaseweazle** – 16 protocol bugs fixed:
- Correct USB command codes from `gw_protocol.h` v1.23
- Fixed bandwidth negotiation, SET_BUS_TYPE, GET_FLUX_STATUS
- Proper CRC-based checksum for USB frames

### 📦 Build & Packaging
- GitHub Actions CI/CD for automated multi-platform releases
- macOS: `.dmg` + `.tar.gz` (Universal Binary x86_64/arm64)
- Linux: `.deb` (Ubuntu/Debian) + `.tar.gz`
- Windows: `.tar.gz` with Qt DLLs bundled

## System Requirements
- Qt 6.5+ (Qt 6.7 recommended)
- macOS 11+ (Big Sur), Windows 10+, Ubuntu 22.04+
- Optional: libusb-1.0, Qt SerialPort module

## Supported Hardware
- Greaseweazle (v1.x firmware)
- SuperCard Pro (SCP SDK v1.7)
- FC5025 (read-only)
- KryoFlux (via DTC tool)
- Pauline FDC (via network)
- FluxEngine
- XUM1541 / OpenCBM
