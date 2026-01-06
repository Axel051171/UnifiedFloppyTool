/*
 * uft_presets.h - Master Presets Header
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 *
 * Includes all platform-specific preset headers.
 * Use this single header to access all format presets.
 */

#ifndef UFT_PRESETS_H
#define UFT_PRESETS_H

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Presets - Major Systems
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "presets/uft_preset_zx_spectrum.h"   /* ZX Spectrum (+3, Beta, MGT, etc.) */
#include "presets/uft_preset_pc98.h"          /* NEC PC-98 */
#include "presets/uft_preset_msx.h"           /* MSX */
#include "presets/uft_preset_trs80.h"         /* TRS-80 (JV1, JV3, DMK) */
#include "presets/uft_preset_acorn.h"         /* BBC Micro / Acorn (DFS, ADFS) */
#include "presets/uft_preset_apple.h"         /* Apple II / Macintosh */
#include "presets/uft_preset_atari_st.h"      /* Atari ST */
#include "presets/uft_preset_commodore.h"     /* Commodore (D64, D71, D81, G64) */

/* ═══════════════════════════════════════════════════════════════════════════
 * Container Formats (CQM, IMD, TD0, etc.)
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "presets/uft_preset_containers.h"    /* CQM, IMD, TD0, QCOW, VDI, etc. */

/* ═══════════════════════════════════════════════════════════════════════════
 * Regional/Specialized Formats
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "presets/uft_preset_historical.h"    /* Victor 9000, Oric, DEC, HP, etc. */
#include "presets/uft_preset_japanese.h"      /* DIM, NFD, FDD, D88, XDF */

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_PRESET_PLATFORM_COUNT   11

#define UFT_PRESET_TOTAL_FORMATS    (UFT_ZX_FORMAT_COUNT + \
                                     UFT_PC98_FORMAT_COUNT + \
                                     UFT_MSX_FORMAT_COUNT + \
                                     UFT_TRS80_FORMAT_COUNT + \
                                     UFT_ACORN_FORMAT_COUNT + \
                                     UFT_APPLE_FORMAT_COUNT + \
                                     UFT_ATARI_ST_FORMAT_COUNT + \
                                     UFT_CBM_FORMAT_COUNT + \
                                     UFT_CONTAINER_FORMAT_COUNT + \
                                     UFT_HISTORICAL_FORMAT_COUNT + \
                                     UFT_JAPANESE_FORMAT_COUNT)

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Enumeration
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_platform_id {
    UFT_PLATFORM_ZX_SPECTRUM = 0,
    UFT_PLATFORM_PC98,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_TRS80,
    UFT_PLATFORM_ACORN,
    UFT_PLATFORM_APPLE,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_COMMODORE,
    UFT_PLATFORM_CONTAINERS,
    UFT_PLATFORM_HISTORICAL,
    UFT_PLATFORM_JAPANESE,
} uft_platform_id_t;

typedef struct uft_platform_info {
    uft_platform_id_t id;
    const char *name;
    const char *description;
    uint8_t format_count;
} uft_platform_info_t;

static const uft_platform_info_t UFT_PLATFORMS[] = {
    { UFT_PLATFORM_ZX_SPECTRUM, "ZX Spectrum", "Sinclair ZX Spectrum & clones", UFT_ZX_FORMAT_COUNT },
    { UFT_PLATFORM_PC98, "NEC PC-98", "Japanese NEC PC-9801/9821", UFT_PC98_FORMAT_COUNT },
    { UFT_PLATFORM_MSX, "MSX", "MSX home computers", UFT_MSX_FORMAT_COUNT },
    { UFT_PLATFORM_TRS80, "TRS-80", "Radio Shack TRS-80", UFT_TRS80_FORMAT_COUNT },
    { UFT_PLATFORM_ACORN, "Acorn/BBC", "BBC Micro & Acorn Archimedes", UFT_ACORN_FORMAT_COUNT },
    { UFT_PLATFORM_APPLE, "Apple", "Apple II & Macintosh", UFT_APPLE_FORMAT_COUNT },
    { UFT_PLATFORM_ATARI_ST, "Atari ST", "Atari ST/STE/TT/Falcon", UFT_ATARI_ST_FORMAT_COUNT },
    { UFT_PLATFORM_COMMODORE, "Commodore", "Commodore C64/C128/PET", UFT_CBM_FORMAT_COUNT },
    { UFT_PLATFORM_CONTAINERS, "Containers", "CQM, IMD, TD0, QCOW, etc.", UFT_CONTAINER_FORMAT_COUNT },
    { UFT_PLATFORM_HISTORICAL, "Historical", "Victor 9000, Oric, DEC, HP, etc.", UFT_HISTORICAL_FORMAT_COUNT },
    { UFT_PLATFORM_JAPANESE, "Japanese", "DIM, NFD, FDD, D88, XDF", UFT_JAPANESE_FORMAT_COUNT },
};

#endif /* UFT_PRESETS_H */
