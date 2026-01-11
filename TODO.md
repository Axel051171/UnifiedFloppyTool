# UFT TODO - v3.7.0

**Status:** Release Candidate
**Date:** 2025-01-11

## ✅ COMPLETED (v3.7.0)

### RetroGhidra Format Integration - Phase 1
- [x] BBC Micro UEF Tape Format (chunk-based, BeebEm state)
- [x] ZX Spectrum SNA Snapshot (48K/128K)
- [x] Amstrad CPC SNA Snapshot (464/664/6128)
- [x] C64 CRT Cartridge (61 types, Big Endian)

### RetroGhidra Format Integration - Phase 2
- [x] Commodore D80/D82 Disk (8050/8250, 77/154 tracks)
- [x] Apple II DOS 3.3 Disk (35×16×256)
- [x] Apple II ProDOS Disk (block-based, hierarchical)
- [x] Atari 8-bit XEX Executable (segmented, RUNAD/INITAD)
- [x] Atari ST PRG/TOS Executable (68000, TEXT/DATA/BSS)
- [x] TRS-80 /CMD Executable (record-based)
- [x] CoCo CCC Cartridge (6809, 2K-32K)
- [x] Spectrum Next NEX Executable (512-byte header)

### DDR Tape Formats (v3.8.1-3.8.2)
- [x] KC85 CAOS FSK (8 file types, 128-byte header)
- [x] Z1013 Headersave (32-byte header, XOR checksum)
- [x] 7 Turboloader-Profile (TURBOTAPE to BASICODE)

### ZX Spectrum Tape (v3.8.3-3.8.5)
- [x] TZX v1.20 (24 block types)
- [x] TAP (simple tape format)
- [x] PZX (full PULS spec: Simple/Extended/Repeat)
- [x] CSW (Compressed Square Wave v1/v2)
- [x] C64 TAP (v0/v1/v2, PAL/NTSC)
- [x] T64 (tape archive, PETSCII)

### Test Suite
- [x] 435 tests, all PASS
- [x] 19 ctest suites
- [x] 71 RetroGhidra tests

---

## 🔄 IN PROGRESS (P0)

### CI/Release
- [ ] Windows CI build verification
- [ ] macOS CI build verification (OpenMP disabled)
- [ ] GitHub Release automation

---

## 📋 BACKLOG (P1)

### Format Extensions
- [ ] Classic Mac Resource Fork parser
- [ ] Commodore D80 BAM analysis
- [ ] Apple DOS 3.3 file extraction
- [ ] ProDOS file extraction

### HAL (Hardware Abstraction)
- [ ] Real device testing (Greaseweazle, FluxEngine)
- [ ] USB hotplug support
- [ ] Drive calibration profiles

### Quality
- [ ] Fuzz testing expansion
- [ ] Memory sanitizer runs
- [ ] Coverage report

---

## 📊 METRICS

| Category | Count |
|----------|-------|
| Tape Formats | 10 |
| Snapshot Formats | 2 |
| Cartridge Formats | 2 |
| Disk Formats | 3 (D80, DOS 3.3, ProDOS) |
| Executable Formats | 4 (XEX, PRG, CMD, NEX) |
| Tests | 435 |
| Test Suites | 19 |

---

## 🏷️ VERSION HISTORY

- **v3.7.0** - RetroGhidra Phase 2 (8 formats)
- **v3.8.5** - C64 Tape (TAP, T64)
- **v3.8.4** - CSW Compressed Square Wave
- **v3.8.3** - TZX/TAP/PZX ZX Spectrum Tape
- **v3.8.2** - KC Turbo Profiles
- **v3.8.1** - DDR Tape (KC85, Z1013)
- **v3.7.0** - Initial Release
