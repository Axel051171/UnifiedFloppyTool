# UFT TODO - Future Features

## ✅ COMPLETED - v5.27.0

### Recovery System (GOD MODE)
- [x] Flux-Level Recovery (Multi-Rev, PLL, RPM Drift, Histograms)
- [x] Bitstream Recovery (Slip, Hypotheses, Sync, Mixed-Encoding)
- [x] Track Recovery (Index, Length, Splice, Alignment)
- [x] Sector Recovery (Multi-Candidate, Ghost, Best-of-N)
- [x] Cross-Track Recovery (Comparison, Interleave, Side-to-Side)
- [x] Meta/Decision Layer (Source Tracking, Undo, Forensic Log)
- [x] Protection Handling (Preserve, Don't Fix)
- [x] User Control (Levels, Overrides, Forensic Lock)

### Integrated External Code
- [x] FluxEngine arch modules (Amiga, Apple, C64, IBM, Mac, Victor9k)
- [x] FluxEngine lib (fluxsource, fluxsink, decoders, encoders, vfs)
- [x] SAMdisk format handlers
- [x] Track extractors (MFM, GCR, FM, various platforms)
- [x] HFE/SCP/IPF/KryoFlux loaders

---

## 🔮 PHASE 2: FILESYSTEM LAYER (nach Format-Completion)

### Priority 1: Commodore CBM DOS
- [ ] D64/D71/D81 Directory Parser
- [ ] BAM (Block Allocation Map) Handler
- [ ] File Chain Following
- [ ] PRG/SEQ/USR/REL File Types
- [ ] Operations: list(), extract(), inject(), delete(), rename()
- [ ] Validate/Fix BAM

### Priority 2: AmigaDOS
- [ ] OFS (Old File System) Parser
- [ ] FFS (Fast File System) Parser
- [ ] Directory Blocks
- [ ] File Header/Data Blocks
- [ ] Bitmap Blocks
- [ ] Operations: list(), extract(), inject()

### Priority 3: FAT12/FAT16
- [ ] Boot Sector Parser
- [ ] FAT Table Handler
- [ ] Directory Entry Parser
- [ ] 8.3 Filename + LFN Support
- [ ] Cluster Chain Following
- [ ] Platforms: PC IMG, Atari ST, MSX

### Priority 4: Apple DOS
- [ ] DOS 3.3 VTOC/Catalog
- [ ] ProDOS Volume Directory
- [ ] File Types (A/B/T/I/R/S)
- [ ] Sparse Files Support

### Priority 5: Other Filesystems
- [ ] CP/M (various formats)
- [ ] TRSDOS / LDOS / NewDOS
- [ ] BBC DFS
- [ ] Atari DOS 2.x
- [ ] TI-99 Disk Manager

---

## 🔄 PHASE 3: CONVERSION LAYER

### Sector ↔ Flux
- [ ] D64 → G64 (add GCR encoding)
- [ ] G64 → D64 (decode GCR)
- [ ] ADF → HFE
- [ ] HFE → ADF
- [ ] Generic MFM encoder/decoder
- [ ] Generic GCR encoder/decoder

### Format Conversion
- [ ] D64 ↔ D71 ↔ D81
- [ ] ADF ↔ ADZ ↔ DMS
- [ ] DSK ↔ EDSK
- [ ] IMD ↔ TD0

---

## 🛠️ PHASE 4: TOOLS & CLI

### Command Line Interface
- [ ] `uft info <file>` - Show format details
- [ ] `uft list <image>` - Directory listing
- [ ] `uft extract <image> <file>` - Extract file
- [ ] `uft inject <image> <file>` - Add file
- [ ] `uft convert <src> <dst>` - Convert formats
- [ ] `uft verify <image>` - Validate image
- [ ] `uft fix <image>` - Repair image

### GUI (Qt)
- [ ] File browser with preview
- [ ] Drag & drop extract/inject
- [ ] Hex viewer
- [ ] Track/sector visualizer
- [ ] Flux waveform display

---

## 📝 NOTES

Created: 2026-01-02
Status: PHASE 1 (Format Parsers) - 382 formats complete
Next: Continue adding format parsers until comprehensive coverage
