# UFT R54 - Complete Amiga Protection Registry & Flux Parsers

## Overview

R54 delivers the complete Amiga copy protection database (194 entries) and 
production-ready parsers for the two most important flux capture formats:
SuperCard Pro (SCP) and KryoFlux Stream.

## New Modules

### 1. Amiga Protection Registry (Complete)

**Files:**
- `include/uft/protection/uft_amiga_protection_full.h` (500 lines)
- `src/protection/uft_amiga_protection_full.c` (505 lines)

**Features:**
- 194 protection type definitions covering all known Amiga copy protections
- Publisher-organized registry (Psygnosis, Factor5, Team17, Gremlin, etc.)
- Game-specific protections (Dungeon Master, Shadow of the Beast, Turrican, etc.)
- Sync pattern database (9 unique patterns: 0x4489, 0x8951, 0x448A448A, etc.)
- Protection flags (longtrack, timing, weak bits, LFSR, dual-format)
- Score-based multi-protection detection
- Key track identification

**Protection Categories:**
| Category | Count | Examples |
|----------|-------|----------|
| Major Systems | 12 | CopyLock, SpeedLock, RNC |
| Psygnosis | 8 | Shadow of the Beast, Baal |
| Factor 5 | 5 | Turrican, Denaris |
| Rainbow Arts | 5 | X-Out, Katakis |
| Thalion | 5 | Lionheart, Ambermoon |
| Team17 | 5 | Alien Breed, Worms |
| Gremlin | 5 | Zool, Lotus Turbo |
| Bitmap Brothers | 8 | Xenon, Gods, Speedball |
| Game-Specific | 141 | Various titles |

**API:**
```c
// Get complete registry
const uft_prot_entry_t* uft_prot_get_registry(size_t* count);

// Detect protection from track signatures
int uft_prot_detect(const uft_track_signature_t* tracks,
                    size_t num_tracks,
                    uft_prot_detect_result_t* results,
                    size_t max_results);

// Query protection properties
bool uft_prot_is_longtrack(uft_amiga_prot_type_t type);
bool uft_prot_needs_timing(uft_amiga_prot_type_t type);
bool uft_prot_has_weak_bits(uft_amiga_prot_type_t type);
```

### 2. SuperCard Pro (SCP) Parser

**Files:**
- `include/uft/flux/uft_scp_parser.h` (334 lines)
- `src/flux/uft_scp_parser.c` (436 lines)

**Features:**
- Full SCP format v2.0 support (Jim Drew specification)
- Multi-revolution capture (up to 5 revolutions)
- All disk types: Commodore, Amiga, Atari, Apple, PC, TRS-80, TI, Roland
- Flux overflow handling (0x0000 entries)
- Resolution support (25ns base, configurable multiplier)
- Extension footer parsing
- Checksum verification

**Supported Platforms:**
| Manufacturer | Disk Types |
|--------------|------------|
| Commodore | C64, Amiga |
| Atari | FM SS/DS, ST SS/DS |
| Apple | II, II Pro, Mac 400K/800K/1.44MB |
| IBM PC | 360KB, 720KB, 1.2MB, 1.44MB |
| Tandy | TRS-80 SS/DS SD/DD |
| TI | 99/4A |
| Roland | D-20 |

**API:**
```c
// Create/destroy parser
uft_scp_ctx_t* uft_scp_create(void);
void uft_scp_destroy(uft_scp_ctx_t* ctx);

// Open SCP file
int uft_scp_open(uft_scp_ctx_t* ctx, const char* filename);
int uft_scp_open_memory(uft_scp_ctx_t* ctx, const uint8_t* data, size_t size);

// Read track data
int uft_scp_read_track(uft_scp_ctx_t* ctx, int track, uft_scp_track_data_t* data);
void uft_scp_free_track(uft_scp_track_data_t* data);

// Utilities
uint32_t uft_scp_calculate_rpm(uint32_t index_time_ns);
const char* uft_scp_disk_type_name(uint8_t disk_type);
```

### 3. KryoFlux Stream Parser

**Files:**
- `include/uft/flux/uft_kryoflux_parser.h` (256 lines)
- `src/flux/uft_kryoflux_parser.c` (371 lines)

**Features:**
- Complete KryoFlux stream format support
- Variable-length flux encoding (Flux2, Flux3, overflow)
- Out-of-band (OOB) block parsing
- Index marker extraction (up to 10 revolutions)
- Sample clock: ~48.054 MHz
- Index clock: 1.152 MHz
- Filename parsing (trackXX.Y.raw pattern)

**Stream Opcodes:**
| Opcode | Name | Description |
|--------|------|-------------|
| 0x00-0x07 | Flux2 | 2-byte flux value |
| 0x08 | Nop1 | Skip 1 byte |
| 0x09 | Nop2 | Skip 2 bytes |
| 0x0A | Nop3 | Skip 3 bytes |
| 0x0B | Ovl16 | Overflow (+65536) |
| 0x0C | Flux3 | 3-byte flux value |
| 0x0D | OOB | Out-of-band block |

**OOB Block Types:**
| Type | Name | Content |
|------|------|---------|
| 0x01 | StreamInfo | Stream position, transfer time |
| 0x02 | Index | Index marker with timing |
| 0x03 | StreamEnd | Final position, result code |
| 0x04 | KFInfo | KryoFlux info string |
| 0x0D | EOF | End of file |

**API:**
```c
// Create/destroy parser
uft_kf_ctx_t* uft_kf_create(void);
void uft_kf_destroy(uft_kf_ctx_t* ctx);

// Load stream
int uft_kf_load_file(uft_kf_ctx_t* ctx, const char* filename);
int uft_kf_load_memory(uft_kf_ctx_t* ctx, const uint8_t* data, size_t size);

// Parse stream to revolutions
int uft_kf_parse_stream(uft_kf_ctx_t* ctx, uft_kf_track_data_t* track);
void uft_kf_free_track(uft_kf_track_data_t* track);

// Utilities
uint32_t uft_kf_ticks_to_ns(uint32_t ticks);
uint32_t uft_kf_calculate_rpm(double index_time_us);
bool uft_kf_parse_filename(const char* filename, int* track, int* side);
```

## Usage Examples

### Detect Amiga Protection

```c
#include "uft/protection/uft_amiga_protection_full.h"

// Create track signatures from flux analysis
uft_track_signature_t tracks[160];
// ... fill in track data ...

// Detect protection
uft_prot_detect_result_t results[5];
int count = uft_prot_detect(tracks, 160, results, 5);

for (int i = 0; i < count; i++) {
    printf("Detected: %s (%s) - %d%% confidence\n",
           results[i].name, results[i].publisher, results[i].confidence);
}
```

### Read SCP File

```c
#include "uft/flux/uft_scp_parser.h"

uft_scp_ctx_t* scp = uft_scp_create();
if (uft_scp_open(scp, "disk.scp") == UFT_SCP_OK) {
    printf("Disk type: %s\n", uft_scp_disk_type_name(scp->header.disk_type));
    printf("Tracks: %d\n", uft_scp_get_track_count(scp));
    
    for (int t = 0; t < 160; t++) {
        if (uft_scp_has_track(scp, t)) {
            uft_scp_track_data_t track;
            if (uft_scp_read_track(scp, t, &track) == UFT_SCP_OK) {
                printf("Track %d: %d revolutions, RPM=%d\n",
                       t, track.revolution_count, 
                       track.revolutions[0].rpm);
                uft_scp_free_track(&track);
            }
        }
    }
}
uft_scp_destroy(scp);
```

### Read KryoFlux Stream

```c
#include "uft/flux/uft_kryoflux_parser.h"

uft_kf_ctx_t* kf = uft_kf_create();
if (uft_kf_load_file(kf, "track00.0.raw") == UFT_KF_OK) {
    uft_kf_track_data_t track;
    if (uft_kf_parse_stream(kf, &track) == UFT_KF_OK) {
        printf("Revolutions: %d\n", track.revolution_count);
        for (int r = 0; r < track.revolution_count; r++) {
            printf("  Rev %d: %d flux transitions, %.2f ms, %d RPM\n",
                   r, track.revolutions[r].flux_count,
                   track.revolutions[r].index_time_us / 1000.0,
                   uft_kf_calculate_rpm(track.revolutions[r].index_time_us));
        }
        uft_kf_free_track(&track);
    }
}
uft_kf_destroy(kf);
```

## Statistics

| Component | Header Lines | Implementation Lines | Total |
|-----------|--------------|---------------------|-------|
| Amiga Protection Full | 500 | 505 | 1,005 |
| SCP Parser | 334 | 436 | 770 |
| KryoFlux Parser | 256 | 371 | 627 |
| **Total** | **1,090** | **1,312** | **2,402** |

## Source Attribution

- **Amiga Protections**: disk-utilities by Keir Fraser (Public Domain)
- **SCP Format**: Specification v2.0 by Jim Drew, HxCFloppyEmulator (GPL v2)
- **KryoFlux Format**: KryoFlux documentation, HxCFloppyEmulator (GPL v2)

## Integration Notes

All modules follow UFT coding standards:
- Pure C99/C11 compatible
- No external dependencies (stdlib only)
- Thread-safe design (no global state)
- Comprehensive error handling
- Memory-safe (no leaks, bounds checking)
- Cross-platform (Linux/macOS/Windows)
