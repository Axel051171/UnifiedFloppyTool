# Private Headers - DO NOT USE DIRECTLY

Diese Header sind **interne Implementation-Details** und sollten
**nicht direkt von externem Code inkludiert werden**.

## Warum sind diese Header privat?

1. **Externe Abhängigkeiten**: Benötigen libflux, Qt, oder andere externe Libs
2. **Instabile API**: Kann sich ohne Vorwarnung ändern
3. **Implementation Details**: Interne Datenstrukturen

## Verzeichnisse

### PRIVATE/compat/
Legacy-Kompatibilitäts-Header aus libflux/SAMdisk-Integration:
- `floppy_utils.h`
- `sector_search.h`
- `internal_libflux.h`

### Externe Bereiche (nicht verschoben, aber privat)

**flux/fdc_bitstream/**
- `fdc_bitstream.h`, `fdc_crc.h`, `fdc_defs.h`, `fdc_misc.h`
- `fdc_vfo_base.h`
- `image_base.h`, `image_d77.h`, `image_fdx.h`, `image_hfe.h`
- `image_mfm.h`, `image_raw.h`, `image_rdd.h`

**flux/vfo/**
- `vfo_experimental.h`, `vfo_fixed.h`
- `vfo_pid.h`, `vfo_pid2.h`, `vfo_pid3.h`
- `vfo_simple.h`, `vfo_simple2.h`

**gui/**
- `hardwareprovider.h` (Qt-abhängig)
- `uft_gui_kryoflux_style.h` (Qt-abhängig)

## Stattdessen verwenden

Für Flux-Operationen:
```c
#include "uft/uft_flux.h"          // Public Flux API
#include "uft/core/uft_pll.h"      // Public PLL API
```

Für Disk-Images:
```c
#include "uft/uft_disk.h"          // Public Disk API
#include "uft/formats/uft_*.h"     // Format-spezifische APIs
```

---

*UFT v3.8.5 - Private Header Documentation*
