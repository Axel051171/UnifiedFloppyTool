#ifndef PROTECTION_ANALYZER_H
#define PROTECTION_ANALYZER_H

/*
 * protection_analyzer.h
 *
 * UFT Protection Analyzer Module
 * --------------------------------
 * Purpose:
 *  - Wraps raw sector images (.IMG/.IMA/.DD/.D64) and Atari .ATR images
 *  - Scans sector data for known copy-protection *signatures / heuristics*
 *  - Generates "flux-aware" metadata (bad sectors, weak-bit regions, key sectors)
 *  - Can export a "protected" representation for downstream tools:
 *      - IMD: flags bad-CRC sectors (standard, widely supported)
 *      - ATX: emitted as a UFT-compatible stub container (see notes in .c)
 *
 * IMPORTANT LIMITATION (honest):
 *  - True copy protections that depend on analog properties (weak bits, fuzzy
 *    areas, long/short tracks, half-tracks, deliberate sync violations, etc.)
 *    are generally NOT representable in plain sector dumps (.IMG/.D64).
 *  - This module therefore focuses on:
 *      (1) detecting what we *can* detect in sector data,
 *      (2) emitting metadata suitable for flux workflows,
 *      (3) generating weak-bit flux timing patterns for hardware write.
 */

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* filePath;
    uint32_t tracks;
    uint32_t heads;
    uint32_t sectorsPerTrack;
    uint32_t sectorSize;
    bool isReadOnly;
    bool supportFlux;     // For copy-protection metadata
    void* internalData;   // format-specific context (owned by module)
} FloppyInterface;

/* --- Uniform interface (MUST stay identical across modules) --- */
int floppy_open(FloppyInterface* dev, const char* path);
int floppy_close(FloppyInterface* dev);
int floppy_read(FloppyInterface* dev, uint32_t c, uint32_t h, uint32_t s, uint8_t* buffer);
int floppy_write(FloppyInterface* dev, uint32_t c, uint32_t h, uint32_t s, const uint8_t* buffer);
int floppy_analyze_protection(FloppyInterface* dev);

/* --- Extra API (allowed) --- */

/* What kind of platform is the image most likely from? */
typedef enum {
    UFT_PLATFORM_UNKNOWN = 0,
    UFT_PLATFORM_PC_DOS,
    UFT_PLATFORM_ATARI_8BIT,
    UFT_PLATFORM_COMMODORE_1541
} UFT_Platform;

/* Protection types we (heuristically) detect. */
typedef enum {
    UFT_PROT_NONE = 0,
    UFT_PROT_PC_SAFEDISC_EARLY,
    UFT_PROT_PC_KEY_SECTOR,
    UFT_PROT_PC_INTENTIONAL_CRC,

    UFT_PROT_ATARI_VMAX,
    UFT_PROT_ATARI_RAPIDLOK,
    UFT_PROT_ATARI_SUPERCHIP_WEAKBITS,

    UFT_PROT_CBM_GCR_HINT,
    UFT_PROT_CBM_ERROR23_HINT
} UFT_ProtectionId;

typedef struct {
    uint32_t c, h, s;
} UFT_CHS;

/* "Bad sector" marker (e.g., deliberate CRC error in IMD export) */
typedef struct {
    UFT_CHS chs;
    uint32_t reason; /* UFT_ProtectionId */
} UFT_BadSector;

/* Weak-bit region marker (for flux-oriented write workflows) */
typedef struct {
    UFT_CHS chs;
    uint32_t byteOffset;  /* offset within sector */
    uint32_t byteLength;  /* length within sector */
    uint32_t cell_ns;     /* nominal bitcell duration (ns) */
    uint32_t jitter_ns;   /* random jitter (ns) */
    uint32_t seed;        /* deterministic seed */
    uint32_t protectionId;
} UFT_WeakRegion;

typedef struct {
    UFT_Platform platform;
    UFT_ProtectionId primaryProtection;

    /* dynamic arrays (owned by analyzer ctx) */
    UFT_BadSector* badSectors;
    size_t badSectorCount;

    UFT_WeakRegion* weakRegions;
    size_t weakRegionCount;
} UFT_ProtectionReport;

/* Retrieve the last report after floppy_analyze_protection(). */
const UFT_ProtectionReport* uft_get_last_report(const FloppyInterface* dev);

/*
 * generate_flux_pattern()
 * -----------------------
 * Create a flux interval stream representing a weak-bit area.
 *
 * Output: malloc()'d array of uint32_t intervals in nanoseconds.
 *         Caller must free().
 *
 * The pattern is intentionally noisy: it jitters around nominal cell time.
 * This is a hardware-level helper, NOT an ATX/G64 encoder.
 */
uint32_t* generate_flux_pattern(uint32_t bit_cells,
                               uint32_t cell_ns,
                               uint32_t jitter_ns,
                               uint32_t seed,
                               uint32_t* out_count);

/*
 * Export helpers:
 *  - IMD: standard-ish output with bad-CRC marking
 *  - ATX: UFT stub (magic + JSON) carrying weak-region metadata
 */
int uft_export_imd(const FloppyInterface* dev, const char* out_path);
int uft_export_atx_stub(const FloppyInterface* dev, const char* out_path);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PROTECTION_ANALYZER_H */
