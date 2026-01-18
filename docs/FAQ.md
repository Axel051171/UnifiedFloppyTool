# Frequently Asked Questions (FAQ)

---

## General Questions

### What is UnifiedFloppyTool?

UnifiedFloppyTool (UFT) is an open-source application for reading, writing, converting, and analyzing floppy disk images. It supports over 470 disk image formats and works with popular hardware controllers like Greaseweazle, KryoFlux, and SuperCard Pro.

### What platforms does UFT support?

- Windows 10/11 (64-bit)
- Linux (Ubuntu 20.04+, Debian 11+, Fedora 35+)
- macOS 11+ (Intel and Apple Silicon)

### Is UFT free?

Yes, UFT is free and open-source software released under the MIT license.

### What's the difference between sector images and flux images?

**Sector images** (ADF, D64, IMG):
- Store only decoded sector data
- Smaller file size
- Cannot preserve copy protection
- Easier to work with

**Flux images** (SCP, KF, MFM):
- Store raw magnetic flux transitions
- Larger file size
- Can preserve copy protection
- Required for accurate preservation

---

## Hardware Questions

### Do I need special hardware?

For **reading standard PC disks**: A USB floppy drive works fine.

For **reading other formats** (Amiga, C64, Apple, etc.): You need a flux-capable controller like Greaseweazle, KryoFlux, or SuperCard Pro.

### Which controller should I buy?

| Use Case | Recommendation |
|----------|----------------|
| Beginner, budget | Greaseweazle F7 ($30) |
| Professional preservation | KryoFlux ($100) |
| Multiple drives | SuperCard Pro ($100) |
| DIY enthusiast | FluxEngine ($15) |
| Commodore only | XUM1541 ($25) |

### Can I use any floppy drive?

Yes, but some drives work better:
- **3.5" HD**: Sony MPF920, Panasonic JU-257
- **5.25" DD**: Panasonic JU-475, TEAC FD-55
- **5.25" HD**: TEAC FD-55GFR

Avoid: Very old drives, laptop slim drives, cheap USB drives.

### My hardware isn't detected. What do I do?

1. Check USB connection
2. Try a different USB port
3. Linux: Add user to `dialout` group
4. Windows: Check Device Manager for COM port
5. Update controller firmware

---

## Format Questions

### What formats can UFT read?

Over 470 formats including:
- **PC**: IMG, IMA, DSK, IMD, TD0, CQM
- **Amiga**: ADF, ADZ, DMS, HDF, IPF
- **Commodore**: D64, D71, D81, G64, NIB, T64
- **Apple**: DSK, DO, PO, NIB, WOZ, 2IMG
- **Atari**: ATR, ATX, XFD, ST, MSA, STX
- **TRS-80**: DMK, JV1, JV3, IMD
- **Flux**: SCP, KF, HFE, A2R, MFM

### How do I know which format to use?

**For preservation**: Use flux format (SCP recommended)  
**For emulators**: Use native format (ADF for Amiga, D64 for C64, etc.)  
**For file transfer**: Convert to IMG or use filesystem extraction

### Can UFT read copy-protected disks?

Yes! UFT can:
- Detect common protection schemes
- Capture protected disks in flux format
- Preserve weak bits, long tracks, and timing variations
- Support over 50 known protection schemes

### What's the difference between DMK and IMG?

**DMK**: Raw track format that preserves FM/MFM encoding, sector headers, gaps, and CRCs. Used for TRS-80 and other systems.

**IMG**: Pure sector data with no track structure. Smaller but loses formatting information.

---

## Reading Disks

### How many revolutions should I capture?

| Disk Condition | Revolutions |
|----------------|-------------|
| Good disk | 2 |
| Older disk | 3-5 |
| Damaged disk | 5-10 |
| Protected disk | 3-5 |

More revolutions = better recovery but larger files.

### What does "CRC Error" mean?

The sector's checksum doesn't match. This means:
- Data corruption on the disk
- Read error
- Wrong format/encoding setting

Try: More retries, cleaning the drive heads, adjusting PLL settings.

### Why are some tracks empty?

Possible causes:
1. Disk is single-sided but reading both sides
2. Disk is damaged
3. Wrong density setting
4. Drive head alignment issue

### How do I read a disk with unknown format?

1. Use flux capture (SCP format)
2. Go to Tools → Analyze
3. UFT will detect encoding and format
4. Convert to appropriate format

---

## Writing Disks

### Can I write to floppy disks?

Yes, but with limitations:
- Need supported hardware (Greaseweazle, SCP, etc.)
- Disk must not be write-protected
- Drive must be capable of writing

### Why did write verification fail?

1. Disk is worn out - try fresh disk
2. Drive heads dirty - clean them
3. Wrong density - match disk to format
4. Write protect tab - check disk

### Can I recreate copy-protected disks?

For personal backup: Yes, use flux-to-flux copy.
For distribution: Check copyright laws in your jurisdiction.

---

## Conversion Questions

### How do I convert SCP to ADF?

**GUI:**
1. Tools → Convert
2. Select SCP file
3. Choose "ADF" output
4. Click Convert

**CLI:**
```bash
uft convert input.scp output.adf
```

### Why is my conversion failing?

Common reasons:
1. Source format not compatible with destination
2. Missing data (partial capture)
3. Copy protection preventing decode
4. Wrong source format specified

### Can I convert between any formats?

Most conversions work, but some lose data:
- Flux → Sector: Works, may lose protection
- Sector → Flux: Creates standard encoding only
- Protected → Unprotected: Loses protection data

---

## Analysis Questions

### What does the Flux Histogram show?

The histogram shows timing distribution of flux transitions:
- **Peaks**: Indicate bit cell timing
- **MFM**: 3 peaks (2T, 3T, 4T)
- **FM**: 2 peaks (1T, 2T)
- **GCR**: Variable pattern

### How do I detect copy protection?

1. Read disk with flux capture
2. Go to Tools → Analyze
3. Check "Protection Detection" section
4. Results show detected schemes

### What's the DMK Analyzer for?

Specifically for TRS-80 DMK images:
- Shows track structure
- Validates IDAM pointers
- Detects FM vs MFM
- Finds bad sectors

---

## Troubleshooting

### UFT won't start on Windows

1. Install Visual C++ Redistributable 2019
2. Check antivirus isn't blocking
3. Run as Administrator
4. Check log file in %APPDATA%\UnifiedFloppyTool\

### UFT won't start on Linux

```bash
# Check Qt dependencies
ldd ./UnifiedFloppyTool | grep "not found"

# Install missing libraries
sudo apt install libqt6widgets6 libqt6serialport6
```

### UFT shows "App is damaged" on macOS

```bash
# Remove quarantine attribute
xattr -cr /Applications/UnifiedFloppyTool.app

# Or right-click → Open → Open anyway
```

### Application crashes during read

1. Check log file for details
2. Reduce read speed
3. Update firmware
4. Try different USB port
5. Report issue with log file

---

## Legal Questions

### Is it legal to copy floppy disks?

**For personal backup**: Generally yes in most countries.

**For distribution**: Depends on copyright status of the software.

**For preservation**: Many archives operate under exemptions.

Consult local laws for your specific situation.

### Can I contribute to UFT?

Yes! See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

---

## Getting More Help

- **Documentation**: [docs/](../docs/)
- **GitHub Issues**: [Report bugs](https://github.com/Axel051171/UnifiedFloppyTool/issues)
- **Discussions**: [Ask questions](https://github.com/Axel051171/UnifiedFloppyTool/discussions)
- **Wiki**: [Community knowledge](https://github.com/Axel051171/UnifiedFloppyTool/wiki)

---

*Can't find your answer? Open a Discussion on GitHub!*
