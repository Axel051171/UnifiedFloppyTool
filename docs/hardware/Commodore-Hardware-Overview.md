# Commodore Hardware Support in UFT

## Overview

UFT supports multiple hardware adapters for reading and writing Commodore floppy disks. This guide covers all supported hardware and their capabilities.

## Supported Hardware Adapters

### USB-Based Adapters

| Device | Interface | Speed | Copy Protection | Price |
|--------|-----------|-------|-----------------|-------|
| **ZoomFloppy** | USB | Fast | With nibtools | ~$40 |
| **XUM1541-II** | USB | Fast | With nibtools | DIY |
| **XUM1541** | USB | Medium | Limited | DIY |

### Flux-Level Devices

| Device | Interface | Speed | Copy Protection | Notes |
|--------|-----------|-------|-----------------|-------|
| **Greaseweazle** | USB | Variable | Excellent | Universal flux reader |
| **FluxEngine** | USB | Variable | Excellent | Alternative flux reader |
| **Kryoflux** | USB | Variable | Excellent | Commercial, archived |

## Supported Commodore Drives

### IEC Serial Bus Drives

| Drive | Format | Capacity | Notes |
|-------|--------|----------|-------|
| **1541** | GCR | 170 KB | Most common |
| **1541-II** | GCR | 170 KB | Improved design |
| **1571** | GCR | 340 KB | Double-sided |
| **1581** | MFM | 800 KB | 3.5" disk |

### CMD Drives

| Drive | Format | Capacity | Notes |
|-------|--------|----------|-------|
| **FD2000** | MFM | 720KB/1.44MB | DD/HD support |
| **FD4000** | MFM | 720KB-2.88MB | DD/HD/ED support |

### IEEE-488 Drives

| Drive | Format | Capacity | Notes |
|-------|--------|----------|-------|
| **2031** | GCR | 170 KB | Single drive |
| **4040** | GCR | 340 KB | Dual drive |
| **8050** | GCR | 1 MB | Dual drive |
| **8250** | GCR | 2 MB | Dual drive |
| **SFD-1001** | GCR | 1 MB | Single drive |

## Connection Diagrams

### IEC Serial Bus

```
Computer ─────┬───── 1541
              │
              ├───── 1571
              │
              └───── 1581

XUM1541/ZoomFloppy connects here via USB
```

### IEEE-488 Bus

```
Computer ─────┬───── 8050
              │
              └───── 8250

ZoomFloppy+IEEE connects here
```

## Hardware Profiles in UFT

### Viewing Profiles

```bash
# List all hardware profiles
uft --list-profiles

# Show specific profile
uft --show-profile xum1541-ii
```

### Profile Selection

UFT auto-detects hardware, but you can force a profile:

```bash
# Force XUM1541-II profile
uft read --profile xum1541-ii --drive 8 --output disk.d64
```

## Recommended Configurations

### For Archiving (Best Quality)

```
Hardware:     Greaseweazle or FluxEngine
Output:       .SCP or .RAW flux files
Convert to:   .G64 for preservation
```

### For Daily Use (Fast Transfers)

```
Hardware:     ZoomFloppy or XUM1541-II with parallel
Output:       .D64, .D71, .D81
Speed:        ~30 seconds/disk
```

### For Copy Protection

```
Hardware:     XUM1541 + nibtools OR Flux device
Output:       .G64 (GCR) or .NIB
Note:         Some protections need flux-level capture
```

## Comparison Table

| Feature | ZoomFloppy | XUM1541-II | Greaseweazle |
|---------|------------|------------|--------------|
| D64 Read/Write | ✅ | ✅ | ✅ |
| G64 Read/Write | ✅ | ✅ | ✅ |
| Flux Capture | ❌ | ❌ | ✅ |
| Copy Protection | Partial | Partial | Full |
| IEEE-488 | Optional | ❌ | ❌ |
| Price | $40 | DIY | ~$30 |
| Ease of Use | Easy | Medium | Medium |

## Quick Start by Hardware

### ZoomFloppy

1. Install OpenCBM: `opencbm-install`
2. Connect drive, power on
3. `uft read --device zoomfloppy --drive 8 -o disk.d64`

### XUM1541-II

1. Build hardware from [tebl's repository](https://github.com/tebl/C64-XUM1541-II)
2. Flash firmware: `xum1541-PROMICRO_7406-v08.hex`
3. Install driver with Zadig
4. `uft read --device xum1541 --drive 8 -o disk.d64`

### Greaseweazle

1. Flash Greaseweazle firmware
2. Connect to drive power/data
3. `uft read --device greaseweazle --drive 0 -o disk.scp`
4. Convert: `uft convert disk.scp disk.g64`

---

*Documentation version: UFT 4.1.1*
