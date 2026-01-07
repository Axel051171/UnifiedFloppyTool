# libflux - Floppy Flux Processing Library

Standalone library modules for floppy disk flux processing,
data recovery, and diagnostics.

## Modules

### 1. Revolution Fusion (`libflux_revolution_fusion.h/c`)

Multi-revolution fusion for improved data recovery from damaged disks.

**Features:**
- Majority voting fusion
- Quality-weighted fusion  
- Per-sector best selection
- Confidence-based selection
- Weak bit detection
- JSON/text report export

**Example:**
```c
#include "libflux_revolution_fusion.h"

uint8_t *revs[4] = { rev0, rev1, rev2, rev3 };
int sizes[4] = { 6250, 6250, 6250, 6250 };
int bitrates[4] = { 250000, 250000, 250000, 250000 };

libflux_fusion_config_t config;
libflux_fusion_config_init(&config);
config.method = LIBFLUX_FUSION_MAJORITY;

libflux_fusion_result_t result;
libflux_fuse_revolutions(revs, sizes, bitrates, 4, &config, &result);

printf("Confidence: %d%%\n", result.overall_confidence);
libflux_fusion_result_free(&result);
```

### 2. JSON Diagnostics (`libflux_json_diagnostics.h/c`)

Machine-readable diagnostic export in JSON format.

## Building

```bash
gcc -c -std=c99 -Wall libflux_revolution_fusion.c -o libflux_fusion.o
gcc -c -std=c99 -Wall libflux_json_diagnostics.c -o libflux_json.o
ar rcs libflux.a libflux_fusion.o libflux_json.o
```

## License

```
Copyright (C) 2025 UFT Project Contributors
SPDX-License-Identifier: GPL-2.0-or-later
```

Clean-room implementation. No external code dependencies.
