# UFT Hardware Abstraction Layer (HAL) Architecture

## Overview

The UFT HAL provides a unified interface to various floppy disk controllers,
abstracting the hardware-specific details behind a common API.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Application Layer                           │
│                    (uft_read_disk, uft_write_disk)                  │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          HAL Unified API                            │
│                        (uft_hal_unified.h)                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐   │
│  │ enumerate() │ │   open()    │ │ read_flux() │ │ write_flux()│   │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Controller Drivers                           │
├─────────────┬─────────────┬─────────────┬─────────────┬────────────┤
│ Greaseweazle│  Kryoflux   │ SuperCard   │  XUM1541    │   FC5025   │
│             │    DTC      │    Pro      │ ZoomFloppy  │            │
├─────────────┼─────────────┼─────────────┼─────────────┼────────────┤
│ Applesauce  │  NibTools   │   UFI       │   ...       │            │
└─────────────┴─────────────┴─────────────┴─────────────┴────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         OS / Hardware                               │
│              (USB, Serial, libusb, platform APIs)                   │
└─────────────────────────────────────────────────────────────────────┘
```

## Files

### Headers (include/uft/hal/)

| File | Purpose | Status |
|------|---------|--------|
| `uft_hal.h` | Primary HAL API | ✅ Active |
| `uft_hal_unified.h` | Extended unified API | ✅ Active |
| `uft_hal_v2.h` | Deprecated → redirects to v3 | ⚠️ Legacy |
| `uft_hal_v3.h` | HAL v3 types (identical to v2) | ⚠️ Legacy |
| `uft_drive.h` | Drive parameters | ✅ Active |
| `uft_greaseweazle.h` | Greaseweazle specific | ✅ Active |
| `uft_kryoflux.h` | Kryoflux specific | ✅ Active |
| `uft_supercard.h` | SuperCard Pro specific | ✅ Active |
| `uft_xum1541.h` | XUM1541/ZoomFloppy | ✅ Active |
| `uft_fc5025.h` | FC5025 specific | ✅ Active |
| `uft_applesauce.h` | Applesauce specific | ✅ Active |
| `ufi.h` | USB Floppy Interface | ✅ Active |

### Sources (src/hal/)

| File | Purpose | Status |
|------|---------|--------|
| `uft_hal_unified.c` | Main HAL implementation | ✅ In Build |
| `uft_drive.c` | Drive parameter handling | ✅ In Build |
| `uft_kryoflux_dtc.c` | Kryoflux DTC protocol | ✅ In Build |
| `uft_supercard.c` | SuperCard Pro support | ✅ In Build |
| `uft_xum1541.c` | XUM1541/ZoomFloppy | ✅ In Build |
| `uft_fc5025.c` | FC5025 USB controller | ✅ In Build |
| `uft_applesauce.c` | Applesauce controller | ✅ In Build |
| `uft_nibtools.c` | NibTools interface | ✅ In Build |
| `uft_latency_tracking.c` | Latency measurement | ✅ In Build |
| `ufi.c` | USB Floppy Interface | ✅ In Build |
| `uft_greaseweazle_full.c` | Full GW implementation | ⏳ P3: API fix |
| `uft_hal_profiles.c` | Drive profiles | ⏳ P3: API fix |
| `uft_hal_v3.c` | Deprecated (identical to v2) | 🚫 Removed |
| `uft_hal_v2.c` | Deprecated | 🚫 Removed |
| `uft_hal.c` | Legacy base HAL | 🚫 Not in build |

## Supported Controllers

| Controller | Read | Write | Flux | Notes |
|------------|------|-------|------|-------|
| Greaseweazle | ✅ | ✅ | ✅ | Primary recommended |
| Kryoflux | ✅ | ✅ | ✅ | DTC protocol |
| SuperCard Pro | ✅ | ✅ | ✅ | |
| XUM1541 | ✅ | ✅ | ❌ | C64/1541 native |
| ZoomFloppy | ✅ | ✅ | ❌ | XUM1541 compatible |
| FC5025 | ✅ | ❌ | ❌ | Read-only |
| Applesauce | ✅ | ✅ | ✅ | Apple II |

## Usage Example

```c
#include "uft/hal/uft_hal.h"

// Enumerate available controllers
uft_hal_controller_t controllers[8];
int count = uft_hal_enumerate(controllers, 8);

// Open first controller
uft_hal_t* hal = uft_hal_open(controllers[0], "/dev/ttyACM0");
if (!hal) {
    fprintf(stderr, "Failed to open: %s\n", uft_hal_error(hal));
    return -1;
}

// Get capabilities
uft_hal_caps_t caps;
uft_hal_get_caps(hal, &caps);
printf("Max tracks: %d, Sample rate: %u Hz\n", 
       caps.max_tracks, caps.sample_rate_hz);

// Read flux data
uint32_t* flux;
size_t flux_count;
uft_hal_motor(hal, true);
uft_hal_seek(hal, 0);
uft_hal_read_flux(hal, 0, 0, 2, &flux, &flux_count);

// Cleanup
free(flux);
uft_hal_motor(hal, false);
uft_hal_close(hal);
```

## P3 Tasks

1. **uft_greaseweazle_full.c**: Update to match current API types
2. **uft_hal_profiles.c**: Update drive profile structure to current schema
3. Consider removing legacy v2/v3 headers entirely

## Version History

- v1.0: Initial HAL with basic controller support
- v2.0: Added unified API (uft_hal_unified.h)
- v3.0: P2-16 consolidation, removed duplicates
