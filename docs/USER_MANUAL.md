# UnifiedFloppyTool User Manual

**Version 4.0.0** | Last Updated: January 2026

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Hardware Setup](#hardware-setup)
3. [Reading Disks](#reading-disks)
4. [Writing Disks](#writing-disks)
5. [Converting Formats](#converting-formats)
6. [Analysis Tools](#analysis-tools)
7. [Command Line Interface](#command-line-interface)
8. [Troubleshooting](#troubleshooting)

---

## Getting Started

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Windows 10, Ubuntu 20.04, macOS 11 | Latest version |
| RAM | 4 GB | 8 GB |
| Storage | 100 MB | 1 GB (for disk images) |
| USB | 2.0 | 3.0 |

### Installation

#### Windows
1. Download `UnifiedFloppyTool-4.0.0-win64.zip`
2. Extract to desired location
3. Run `UnifiedFloppyTool.exe`

#### Linux
```bash
# Option 1: AppImage (recommended)
chmod +x UnifiedFloppyTool-4.0.0-x86_64.AppImage
./UnifiedFloppyTool-4.0.0-x86_64.AppImage

# Option 2: Debian package
sudo dpkg -i unifiedfloppytool_4.0.0_amd64.deb

# Option 3: From tarball
tar xzf UnifiedFloppyTool-4.0.0-linux-x64.tar.gz
cd UnifiedFloppyTool-4.0.0-linux-x64
./UnifiedFloppyTool.sh
```

#### macOS
1. Download `UnifiedFloppyTool-4.0.0-macos.dmg`
2. Drag to Applications folder
3. First run: Right-click → Open (to bypass Gatekeeper)

### First Launch

When you start UFT for the first time:

1. **Hardware Detection**: UFT scans for connected controllers
2. **Welcome Screen**: Shows available hardware and quick actions
3. **Configuration**: Settings are saved in `~/.config/UnifiedFloppyTool/`

---

## Hardware Setup

### Greaseweazle

The recommended controller for most users.

**Connection:**
1. Connect Greaseweazle to USB port
2. Connect floppy drive ribbon cable (Pin 1 to Pin 1)
3. Connect drive power (Berg connector)

**Configuration in UFT:**
1. Go to **Hardware** tab
2. Click **Detect Hardware**
3. Select your Greaseweazle from the dropdown
4. Configure drive type (3.5" HD, 5.25" DD, etc.)

**Firmware Update:**
```bash
# Linux
gw update

# Or use UFT's built-in updater in Hardware tab
```

### KryoFlux

Professional-grade flux capture device.

**Driver Installation:**
- Windows: Install DTC driver from KryoFlux package
- Linux: Add udev rules for non-root access
- macOS: Use included kernel extension

**UFT Configuration:**
1. Hardware tab → Detect Hardware
2. Select "KryoFlux" 
3. Set output directory for raw flux files

### SuperCard Pro

High-end flux capture with dual-drive support.

**Setup:**
1. Install SCP USB driver
2. Connect drives to internal connectors
3. In UFT: Hardware → Detect → SuperCard Pro

### FluxEngine

Open-source alternative using Cypress FX2 hardware.

**Supported Boards:**
- Cypress CY7C68013A development boards
- FluxEngine-specific PCBs

---

## Reading Disks

### Quick Read

1. Insert floppy disk in drive
2. Go to **Workflow** tab
3. Set **Source**: Your hardware device
4. Set **Destination**: File path for output
5. Select output format (SCP, ADF, IMG, etc.)
6. Click **▶ START**

### Read Options

| Option | Description |
|--------|-------------|
| **Revolutions** | Number of disk rotations to capture (1-10) |
| **Retry Count** | Retries for bad sectors (0-20) |
| **Verify** | Read-after-write verification |
| **Side** | 0, 1, or Both |
| **Tracks** | Range (e.g., 0-79) |

### Format Auto-Detection

UFT automatically detects:
- Disk encoding (MFM, FM, GCR)
- Sector layout
- Platform (Amiga, C64, PC, etc.)
- Copy protection

### Handling Protected Disks

For copy-protected disks:

1. Use **Flux** output format (SCP, KF)
2. Enable **Multi-revolution** capture (3+ revolutions)
3. Check **Preserve weak bits** option

---

## Writing Disks

### Quick Write

1. Go to **Workflow** tab
2. Set **Source**: Disk image file
3. Set **Destination**: Hardware device
4. Click **▶ START**

### Write Options

| Option | Description |
|--------|-------------|
| **Verify** | Read back and compare after write |
| **Erase First** | Bulk erase before writing |
| **Write Precompensation** | Timing adjustment for inner tracks |

### Supported Write Formats

| Format | Hardware Required |
|--------|-------------------|
| ADF (Amiga) | Greaseweazle, SCP, FluxEngine |
| D64 (C64) | Greaseweazle, XUM1541 |
| IMG (PC) | Any controller, USB floppy |
| DMK (TRS-80) | Greaseweazle, SCP |

---

## Converting Formats

### GUI Conversion

1. Go to **Tools** tab
2. Click **Convert**
3. Select source file
4. Choose output format
5. Click **Convert**

### Batch Conversion

1. Tools → Batch Convert
2. Add multiple files
3. Select output format
4. Set output directory
5. Click **Start Batch**

### Common Conversions

| From | To | Notes |
|------|-----|-------|
| SCP | ADF | Decodes Amiga flux to sector image |
| SCP | D64 | Decodes C64 GCR to sector image |
| ADF | SCP | Re-encodes for flux-level backup |
| IMG | HFE | Creates HxC emulator image |
| DMK | IMG | Extracts sector data from raw track |

---

## Analysis Tools

### DMK Analyzer (Status Tab → 🔎 DMK Analysis)

Analyzes TRS-80 DMK disk images:

- Track-by-track sector map
- IDAM pointer validation
- FM/MFM encoding detection
- CRC verification
- Export to raw sector image

### Flux Histogram (Workflow Tab → 📊 Histogram)

Visualizes flux timing distribution:

- Real-time histogram display
- Automatic encoding detection
- Cell time measurement
- Peak identification
- CSV export for analysis

### GW→DMK Direct (Settings Tab → 🔧 GW→DMK Panel)

Direct Greaseweazle to DMK conversion:

- TRS-80 Model I/III/4 presets
- Real-time decode progress
- Automatic track format detection

### Forensic Report (Tools → Generate Report)

Creates detailed HTML/JSON reports:

- Disk geometry summary
- Sector-by-sector analysis
- Error map
- SHA-256 checksums
- Copy protection detection

---

## Command Line Interface

### Basic Commands

```bash
# Read disk to image
uft read -d greaseweazle -o output.scp

# Convert between formats
uft convert input.scp output.adf

# Analyze disk image
uft analyze disk.dmk

# Check disk integrity
uft check disk.d64

# View flux histogram
uft hist disk.scp --track 0

# List supported formats
uft formats

# Show hardware
uft devices
```

### Advanced Options

```bash
# Read specific tracks
uft read -d gw -o disk.scp --tracks 0-39 --side 0

# Multi-revolution capture
uft read -d gw -o disk.scp --revs 5

# Convert with specific parameters
uft convert input.scp output.adf --encoding mfm --sectors 11

# Batch convert directory
uft batch-convert ./input/*.scp --format adf --output ./output/
```

### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid arguments |
| 3 | Hardware error |
| 4 | Read error |
| 5 | Write error |
| 6 | Format error |

---

## Troubleshooting

### Hardware Not Detected

**Greaseweazle:**
```bash
# Linux: Check USB permissions
sudo usermod -a -G dialout $USER
# Then log out and back in

# Check device
ls -la /dev/ttyACM*
```

**Windows:**
- Install correct COM port driver
- Check Device Manager for COM port number

### Read Errors

| Error | Cause | Solution |
|-------|-------|----------|
| CRC Error | Damaged sector | Increase retry count |
| No Sync | Wrong encoding | Check format auto-detect |
| Track Empty | Drive issue | Clean heads, check cable |
| Timeout | Communication error | Reduce RPM, check USB |

### Write Verification Failed

1. Check disk is not write-protected
2. Verify drive can write (clean heads)
3. Try lower write speed
4. Use fresh disk

### Application Crashes

1. Check log file: `~/.config/UnifiedFloppyTool/uft.log`
2. Update to latest version
3. Report issue with:
   - Operating system
   - Hardware controller
   - Steps to reproduce
   - Log file contents

### Format Not Supported

1. Check file extension matches format
2. Try format auto-detection
3. Consult [Format Reference](FORMATS.md)
4. Request new format via GitHub Issues

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+R | Start read/write |
| Ctrl+H | Detect hardware |
| Ctrl+Q | Quit |
| F1 | Help |
| F5 | Refresh |

---

## Getting Help

- **Documentation**: https://github.com/Axel051171/UnifiedFloppyTool/wiki
- **Issues**: https://github.com/Axel051171/UnifiedFloppyTool/issues
- **Discussions**: https://github.com/Axel051171/UnifiedFloppyTool/discussions

---

*"Bei uns geht kein Bit verloren"*
