# UFT TODO - v3.7.0

**Status:** CI Fixes Applied  
**Date:** 2025-01-11

---

## ✅ COMPLETED

### CI/Build Fixes (P0) - DONE
- [x] MSVC packed struct compatibility (pragma pack)
- [x] Windows S_ISDIR/S_ISREG macros
- [x] macOS strcasecmp (strings.h)
- [x] macOS CRTSCTS serial port flag
- [x] Integer overflow in display_track.c
- [x] Macro redefinition guards (uft_common.h, uft_atomics.h)

### RetroGhidra Formats - DONE
- [x] BBC Micro UEF Tape Format
- [x] ZX Spectrum SNA Snapshot (48K/128K)
- [x] Amstrad CPC SNA Snapshot
- [x] C64 CRT Cartridge (61 types)
- [x] Commodore D80/D82 Disk
- [x] Apple II DOS 3.3 / ProDOS
- [x] Atari 8-bit XEX / Atari ST PRG
- [x] TRS-80 /CMD / CoCo CCC
- [x] Spectrum Next NEX

### DDR/ZX Tape Formats - DONE
- [x] KC85 CAOS FSK
- [x] Z1013 Headersave
- [x] TZX/TAP/PZX
- [x] CSW v1/v2
- [x] C64 TAP/T64

### Test Suite - DONE
- [x] 19 ctest suites all passing
- [x] 435+ individual tests

---

## 🔄 PENDING (Awaiting CI Verification)

### P0 - CI Verification
- [ ] Windows build on GitHub Actions
- [ ] macOS build on GitHub Actions
- [ ] All platform tests green

---

## 📋 BACKLOG

### P1 - Format Extensions
- [ ] Classic Mac Resource Fork parser
- [ ] D80 BAM analysis
- [ ] Apple DOS 3.3 file extraction
- [ ] ProDOS file extraction

### P1 - Quality
- [ ] Fix remaining warnings (strncpy truncation)
- [ ] Remove unused variables in xdf module
- [ ] Add fread return value checks

### P2 - HAL
- [ ] Real device testing (Greaseweazle, FluxEngine)
- [ ] USB hotplug support
- [ ] Drive calibration profiles

### P3 - Documentation
- [ ] API documentation
- [ ] Format specification docs

---

## 📊 Metrics

| Category | Count |
|----------|-------|
| Files Fixed | 12 |
| Build Platforms | 3 (Linux ✅, Windows ⏳, macOS ⏳) |
| Test Suites | 19 |
| Total Tests | 435+ |

---

## Version History

- **v3.7.0** - CI fixes, cross-platform compatibility
