# UFT CAPS/IPF Integration Guide

## Overview

UFT supports IPF (Interchangeable Preservation Format) files at two levels:

1. **Container Level** (built-in) - Always available
2. **Track Level** (via CAPS library) - Optional, requires external library

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        UFT Application                          │
├─────────────────────────────────────────────────────────────────┤
│                     uft_ipf.h (Container API)                   │
│  • Read/write IPF container structure                           │
│  • CRC validation                                               │
│  • Record enumeration                                           │
├─────────────────────────────────────────────────────────────────┤
│                    uft_ipf_caps.h (CAPS Adapter)                │
│  • Dynamic library loading                                      │
│  • Track decoding via libcapsimage                              │
│  • Sector extraction                                            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    External CAPS Library                         │
│  • libcapsimage.so.4 (Linux)                                    │
│  • CAPSImg.dll (Windows)                                        │
│  • CAPSImage.framework (macOS)                                  │
└─────────────────────────────────────────────────────────────────┘
```

## API Versions

| Version | Source | Features |
|---------|--------|----------|
| Access API | caps-project.org | Base API for plugins |
| IPFLib 4.2 | softpres.org | Full API with weak bits, sectors |

### Access API (Minimal)
```c
struct CapsTrackInfo    // Basic track info
struct CapsTrackInfoT1  // With overlap
```

### IPFLib 4.2 (Full)
```c
struct CapsTrackInfo    // Basic
struct CapsTrackInfoT1  // With overlap  
struct CapsTrackInfoT2  // With weak bits, start position
struct CapsSectorInfo   // Sector details
struct CapsDataInfo     // Data regions (weak bits)
```

## Platform IDs

| ID | Platform | Notes |
|----|----------|-------|
| 0 | N/A | Invalid |
| 1 | Amiga | OCS/ECS/AGA |
| 2 | Atari ST | ST/STE |
| 3 | PC | DOS/Windows |
| 4 | Amstrad CPC | |
| 5 | ZX Spectrum | |
| 6 | Sam Coupé | |
| 7 | Archimedes | |
| 8 | C64 | |
| 9 | Atari 8-bit | 400/800/XL/XE |

## Lock Flags

```c
DI_LOCK_INDEX    (1<<0)   // Re-align to index
DI_LOCK_ALIGN    (1<<1)   // Word-aligned decode
DI_LOCK_DENVAR   (1<<2)   // Variable density info
DI_LOCK_DENAUTO  (1<<3)   // Auto density
DI_LOCK_DENNOISE (1<<4)   // Noise density
DI_LOCK_NOISE    (1<<5)   // Unformatted data
DI_LOCK_NOISEREV (1<<6)   // Noise per revolution
DI_LOCK_MEMREF   (1<<7)   // Memory reference
DI_LOCK_UPDATEFD (1<<8)   // Update flakey data
DI_LOCK_TYPE     (1<<9)   // Specify struct type
DI_LOCK_DENALT   (1<<10)  // Alternate density
DI_LOCK_OVLBIT   (1<<11)  // Overlap in bits
DI_LOCK_TRKBIT   (1<<12)  // Track length in bits
DI_LOCK_NOUPDATE (1<<13)  // No weak/overlap update
DI_LOCK_SETWSEED (1<<14)  // Set weak seed
```

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | imgeOk | Success |
| 1 | imgeUnsupported | Unsupported feature |
| 2 | imgeGeneric | Generic error |
| 3 | imgeOutOfRange | Parameter out of range |
| 4 | imgeReadOnly | Read-only image |
| 5 | imgeOpen | Open error |
| 6 | imgeType | Type error |
| 7 | imgeShort | File too short |
| 8 | imgeTrackHeader | Track header error |
| 9 | imgeTrackStream | Track stream error |
| 10 | imgeTrackData | Track data error |
| 11 | imgeDensityHeader | Density header error |
| 12 | imgeDensityStream | Density stream error |
| 13 | imgeDensityData | Density data error |
| 14 | imgeIncompatible | Incompatible version |
| 15 | imgeUnsupportedType | Unsupported type |
| 16 | imgeBadBlockType | Bad block type |
| 17 | imgeBadBlockSize | Bad block size |
| 18 | imgeBadDataStart | Bad data start |
| 19 | imgeBufferShort | Buffer too short |

## Usage Example

### Container-Only (No CAPS Library)
```c
#include "uft/formats/ipf/uft_ipf.h"

uft_ipf_t ipf;
uft_ipf_open(&ipf, "game.ipf");

// Get container info
const uft_ipf_info_t *info = uft_ipf_get_info(&ipf);
printf("Tracks: %u-%u\n", info->min_track, info->max_track);

// List records
for (size_t i = 0; i < uft_ipf_record_count(&ipf); i++) {
    const uft_ipf_record_t *r = uft_ipf_record_at(&ipf, i);
    printf("%s: %u bytes\n", uft_ipf_type_to_string(r->type), r->length);
}

uft_ipf_close(&ipf);
```

### Full Track Decoding (With CAPS Library)
```c
#include "uft/formats/ipf/uft_ipf_caps.h"

caps_lib_t caps;
if (caps_lib_load(&caps, NULL)) {
    caps.Init();
    
    int id = caps.AddImage();
    if (caps.LockImage(id, "game.ipf") == CAPS_OK) {
        caps_image_info_t info;
        caps.GetImageInfo(&info, id);
        
        for (uint32_t cyl = info.min_cylinder; cyl <= info.max_cylinder; cyl++) {
            for (uint32_t head = info.min_head; head <= info.max_head; head++) {
                caps_track_info_t track;
                caps.LockTrack(&track, id, cyl, head, DI_LOCK_INDEX);
                
                // track.trackbuf contains decoded MFM data
                // track.tracklen is the length
                
                caps.UnlockTrack(id, cyl, head);
            }
        }
        
        caps.UnlockImage(id);
    }
    caps.RemImage(id);
    caps.Exit();
    caps_lib_unload(&caps);
}
```

## Installing CAPS Library

### Linux
```bash
# From SPS website
wget http://www.softpres.org/download/ipflib42_linux-x86_64.tar.gz
tar -xzf ipflib42_linux-x86_64.tar.gz
sudo cp libcapsimage.so.4.2 /usr/lib/
sudo ldconfig
```

### Windows
```
Download CAPSImg.dll from softpres.org
Place in same directory as application or System32
```

### macOS
```bash
Download CAPSImage.framework
Copy to /Library/Frameworks/ or ~/Library/Frameworks/
```

## References

- Software Preservation Society: http://www.softpres.org
- C.A.P.S. (Classic Amiga Preservation Society): http://www.caps-project.org
- KryoFlux: http://www.kryoflux.com
