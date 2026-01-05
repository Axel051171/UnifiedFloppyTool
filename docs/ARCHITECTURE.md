# UFT Architecture

## Overview

UnifiedFloppyTool (UFT) is a modular, cross-platform floppy disk preservation and analysis tool.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              Qt6 GUI                                    │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ │
│  │  MainWin  │ │ TrackView │ │ FluxViz   │ │ Settings  │ │ FsBrowser │ │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ │
└────────┼─────────────┼─────────────┼─────────────┼─────────────┼───────┘
         │             │             │             │             │
         └─────────────┴──────┬──────┴─────────────┴─────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────────┐
│                          Core API Layer                                  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐        │
│  │ Session Mgr │ │ Format Reg  │ │ Error Codes │ │  Presets    │        │
│  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘        │
└─────────┼───────────────┼───────────────┼───────────────┼───────────────┘
          │               │               │               │
┌─────────▼───────────────▼───────────────▼───────────────▼───────────────┐
│                          Processing Pipeline                             │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   │
│  │ Acquire │ → │ Decode  │ → │Normalize│ → │ Export  │ → │ Verify  │   │
│  └────┬────┘   └────┬────┘   └────┬────┘   └────┬────┘   └────┬────┘   │
└───────┼─────────────┼─────────────┼─────────────┼─────────────┼─────────┘
        │             │             │             │             │
┌───────▼─────────────▼─────────────▼─────────────▼─────────────▼─────────┐
│                        Format Handler Layer                              │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐       │
│  │ D64 │ │ ADF │ │ ATR │ │ WOZ │ │ SCP │ │ HFE │ │ IMD │ │ ... │       │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘       │
└─────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────────┘
      │       │       │       │       │       │       │       │
┌─────▼───────▼───────▼───────▼───────▼───────▼───────▼───────▼───────────┐
│                     Hardware Abstraction Layer                           │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐            │
│  │Greaseweazle│ │  KryoFlux  │ │ FluxEngine │ │   FC5025   │            │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘            │
└─────────────────────────────────────────────────────────────────────────┘
```

## Module Hierarchy

### Layer 1: GUI (Qt6)
- **MainWindow**: Application shell, menu, toolbar
- **TrackView**: Sector visualization, heatmaps
- **FluxVisualizer**: Waveform display, timing analysis
- **SettingsDialog**: Parameter configuration
- **FilesystemBrowser**: Native FS exploration (D64, ADF, etc.)

### Layer 2: Core API
- **Session Manager**: Project state, undo/redo
- **Format Registry**: Auto-detection, format metadata
- **Error Codes**: 80 codes across 8 categories
- **Preset System**: 52 presets, 12 categories

### Layer 3: Processing Pipeline

```
Acquire → Decode → Normalize → Export → Verify
   │         │         │          │        │
   │         │         │          │        └── Hash verification
   │         │         │          └── Write to target format
   │         │         └── Sector/track normalization
   │         └── Flux→Bits→Sectors decoding
   └── Read from file/hardware
```

### Layer 4: Format Handlers

Each format implements:
```c
typedef struct uft_format_handler {
    const char *name;
    const char *extension;
    
    bool (*probe)(const uint8_t *data, size_t size);
    int  (*read)(uft_disk_t *disk, FILE *fp);
    int  (*write)(const uft_disk_t *disk, FILE *fp);
    void (*free)(void *ctx);
} uft_format_handler_t;
```

### Layer 5: Hardware Abstraction

```c
typedef struct uft_hw_controller {
    const char *name;
    
    int  (*open)(const char *device);
    void (*close)(void);
    int  (*read_track)(int track, int side, uft_raw_track_t *out);
    int  (*write_track)(int track, int side, const uft_raw_track_t *in);
} uft_hw_controller_t;
```

## Data Flow

### Read Operation

```
[Hardware/File] → uft_raw_track_t → PLL → uft_track_t → uft_sector_t → [Filesystem]
                      │                      │               │
                      │                      │               └── Sector data
                      │                      └── Decoded bits, sync marks
                      └── Raw flux/bitstream samples
```

### Write Operation

```
[Filesystem] → uft_sector_t → Encoder → uft_track_t → uft_raw_track_t → [Hardware/File]
```

## Key Data Structures

### uft_disk_t
Top-level container for a complete disk image.

### uft_track_t
Single track with decoded sectors, sync patterns, gap info.

### uft_sector_t  
Individual sector: header (C/H/R/N), data, CRC status.

### uft_raw_track_t
Hub format for flux/bitstream data, used for N×M converter optimization.

## Error Handling

All functions return `int` status:
- `UFT_OK (0)` - Success
- `UFT_ERR_* (negative)` - Error codes

Categories:
- `0x0100` - I/O errors
- `0x0200` - Format errors
- `0x0300` - Memory errors
- `0x0400` - Hardware errors
- `0x0500` - Parameter errors
- `0x0600` - Decoder errors
- `0x0700` - Encoder errors
- `0x0800` - Verification errors

## Thread Safety

- GUI runs on main thread
- Processing uses worker threads
- Session state protected by mutex
- Progress via Qt signals

## Dependencies

```
UFT
├── Qt6 (Core, Widgets, GUI)
├── libusb 1.0 (hardware)
├── zlib (compression)
└── C11 standard library
```

## Directory Structure

```
uft/
├── src/              # Core implementation
│   ├── formats/      # Format handlers
│   ├── flux/         # Flux processing
│   ├── core/         # Core utilities
│   └── gui/          # Qt GUI
├── include/          # Public headers
├── tests/            # Test suite
├── docs/             # Documentation
├── packaging/        # DEB/RPM scripts
└── .github/          # CI workflows
```
