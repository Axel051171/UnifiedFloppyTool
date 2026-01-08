# CAPS Access API

Official plugin interface from C.A.P.S. (The Classic Amiga Preservation Society).

## Purpose

This API allows applications to support IPF files as a **plugin** - the actual
CAPS library is loaded dynamically at runtime if available.

## License

**Open License** - can be statically linked to any program, including commercial.
See LICENSE file.

## Key Points

1. No actual functionality provided - just the API definitions
2. Users download CAPS library separately from caps-project.org
3. Perfect for applications that want optional IPF support
4. Can be used in commercial products

## Usage Pattern

```c
#include <caps/capsimage.h>

// Try to initialize CAPS
if (CAPSInit() == imgeOk) {
    // CAPS library is available
    int id = CAPSAddImage();
    CAPSLockImage(id, "game.ipf");
    // ... use IPF
    CAPSExit();
} else {
    // No CAPS library - fall back to container-only mode
}
```

## Source

http://www.caps-project.org
