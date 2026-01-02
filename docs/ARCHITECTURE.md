# UFT Architecture

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Layer-Architektur

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           LAYER 5: GUI                                      │
│  gui/                                                                       │
│  Qt Widgets, Dialogs, Main Window                                          │
│  Darf inkludieren: 1-4                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                           LAYER 4: TOOLS                                    │
│  tools/, cli/                                                               │
│  Command-line tools, Batch processing                                      │
│  Darf inkludieren: 1-3                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                           LAYER 3: FORMATS                                  │
│  formats/, formats_v2/, detection/                                         │
│  Format plugins, Auto-detection, Converters                                │
│  Darf inkludieren: 1-2                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                           LAYER 2: SERVICES                                 │
│  decoder/, encoder/, recovery/, io/                                        │
│  MFM/GCR decoding, PLL, Forensic recovery                                 │
│  Darf inkludieren: 1                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                           LAYER 1: HAL                                      │
│  hal/, hardware/                                                           │
│  Hardware abstraction, Device drivers                                      │
│  Darf inkludieren: 0                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                           LAYER 0: CORE                                     │
│  core/                                                                      │
│  Error handling, Memory, Types, Logging                                    │
│  Darf inkludieren: NICHTS aus UFT                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Include-Regeln

### 2.1 Erlaubte Includes pro Layer

| Layer | Darf inkludieren |
|-------|------------------|
| 5 GUI | `uft/*.h` (alle) |
| 4 TOOLS | `uft/core/`, `uft/hal/`, `uft/formats/`, `uft/decoder/` |
| 3 FORMATS | `uft/core/`, `uft/hal/` |
| 2 SERVICES | `uft/core/` |
| 1 HAL | `uft/core/` |
| 0 CORE | Standard-Library only |

### 2.2 Verbotene Includes

```c
// VERBOTEN: Lower layer includes higher layer
#include "uft/gui/..."      // NIEMALS in core/, hal/, formats/
#include "uft/tools/..."    // NIEMALS in core/, hal/, formats/

// VERBOTEN: Zirkuläre Abhängigkeiten
// formats/d64.h includes formats/g64.h includes formats/d64.h

// VERBOTEN: Platform-spezifisch in Core
#include <windows.h>        // NIEMALS in uft/core/
#include <unistd.h>         // NUR in hal/ mit #ifdef
```

---

## 3. Verzeichnisstruktur

```
uft/
├── include/uft/
│   ├── core/                 # Layer 0
│   │   ├── uft_error.h
│   │   ├── uft_error_handling.h
│   │   ├── uft_ownership.h
│   │   ├── uft_safe_math.h
│   │   ├── uft_endian.h
│   │   └── uft_types.h
│   │
│   ├── hal/                  # Layer 1
│   │   ├── uft_hal.h
│   │   ├── uft_hal_gw.h
│   │   ├── uft_hal_fluxengine.h
│   │   └── uft_hal_kryoflux.h
│   │
│   ├── services/             # Layer 2
│   │   ├── uft_decoder.h
│   │   ├── uft_encoder.h
│   │   ├── uft_pll.h
│   │   └── uft_recovery.h
│   │
│   ├── formats/              # Layer 3
│   │   ├── uft_format_plugin.h
│   │   ├── uft_detection.h
│   │   └── uft_hardened_parsers.h
│   │
│   ├── tools/                # Layer 4
│   │   ├── uft_convert.h
│   │   └── uft_analyze.h
│   │
│   └── gui/                  # Layer 5
│       └── (Qt headers)
│
├── src/
│   ├── core/
│   ├── hal/
│   ├── hardware/
│   ├── decoder/
│   ├── encoder/
│   ├── recovery/
│   ├── formats/
│   ├── formats_v2/
│   ├── detection/
│   ├── tools/
│   └── io/
│
├── gui/
│   ├── main.cpp
│   ├── mainwindow.cpp
│   └── widgets/
│
└── tests/
    ├── unit/
    ├── golden/
    └── fuzz/
```

---

## 4. CMake Target-Struktur

```cmake
# Layer 0: Core (keine UFT-Dependencies)
add_library(uft_core
    src/core/uft_error_handling.c
    src/core/uft_memory.c
)
target_include_directories(uft_core PUBLIC include)

# Layer 1: HAL (depends on core)
add_library(uft_hal
    src/hal/uft_hal.c
    src/hardware/uft_hw_gw.c
)
target_link_libraries(uft_hal PUBLIC uft_core)

# Layer 2: Services (depends on core)
add_library(uft_services
    src/decoder/uft_mfm_decoder.c
    src/encoder/uft_mfm_encoder.c
    src/recovery/uft_forensic.c
)
target_link_libraries(uft_services PUBLIC uft_core)

# Layer 3: Formats (depends on core, hal)
add_library(uft_formats
    src/formats/d64/uft_d64.c
    src/formats/adf/uft_adf.c
    src/formats/scp/uft_scp.c
)
target_link_libraries(uft_formats PUBLIC uft_core uft_hal)

# Layer 4: Tools (depends on all libraries)
add_library(uft_tools
    src/tools/uft_convert.c
    src/tools/uft_analyze.c
)
target_link_libraries(uft_tools PUBLIC uft_formats uft_services)

# Layer 5: GUI (depends on everything)
add_executable(uft_gui
    gui/main.cpp
    gui/mainwindow.cpp
)
target_link_libraries(uft_gui PRIVATE uft_tools Qt6::Widgets)
```

---

## 5. CI Include-Checker

```bash
#!/bin/bash
# scripts/check_includes.sh
# Enforces layer architecture

set -e

ERRORS=0

# Core should not include any UFT headers
for f in src/core/*.c; do
    if grep -l '#include "uft/hal\|#include "uft/formats\|#include "uft/gui' "$f" 2>/dev/null; then
        echo "ERROR: $f includes forbidden layer"
        ERRORS=$((ERRORS + 1))
    fi
done

# HAL should not include formats or gui
for f in src/hal/*.c src/hardware/*.c; do
    if grep -l '#include "uft/formats\|#include "uft/gui' "$f" 2>/dev/null; then
        echo "ERROR: $f includes forbidden layer"
        ERRORS=$((ERRORS + 1))
    fi
done

# Formats should not include gui
for f in src/formats/*.c src/formats/*/*.c; do
    if grep -l '#include "uft/gui' "$f" 2>/dev/null; then
        echo "ERROR: $f includes forbidden layer"
        ERRORS=$((ERRORS + 1))
    fi
done

if [ $ERRORS -gt 0 ]; then
    echo "Found $ERRORS architecture violations"
    exit 1
fi

echo "Architecture check passed"
```

---

## 6. Dependency Diagram

```
                    ┌─────────┐
                    │   GUI   │
                    └────┬────┘
                         │
                    ┌────┴────┐
                    │  TOOLS  │
                    └────┬────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
         ┌────┴────┐ ┌───┴───┐ ┌────┴────┐
         │ FORMATS │ │  I/O  │ │RECOVERY │
         └────┬────┘ └───┬───┘ └────┬────┘
              │          │          │
              └──────────┼──────────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
         ┌────┴────┐ ┌───┴───┐ ┌────┴────┐
         │   HAL   │ │DECODER│ │ ENCODER │
         └────┬────┘ └───┬───┘ └────┬────┘
              │          │          │
              └──────────┼──────────┘
                         │
                    ┌────┴────┐
                    │  CORE   │
                    └─────────┘
```

---

**DOKUMENT ENDE**
