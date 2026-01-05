# BATCH 7 Integration Summary

## Directly Integrated Files

### 1. SCP Tool (C99)
- `include/uft/uft_scp.h` - SCP format reader header
- `src/formats/scp/uft_scp.c` - SCP implementation
- `src/formats/scp/uft_scp_tool.c` - CLI tool

**Features:**
- Clean SCP file parsing
- Extended mode support (flag 0x40)
- Flux transition extraction
- JSON catalog export for GUI
- Strict bounds checking

### 2. Sector Parser
- `include/uft/floppy_sector_parser.h` - Parser header
- `src/parsers/floppy_sector_parser.c` - Implementation

**Features:**
- IBM FM/MFM sector parsing
- CRC16-CCITT validation
- Detailed status flags
- Duplicate detection
- Mark mask support

### 3. Qt GUI Framework
- `include/uft/uft_gui_kryoflux_style.h` - Complete Qt GUI header

**Components:**
- `TrackGridWidget` - 84×2 track grid with states
- `LEDWidget` / `LEDGroupWidget` - Status LEDs
- `ControlPanel` - Start/Stop, output selection
- `TrackInfoPanel` - Tabbed info (Basic/Advanced/Plots)
- `HistogramPlot`, `ScatterPlot`, `DensityPlot` - Visualizations
- `SettingsDialog` - Profile management
- `MainWindow` - Complete layout

## KryoFlux GUI Concepts Adopted

### Layout Structure
```
┌────────────────────────────────────────┐
│ [Menu Bar]                             │
├─────────────────────┬──────────────────┤
│                     │                  │
│   TRACK GRID        │  CONTROL PANEL   │
│   (84 × 2 cells)    │  - LED Status    │
│                     │  - Start/Stop    │
│                     │  - Output Select │
├─────────────────────┴──────────────────┤
│ [Track | Advanced | Histogram | ...]   │
│              INFO TABS                 │
├────────────────────────────────────────┤
│ [Status Bar]                           │
└────────────────────────────────────────┘
```

### Cell States
| State    | Color  | Description |
|----------|--------|-------------|
| Unknown  | Gray   | Not yet read |
| Good     | Green  | All OK |
| Bad      | Red    | Errors |
| Modified | Yellow | Changed |
| Reading  | Blue   | In progress |

### Status Flags
- P: Protection detected
- N: Sector not in image
- X: Decoding stopped
- H: Hidden header data
- I: Non-standard format
- T/S: Wrong track/side
- B: Sector out of range
- L: Non-standard length
- Z: Illegal offset
- C: Unchecked checksum

## API Quick Reference

### SCP Reading
```c
uft_scp_image_t img;
uft_scp_open(&img, "disk.scp");

uft_scp_track_info_t info;
uft_scp_get_track_info(&img, track_idx, &info);

uint32_t transitions[200000];
size_t count;
uft_scp_read_rev_transitions(&img, track_idx, rev_idx,
                             transitions, 200000, &count, NULL);

uft_scp_close(&img);
```

### Sector Parsing
```c
fps_config_t cfg = {
    .encoding = FPS_ENC_MFM,
    .max_sectors = 20,
    .max_search_gap = 100
};

fps_sector_t sectors[20];
uint8_t data_bufs[20][512];
for (int i = 0; i < 20; i++) {
    sectors[i].data = data_bufs[i];
    sectors[i].data_capacity = 512;
}

fps_result_t result;
fps_parse_track(&cfg, track_bytes, track_len, sectors, 20, &result);

printf("Found %d sectors, %d good\n", 
       result.sectors_found, result.sectors_with_data);
```

## Documentation

- `docs/ARCHIVE_ANALYSIS_BATCH7_SPECIAL.md` - Full analysis
- This file - Integration summary

## Next Steps

1. Implement TrackGridWidget in Qt
2. Connect SCP reader to GUI
3. Add histogram plot from flux timings
4. Create profile management dialog
5. Implement settings persistence

---

## Additional KryoFlux Extraction (from JAR + DTC Binary)

### From kryoflux-ui.jar (com.kryoflux.dtc package)

**Stream Decoder Components:**
- `StreamDecoder` - Main stream decoding class
- `CStreamDecoder` - C-Bridge to native code
- `CellBuffer` - Decoded cell storage (position, timing tuples)
- `CellIndex` - Cell position with RPM
- `StreamIndex` - Index marker positions
- `CellStat` - Statistics (avgbps, avgdrift, avgfr, avgrpm)
- `Hist` - Histogram for timing analysis

**OOB Message Types:**
- `c2otStreamRead` (0x01) - Stream data
- `c2otIndex` (0x02) - Index signal
- `c2otStreamEnd` (0x03) - Stream end
- `c2otInfo` (0x04) - Info string
- `c2otEnd` (0x0D) - End marker

**Error Codes (kfe$):**
- kfeOk, kfeCellBadRPM, kfeCellMissingIndex
- kfeStrDevBuffer, kfeStrDevIndex, kfeStrInvalidOOB
- kfeStrMissingData, kfeStrMissingEnd, kfeStrTransfer

### From dtc Binary (CDiskEncoding class)

**FM Encoding:**
- fmcode, fmdecode, fminit

**MFM Encoding:**
- mfmcode, mfmdecode, mfminit, mfmcodebit

**GCR C64/CBM:**
- gcrcode, gcrdecode (standard)
- gcrcode_s, gcrdecode_s (short)

**GCR Apple:**
- gcrahcode/decode (header)
- gcra5code/decode (5.25")
- gcra6code/decode (6-and-2)

**GCR Copy Protection:**
- Vorpal: gcrvorpalcode/decode
- Vorpal2: gcrvorpal2code/decode
- V-Max: gcrvmaxcode/decode
- BigFive, Ozisoft, Teque variants

**GCR 4-bit:**
- gcr4bitcode, gcr4bitdecode

### New Header File

`include/uft/uft_kryoflux_algorithms.h` (420+ lines)

Contains:
- All error codes (uft_kfe_error_t)
- OOB message types and structures
- Cell statistics structure
- Histogram structure
- Timing constants (24027428.57 Hz sample clock)
- All encoding type enums
- DTC image format types (i0-i64)
- DTC command line option structure
- Firmware command enums
- Utility functions (tick/RPM conversion)

### Value for UFT

1. **Error Handling** - Complete error code mapping
2. **Stream Format** - Full OOB protocol understanding
3. **Timing** - Exact clock frequencies
4. **Encoding Types** - All supported GCR variants
5. **Protection Schemes** - Vorpal, V-Max, BigFive, etc.
6. **Format Detection** - Image type enumeration

---

## Amiga Tools Integration

### XCopy Pro Source Analysis

**Copy Modes extracted from xcopy.i:**

| Mode | Value | Description | GUI Name |
|------|-------|-------------|----------|
| DOSCOPY | 0 | Standard DOS sector copy | DosCopy |
| BAMCOPY | 1 | BAM-aware copy (skips empty) | BamCopy+ |
| DOSPLUS | 2 | DOS copy + verify | DosCopy+ |
| NIBBLE | 3 | Raw track copy | Nibble |
| OPTIMIZE | 4 | Disk optimizer | Optimize |
| FORMAT | 5 | Full format | Format |
| QFORMAT | 6 | Quick format | Quick Format |
| ERASE | 7 | Erase disk | Erase |
| MESLEN | 8 | Speed check | Speed Check |
| NAME | 9 | Disk name | Disk Name |
| DIR | 10 | Directory listing | Directory |
| CHECK | 11 | Verify disk | Verify |
| INSTALL | 12 | Install bootblock | Install Boot |

**Drive Selection (bitmask):**
- 0x01 = DF0:
- 0x02 = DF1:
- 0x04 = DF2:
- 0x08 = DF3:

**Sync Words:**
- 0x4489 = Standard Amiga MFM
- 0xF8BC = Index sync (for raw mode)

### DiskSalv 4 Stream Format

Block types for recovery archives:
- ROOT (0x524F4F54) - Archive root
- UDIR (0x55444952) - User directory
- FILE (0x46494C45) - File header
- DATA (0x44415441) - File data
- DLNK/FLNK/SLNK - Hard/Soft links
- ERRS (0x45525253) - Error record
- ENDA (0x454E4441) - End of archive

### XVS Library API

**Bootblock Types:**
- UNKNOWN (0) - Unknown bootblock
- NOTDOS (1) - Not DOS bootblock
- STANDARD13 (2) - Kickstart 1.3 style
- STANDARD20 (3) - Kickstart 2.0+ style
- VIRUS (4) - Virus detected!
- UNINSTALLED (5) - No bootblock

**File Types:**
- EMPTY (1) - Empty file
- DATA (2) - Data file
- EXE (3) - Executable
- DATAVIRUS (4) - Data virus (delete only)
- FILEVIRUS (5) - File virus (delete only)
- LINKVIRUS (6) - Link virus (can repair)

### New Files Created

| File | Lines | Content |
|------|-------|---------|
| `uft_amiga_modes.h` | 480+ | All C structures and enums |
| `uft_amiga_gui.h` | 350+ | Qt GUI widgets |

### GUI Components

**CopyModeWidget** - Radio buttons for XCopy modes
**DriveSelectWidget** - Drive checkboxes (DF0:-DF3:)
**TrackRangeWidget** - Start/End track, side selection
**CopyOptionsWidget** - Verify, virus scan, retries, sync
**AmigaPanel** - Combined main panel

### Integration with UFT GUI

The Amiga panel can be added as a new tab in the main window:

```cpp
// In MainWindow constructor
auto* amigaPanel = new UFT::Amiga::AmigaPanel(this);
m_tabWidget->addTab(amigaPanel, tr("Amiga"));

connect(amigaPanel, &UFT::Amiga::AmigaPanel::startOperation,
        this, &MainWindow::onAmigaStart);
```

### Reference Files

Original Amiga source files preserved in:
`docs/reference/amiga/`
- xcopy.i (XCopy defines)
- ds_stream.h (DiskSalv format)
- xvs.h (Virus scanner API)
