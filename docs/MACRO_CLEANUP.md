# UFT Macro Cleanup Report

## Problem
Multiple files define duplicate macros that should only be defined in `uft_compiler.h`.

## Authoritative Source
`include/uft/uft_compiler.h` - Version 2.0.0

## Files with Duplicate Definitions

| File | Duplicates |
|------|-----------|
| `include/uft/formats/uft_fdi.h` | UFT_PACKED_BEGIN, UFT_PACKED_END, UFT_PACKED_ATTR |
| `include/uft/compat/uft_platform.h` | UFT_PACKED, UFT_PACKED_END, UFT_INLINE |
| `include/uft/uft_common.h` | UFT_PACKED_BEGIN, UFT_PACKED_END, UFT_PACKED, UFT_INLINE |
| `include/uft/uft_fdi.h` | UFT_PACKED_BEGIN, UFT_PACKED_END, UFT_PACKED_ATTR |
| `include/uft/uft_platform.h` | UFT_PACKED_BEGIN, UFT_PACKED_END, UFT_PACKED_STRUCT |
| `include/uft/uft_packed.h` | All UFT_PACKED_* variants |
| `include/uft/uft_fat_filesystem.h` | UFT_PACKED_BEGIN, UFT_PACKED_END, UFT_PACKED |
| `include/uft/uft_config.h` | UFT_INLINE, UFT_PACKED, UFT_UNUSED |

## Solution
All files should include `uft_compiler.h` and use `#ifndef` guards:

\`\`\`c
#include "uft/uft_compiler.h"

/* Macros are now available from uft_compiler.h */
/* DO NOT redefine UFT_PACKED, UFT_INLINE, etc. */
\`\`\`

## Migration Status
- [x] uft_compiler.h is canonical (v2.0.0)
- [x] Duplicate files identified
- [ ] Individual files need update (non-critical, works with current guards)

## Note
The duplicate definitions are guarded with `#ifndef` in most cases, so they
don't cause compilation errors. However, for clean architecture, all files
should use `#include "uft/uft_compiler.h"` instead of defining their own.
