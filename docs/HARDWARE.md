# Hardware Setup Guide

Complete guide to setting up floppy disk controllers with UnifiedFloppyTool.

---

## Quick Compatibility Matrix

| Controller | Read | Write | Flux | Backend | Price | Difficulty |
|------------|:----:|:-----:|:----:|:-------:|:-----:|:----------:|
| **Greaseweazle F7** | ✅ | ✅ | ✅ | native HAL | $30 | Easy |
| **KryoFlux** | ✅ | ✅ | ✅ | DTC subprocess | $100 | Medium |
| **SuperCard Pro** | ✅ | ✅ | ✅ | Qt + M3.1 partial HAL¹ | $100 | Medium |
| **FluxEngine** | ✅ | ✅ | ✅ | fluxengine subprocess | $15 | Medium |
| **Catweasel** | ✅ | ✅ | ✅ | (legacy / unmaintained) | Rare | Hard |
| **XUM1541** | ✅ | ✅ | ❌ | Qt + M3.2 partial HAL¹ | $25 | Easy |
| **ADF-Copy** | ✅ | ✅ | ⚠️ | (DIY adapter) | DIY | Easy |
| **FC5025** | ✅ | ❌ | ❌ | fcimage subprocess | $50 | Easy |
| **Applesauce** | ✅ | ✅ | ✅ | Qt + M3.3 partial HAL¹ | $80 | Easy |
| **USB Floppy** | ✅ | ✅ | ❌ | Qt provider | $15 | Easy |

¹ "Partial HAL" = pure-utility functions and lifecycle scaffold real,
USB/serial wiring still pending (see `docs/MASTER_PLAN.md` §M3 — M3.1
SCP-Direct libusb, M3.2 XUM1541 libusb, M3.3 Applesauce serial). Until
the wiring lands, hardware access goes through the existing Qt
provider (subprocess or QSerialPort), not the C HAL.

---

## Greaseweazle

### What You Need

- Greaseweazle F7 or F7 Plus board
- Floppy drive (3.5" or 5.25")
- 34-pin ribbon cable
- Floppy power cable (Berg connector)
- USB cable

### Physical Setup

```
┌─────────────────┐         ┌─────────────────┐
│   Greaseweazle  │◄────────│   Floppy Drive  │
│                 │ 34-pin  │                 │
│    [USB]────────┼─────────│   [Power]◄──────┤
└─────────────────┘         └─────────────────┘
      │                            │
      ▼                            ▼
    Computer                   Power Supply
```

**Pin 1 Alignment:**
- Look for notch/red stripe on cable
- Align with Pin 1 marking on both boards
- Wrong orientation won't damage hardware but won't work

### Drive Selection

**Single Drive:**
- Set drive to DS0 (Drive Select 0)
- Or use twisted cable (selects DS1 as DS0)

**Two Drives:**
- Drive A: DS0
- Drive B: DS1
- Use straight (non-twisted) cable

### Firmware Update

```bash
# Check current version
gw info

# Update to latest
gw update

# Or manually
gw update --file firmware-F7-v1.5.img
```

### UFT Configuration

1. **Hardware Tab** → Click "Detect Hardware"
2. Select "Greaseweazle" from dropdown
3. Configure:
   - **Port**: Auto-detected COM/tty port
   - **Drive**: A or B
   - **Type**: 3.5" HD, 5.25" DD, etc.

### Troubleshooting

| Problem | Solution |
|---------|----------|
| "No device found" | Check USB connection, try different port |
| "Permission denied" (Linux) | Add user to `dialout` group |
| "Drive not ready" | Check power, motor should spin |
| "No index" | Check ribbon cable orientation |
| **"Greaseweazle is in bootloader/update mode…"** | The board is sitting in its update firmware and cannot drive the floppy. See "Bootloader recovery" below. |
| **Windows: device enumerates but UFT can't talk to it** | Almost always the wrong COM port, the Realtek serial driver hijacking the port, or a stale `gw_tools` instance still holding the handle. See "Windows-specific" below. |

#### Bootloader recovery

UFT 4.1.3+ explicitly rejects the Greaseweazle bootloader because it
answers `GET_INFO` but cannot perform any disk operation (every
subsequent command would return a confusing `Protocol error`). When
`uft_gw_open()` returns `UFT_GW_ERR_BOOTLOADER` the device is alive
but running the wrong firmware image.

Two ways out:

1. **Recommended — reflash main firmware.**
   ```bash
   gw update path/to/greaseweazle-vN.M-fNN.upd
   ```
   `gw update` is the upstream Python tool from
   <https://github.com/keirf/greaseweazle>; the firmware `.upd` file
   ships with each release. After the flash completes the device
   reboots into the main firmware and UFT will detect it normally.

2. **Quick — power-cycle while holding the ID-0 button.**
   Disconnect USB, hold the small button labelled `0` (or `S1` on
   older PCBs) on the board, plug USB back in, release the button
   after ~2 s. The board boots into the main firmware. If you don't
   see the white LED come on, the firmware image is corrupted; use
   path 1.

If you reach this state by mistake (e.g. an aborted firmware upload),
neither the LED state nor the Windows Device Manager will tell you
which firmware is running — UFT's error message is the only clear
signal.

#### Windows-specific notes

- **Driver:** the Greaseweazle uses the in-box Microsoft "USB Serial
  Device" (usbser.sys) driver. No vendor driver needed. If Device
  Manager shows the device under "Other Devices" with a yellow
  triangle, install the Microsoft `serial.inf` from
  `%SystemRoot%\inf` (right-click → "Update driver" → "Browse my
  computer" → point at that path).
- **COM port collision:** Realtek "Bluetooth Serial Port" and many
  Lenovo / HP utilities also enumerate as COMx ports. UFT lists
  every COM device, not just GW boards. Pick the one that appeared
  *after* you plugged the GW in (Windows assigns the highest free
  number).
- **Port stuck in "in use":** if a previous run of `gw_tools` or an
  earlier UFT instance crashed, the COM port can be held by the
  zombie process. Symptoms: UFT reports `UFT_GW_ERR_OPEN_FAILED`
  with a `Win32 error 5` (Access Denied). Close the lingering
  process via Task Manager, or simply replug the device.
- **Power on USB hubs:** the GW F1 draws ~80 mA from USB; F7 draws
  ~140 mA. Some unpowered hubs sag below the threshold under load
  → device drops mid-read with `UFT_GW_ERR_TIMEOUT`. Use a powered
  hub or a direct port.

If after all of the above the device still won't read, capture a
debug log: launch UFT from a terminal (`UnifiedFloppyTool.exe` from
PowerShell, not via double-click) — every USB command and ACK is
printed. Attach that log to a GitHub issue.

---

## KryoFlux

### What You Need

- KryoFlux board
- USB cable
- Floppy drive
- 34-pin ribbon cable
- Floppy power

### Driver Installation

**Windows:**
1. Download DTC package from KryoFlux website
2. Run installer as Administrator
3. Connect KryoFlux - driver installs automatically

**Linux:**
```bash
# Create udev rule
sudo nano /etc/udev/rules.d/80-kryoflux.rules

# Add this line:
SUBSYSTEM=="usb", ATTR{idVendor}=="03eb", ATTR{idProduct}=="6124", MODE="0666"

# Reload rules
sudo udevadm control --reload-rules
```

**macOS:**
1. Install DTC package
2. Allow kernel extension in Security preferences
3. Restart

### UFT Configuration

1. **Hardware Tab** → Detect Hardware
2. Select "KryoFlux"
3. Set options:
   - **Output Directory**: Where to save raw streams
   - **Tracks**: 0-83 (or custom range)
   - **Side**: 0, 1, or both

### Raw Stream vs. Decoded

KryoFlux can output:
- **Raw Stream (.raw)**: Full flux data for archival
- **Decoded Images**: ADF, IMG, etc. via UFT

For preservation, always capture raw streams first.

---

## SuperCard Pro

### What You Need

- SuperCard Pro board
- USB cable
- Up to 2 floppy drives
- Ribbon cables
- Power (internal or Molex)

### Hardware Setup

SCP has TWO internal floppy connectors:

```
┌──────────────────────────────┐
│      SuperCard Pro           │
│  [Drive 0]  [Drive 1]  [USB] │
└──────────────────────────────┘
       │           │
       ▼           ▼
   3.5" HD     5.25" DD
```

### Driver Installation

**Windows:**
- SCP driver installs automatically via Windows Update
- Or download from SuperCard Pro website

**Linux/macOS:**
- No special driver needed (uses libusb)

### UFT Configuration

1. Hardware → Detect Hardware
2. Select "SuperCard Pro"
3. Choose drive (0 or 1)
4. Set parameters:
   - **Revolutions**: 2-5 recommended
   - **Bit Cell**: Auto or manual (e.g., 2µs for DD)

---

## FluxEngine

### What You Need

- Cypress FX2 development board (CY7C68013A)
- FluxEngine firmware
- Floppy drive
- 34-pin ribbon cable
- Logic level converter (if needed)

### Firmware Installation

```bash
# Install fluxengine tools
pip install fluxengine

# Flash firmware (only needed once)
fluxengine upgradefluxengine
```

### Compatible Boards

| Board | Price | Notes |
|-------|-------|-------|
| LCSOFT CY7C68013A | $8 | Most common, works well |
| Waveshare FX2 | $15 | Higher quality |
| Cypress DVK | $50 | Official development kit |

### UFT Configuration

1. Hardware → Detect Hardware
2. Select "FluxEngine"
3. May show as "Cypress FX2" initially

---

## ADF-Copy / ADF-Drive (Amiga)

### What You Need

- ADF-Copy or ADF-Drive board (Teensy 3.2 based, by Nick's Labor)
- Firmware v1.100 or later (platformio-based)
- PC 3.5" floppy drive
- 34-pin ribbon cable (Shugart header)
- USB-A to Micro-USB cable
- Stable 5V power supply for the floppy drive

### Hardware Setup

```
┌─────────────────┐         ┌─────────────────┐
│   ADF-Copy      │◄────────│   Floppy Drive  │
│   (Teensy 3.2)  │ 34-pin  │                 │
│    [USB]────────┼─────────│   [Power]◄──────┤
└─────────────────┘         └─────────────────┘
      │                            │
      ▼                            ▼
    Computer                   5V Power Supply
```

**Power Note:** The floppy drive needs stable 5V.
Recommended: buffer capacitor 800–1000 µF on drive power rail
(see Nick's schematic at https://nickslabor.niteto.de/projekte/adf-copy-english/)

### PCB Versions

| PCB Version | Read | Write | Flux Capture |
|-------------|:----:|:-----:|:------------:|
| v1 / v3 / v5 | ✅ | ✅ | ❌ |
| v2 / v4 / v6 | ✅ | ✅ | ✅ (~20 ns) |

**Flux capture** requires PCB v2/v4/v6 **and** a USB 2.0 host port (not USB 3.x).

### Firmware Update

- Download: https://nickslabor.niteto.de/download/
- Source: https://github.com/Niteto/ADF-Drive-Firmware
- Flash via PlatformIO or Teensy Loader

### Driver Installation

- **Windows 10/11:** Driver included in OS (automatic)
- **Windows XP–7:** https://www.pjrc.com/teensy/serial_install.exe
- **Linux/macOS:** No driver needed. On Linux, install udev rules:

```bash
sudo cp tools/99-floppy-devices.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### UFT Configuration

1. **Hardware Tab** → Click "Detect Hardware"
2. Select "ADF-Copy (Amiga)" from the Flux Controllers dropdown
3. The Teensy board is auto-detected via USB VID/PID (0x16C0:0x0483)
4. Configure:
   - **Port**: Auto-detected serial port
   - **Retries**: 3 (default, per track)
   - **Index Align**: Optional for write operations

### Troubleshooting

| Problem | Solution |
|---------|----------|
| "No device found" | Check USB cable, try different port |
| "Permission denied" (Linux) | Install udev rules or add user to `dialout` group |
| "Flux not supported" | PCB v1/3/5 cannot capture flux — use v2/4/6 |
| "Write-protected" | Check disk's write-protect tab |
| Unstable reads | Add 800–1000 µF capacitor on drive 5V rail |

---

## XUM1541 (Commodore Only)

### What You Need

- XUM1541 adapter
- IEC cable (6-pin DIN)
- Commodore drive (1541, 1571, 1581)
- USB cable

### Setup

```
┌─────────────┐     ┌─────────────────┐
│   XUM1541   │◄────│  Commodore 1541 │
│   [USB]     │ IEC │                 │
└─────────────┘     └─────────────────┘
```

### Driver Installation

**Windows:**
- Install OpenCBM package
- Includes XUM1541 drivers

**Linux:**
```bash
sudo apt install opencbm
sudo usermod -a -G plugdev $USER
```

### UFT Configuration

1. Hardware → Detect Hardware
2. Select "XUM1541 / OpenCBM"
3. Configure:
   - **Device Number**: 8-11 (usually 8)
   - **Transfer Mode**: Parallel (fastest) or Serial

---

## FC5025 (Device Side Data)

### What You Need

- FC5025 USB 5.25" floppy controller
- 5.25" floppy drive
- 34-pin ribbon cable
- USB cable
- Power supply for drive

### Overview

The FC5025 is a **read-only** USB 5.25" floppy controller by Device Side Data.

- **USB VID/PID**: `0x16C0:0x06D6`
- **Supported formats**: 18 disk formats including Apple DOS 3.2/3.3, C64 (1541), IBM PC (FM/MFM), TRS-80, Atari 810, North Star, Commodore 8050, and more
- **No flux capture** — sector-level reads only
- **Forensic limitation**: No timing data, no copy protection preservation

### Driver Installation

**Windows / macOS:**
- Download `fcimage` CLI tool from Device Side Data website

**Linux:**
- Uses libusb — no kernel driver needed
- Install udev rule for non-root access:

```bash
sudo cp tools/99-floppy-devices.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### UFT Configuration

1. **Hardware Tab** → Click "Detect Hardware"
2. Select "FC5025" from dropdown
3. Configure:
   - **Port**: Auto-detected USB device
   - **Format**: Select target disk format from 18 supported types
   - **Drive**: Single drive only (read-only)

### Troubleshooting

| Problem | Solution |
|---------|----------|
| "No device found" | Check USB connection, verify VID:PID with `lsusb` |
| "Permission denied" (Linux) | Install udev rules or run as root |
| Unreadable sectors | Clean drive heads, try different drive speed |
| Wrong format detected | Manually select correct format from list |

**Website:** http://deviceside.com/fc5025.html

---

## Applesauce (Evolution Interactive)

### What You Need

- Applesauce or Applesauce+ controller
- Apple II / Macintosh compatible floppy drive (5.25" or 3.5")
- USB cable
- Power supply for drive

### Overview

Applesauce is an Apple II / Macintosh specialized flux controller by Evolution Interactive.

- **USB VID/PID**: `0x16C0:0x0483`
- **Protocol**: Text-based USB serial (human-readable commands)
- **Supported drives**: 5.25", 3.5", and PC drives
- **Applesauce+ variant**: 420K buffer, 100+ Mbps transfer speed
- **Full flux read/write**, RPM measurement, write-protect detection
- **Output formats**: A2R, WOZ, MOOF

### Driver Installation

**Windows:**
- Detected as USB serial device, driver installs automatically

**Linux:**
- No driver needed. Install udev rule for non-root access:

```bash
sudo cp tools/99-floppy-devices.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**macOS:**
- No driver needed — native USB serial support

### UFT Configuration

1. **Hardware Tab** → Click "Detect Hardware"
2. Select "Applesauce" from dropdown
3. Configure:
   - **Port**: Auto-detected serial port
   - **Drive Type**: 5.25", 3.5", or PC
   - **Output Format**: A2R, WOZ, or MOOF

### Troubleshooting

| Problem | Solution |
|---------|----------|
| "No device found" | Check USB connection, try different port |
| "Permission denied" (Linux) | Install udev rules or add user to `dialout` group |
| Slow transfers | Use Applesauce+ for 100+ Mbps speeds |
| Write failures | Check write-protect detection, verify drive compatibility |

**Website:** https://applesaucefdc.com/
**Wiki:** https://wiki.applesaucefdc.com/

---

## USB Floppy Drives

### Limitations

- Only standard PC formats (720K, 1.44M)
- No flux capture
- No copy protection preservation
- Limited write reliability

### When to Use

- Quick reads of standard PC disks
- Format conversions
- Testing

### Compatibility

| Drive | Read | Write |
|-------|:----:|:-----:|
| Most USB 3.5" | ✅ | ✅ |
| USB 5.25" | ✅ | ⚠️ |
| USB slim drives | ✅ | ⚠️ |

---

## Drive Selection Guide

### 3.5" Drives

| Drive | Type | Good For |
|-------|------|----------|
| **Sony MPF920** | HD | Best overall, reliable |
| **Panasonic JU-257** | HD | Common, good quality |
| **TEAC FD-235** | HD | Industrial grade |
| **Alps DF354** | HD | Mac compatible |

### 5.25" Drives

| Drive | Type | Good For |
|-------|------|----------|
| **TEAC FD-55GFR** | HD | Best for PC 1.2M |
| **Panasonic JU-475** | DD | Good for C64/Apple |
| **Tandon TM100** | DD | Classic, common |

### 8" Drives

| Drive | Good For |
|-------|----------|
| **Shugart SA800** | Classic CP/M systems |
| **Siemens FDD 100-8** | Industrial systems |

---

## Power Considerations

### Floppy Power Connector

```
Berg (Small) 4-pin:
┌─────────────────┐
│ [+5V] [GND] [GND] [+12V] │
└─────────────────┘
  Red   Blk   Blk   Yellow
```

### Power Sources

1. **ATX Power Supply** (recommended)
   - Use Molex-to-Berg adapter
   - Paperclip trick to start without motherboard

2. **Dedicated 5V/12V Supply**
   - Bench power supply
   - Old laptop charger (5V only for some 3.5")

3. **USB Power** (limited)
   - Some 3.5" drives work on 5V only
   - Not recommended for reliability

---

## Maintenance

### Drive Head Cleaning

1. Use isopropyl alcohol (90%+)
2. Apply to cleaning disk or Q-tip
3. Gently clean heads
4. Let dry before use

### Belt Replacement (5.25" drives)

1. Open drive casing
2. Locate spindle belt
3. Replace with correct size
4. Reassemble

### Alignment Check

1. Use alignment disk
2. Check track 0 sensor
3. Adjust if needed (rare)

---

## Safety Notes

- Always handle disks by edges
- Store disks away from magnets
- Keep drives clean and dust-free
- Use ESD precautions when handling boards
- Don't force connections

---

*For more help, visit the [Discussions](https://github.com/Axel051171/UnifiedFloppyTool/discussions)*
