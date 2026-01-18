# XUM1541-II Hardware Guide

## Overview

The **XUM1541-II** is a DIY USB adapter for connecting Commodore disk drives to modern computers. Created by [tebl](https://github.com/tebl/C64-XUM1541-II), it's an improved version of the original XUM1541 design with better signal quality through a 7406 hex inverter.

UFT provides full support for XUM1541-II and other XUM1541 variants.

## Supported Hardware Variants

| Variant | Description | Parallel | IEEE-488 | 7406 | Firmware |
|---------|-------------|----------|----------|------|----------|
| **ZoomFloppy** | Commercial (RETRO Innovations) | ✅ | Optional | ✅ | v08+ |
| **XUM1541-II** | DIY by tebl (Pro Micro + 7406) | ✅ | ❌ | ✅ | v08+ |
| **XUM1541** | Original DIY (Pro Micro only) | ❌ | ❌ | ❌ | v07 |
| **AT90USBKEY** | Development board | ✅ | ❌ | ❌ | v08+ |
| **Bumble-B** | ATmega32U2 daughterboard | ✅ | ❌ | ✅ | v08+ |

## XUM1541-II Specifications

```
┌─────────────────────────────────────────────────────────────┐
│                     XUM1541-II                              │
├─────────────────────────────────────────────────────────────┤
│  MCU:           Arduino Pro Micro (ATmega32U4)              │
│  Inverter:      7406 Hex Inverter (improved signals)        │
│  USB:           VID:PID 16D0:0504                           │
│  Firmware:      xum1541-PROMICRO_7406-v08.hex               │
│  Transfer:      ~60 seconds/disk (serial)                   │
│                 ~30 seconds/disk (with parallel adapter)    │
├─────────────────────────────────────────────────────────────┤
│  Supported Drives:                                          │
│    • Commodore 1541 / 1541-II                               │
│    • Commodore 1571                                         │
│    • Commodore 1581                                         │
│    • CMD FD2000 / FD4000                                    │
└─────────────────────────────────────────────────────────────┘
```

## Hardware Setup

### Required Components (Main Board)

| Component | Value | Notes |
|-----------|-------|-------|
| Arduino Pro Micro | 5V/16MHz | ATmega32U4 based |
| 7406N | Hex Inverter | **Key component for v08 firmware** |
| DIN-6 Connector | Female | IEC serial bus connector |
| Resistors | Various | See schematic |
| Capacitors | 100nF | Decoupling |

### Optional Parallel Adapter

For 2x faster transfers, add the parallel interface module:
- Directly connects to 1541-II disk drive
- Requires parallel cable to XUM1541-II
- Compatible with SpeedDOS ROMs

### Building Resources

- **PCB Files**: [github.com/tebl/C64-XUM1541-II](https://github.com/tebl/C64-XUM1541-II)
- **Schematic**: Available in KiCAD and PDF format
- **PCBWay**: Pre-made PCBs available for order

## Firmware Installation

### Prerequisites

```bash
# Windows: Install WinAVR for avrdude
# Linux:
sudo apt install avrdude

# macOS:
brew install avrdude
```

### Flashing the Firmware

1. **Download firmware**: `xum1541-PROMICRO_7406-v08.hex`
   - Source: [github.com/tebl/C64-XUM1541-II/software/firmware](https://github.com/tebl/C64-XUM1541-II/tree/main/software/firmware)

2. **Put Pro Micro in bootloader mode**:
   - Short RST to GND twice quickly
   - LED will pulse, new COM port appears

3. **Flash with avrdude**:
   ```bash
   avrdude -p m32u4 -c avr109 -P COM10 -U flash:w:xum1541-PROMICRO_7406-v08.hex
   ```
   (Replace COM10 with your port)

4. **Install USB driver** (Windows):
   - Use Zadig to install libusb-win32 driver
   - Select "xum1541" device (NOT the Arduino)

## Using with UFT

### Detection

UFT automatically detects XUM1541-II devices:

```bash
# List connected hardware
uft --list-hardware

# Expected output:
# XUM1541-II detected at USB 16D0:0504
# Firmware: v08
# Capabilities: IEC Serial, Parallel (if connected)
```

### Reading Disks

```bash
# Read D64 image
uft read --device xum1541 --drive 8 --output mydisk.d64

# Read with parallel (2x speed)
uft read --device xum1541 --drive 8 --parallel --output mydisk.d64

# Read G64 (raw GCR for copy protection)
uft read --device xum1541 --drive 8 --format g64 --output mydisk.g64
```

### Writing Disks

```bash
# Write D64 to disk
uft write --device xum1541 --drive 8 --input mydisk.d64

# Verify after write
uft write --device xum1541 --drive 8 --input mydisk.d64 --verify
```

### GUI Usage

1. Open UFT GUI
2. Go to **Hardware** tab
3. Select "XUM1541/ZoomFloppy" from dropdown
4. Click **Detect** to find device
5. Select drive number (usually 8)
6. Use **Read** or **Write** buttons

## Troubleshooting

### Device Not Detected

| Problem | Solution |
|---------|----------|
| No COM port | Check USB cable, try different port |
| Wrong driver | Use Zadig to install libusb-win32 |
| Firmware issue | Re-flash with correct HEX file |

### Transfer Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Device not responding" | Drive not powered | Turn on disk drive first |
| "IEC timeout" | Cable issue | Check DIN-6 connections |
| "Read error track X" | Dirty/damaged disk | Clean disk, try nibtools |

### Firmware Version Issues

| Hardware | Recommended Firmware |
|----------|---------------------|
| XUM1541-II (with 7406) | v08 (`PROMICRO_7406`) |
| XUM1541 (without 7406) | v07 (`PROMICRO`) |
| ZoomFloppy | v08 (`ZOOMFLOPPY`) |

⚠️ **Important**: Using v08 firmware on hardware without 7406 inverter will NOT work!

## Advanced: Parallel Transfer

### Speed Comparison

| Mode | Time per Disk | Notes |
|------|---------------|-------|
| Serial only | ~60 sec | Standard IEC bus |
| With Parallel | ~30 sec | Requires parallel adapter |
| With nibtools | ~90 sec | For copy protection |

### Parallel Adapter Setup

1. Build the "C64 1541-II Parallel Adapter" module
2. Install in 1541-II drive (directly on VIA chip)
3. Connect parallel cable to XUM1541-II
4. Use `--parallel` flag in UFT

## ROM Adapter Options

The XUM1541-II project also includes ROM adapters for 1541-II:

| ROM | Description |
|-----|-------------|
| Original | Standard CBM DOS 2.6 |
| JiffyDOS | Fast serial protocol |
| SpeedDOS | Parallel protocol |
| DolphinDOS | Alternative fast loader |

Use the "C64 1541-II ROM Adapter" to switch between ROMs.

## Related Resources

- [tebl/C64-XUM1541-II](https://github.com/tebl/C64-XUM1541-II) - Hardware project
- [OpenCBM](https://github.com/OpenCBM/OpenCBM) - Original firmware source
- [zyonee/opencbm](https://github.com/zyonee/opencbm) - Alternative firmware
- [spiro.trikaliotis.net](https://spiro.trikaliotis.net/opencbm) - OpenCBM tools

## Appendix: Pin Mappings

### IEC Bus (DIN-6)

```
    ┌───────┐
   /  1   2  \     1 = SRQ IN
  │           │    2 = GND
  │  3  4  5  │    3 = ATN
   \    6    /     4 = CLK
    └───────┘      5 = DATA
                   6 = RESET
```

### Arduino Pro Micro Pinout (XUM1541-II)

| Pro Micro Pin | Signal | Via 7406 |
|---------------|--------|----------|
| 2 | DATA | Yes |
| 3 | CLK | Yes |
| 4 | ATN | Yes |
| 5 | RESET | Yes |
| 6 | SRQ | No |

---

*Documentation version: UFT 4.1.1*
*Last updated: 2026-01-16*
