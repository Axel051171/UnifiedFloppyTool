# Qt-Dependent Headers

Diese Header benötigen Qt 6.x und sollten nur in Qt-Projekten verwendet werden.

## Header mit Qt-Abhängigkeit

| Header | Benötigt |
|--------|----------|
| `uft_gui.h` | Qt6::Widgets |
| `uft_amiga_gui.h` | Qt6::Widgets |
| `hardwareprovider.h` | Qt6::Core |
| `uft_gui_kryoflux_style.h` | Qt6::Widgets |

## Verwendung

```cpp
// In CMakeLists.txt:
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

// In C++:
#include "uft/uft_gui.h"
```

## Hinweis

Für reine C-Projekte ohne Qt verwenden Sie stattdessen:
- `uft/core/uft_types.h` - Basisdatenstrukturen
- `uft/core/uft_disk.h` - Disk-Operationen
- `uft/core/uft_flux.h` - Flux-Verarbeitung

Diese Header sind Qt-frei und in reinem C11 geschrieben.
