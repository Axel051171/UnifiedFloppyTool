// SPDX-License-Identifier: MIT
/*
 * pipeline_formats.h - Flux Pipeline Formats (TIER 0.5)
 * 
 * Intermediate formats between FLUX and logical decoders:
 * - GCRRAW: Normalized GCR bitcells (C64, Apple II)
 * - MFMRAW: Normalized MFM bitcells (PC, Atari ST, Amiga)
 * 
 * These formats operate on decoded bitstreams without DOS assumptions.
 * Essential pipeline layer for FLUX → Logical format conversion.
 * 
 * @version 2.8.9
 * @date 2024-12-26
 */

#ifndef PIPELINE_FORMATS_H
#define PIPELINE_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * PIPELINE FORMATS (TIER 0.5)
 *============================================================================*/

/* GCRRAW - GCR Bitcell Pipeline */
#include "uft_gcrraw.h"

/* MFMRAW - MFM Bitcell Pipeline */
#include "uft_mfmraw.h"

/*============================================================================*
 * ENCODING TYPES
 *============================================================================*/

/**
 * @brief Disk encoding types
 */
typedef enum {
    ENCODING_UNKNOWN = 0,
    ENCODING_GCR,    /* Group Code Recording (C64, Apple II) */
    ENCODING_MFM,    /* Modified Frequency Modulation (PC, Atari, Amiga) */
    ENCODING_FM      /* Frequency Modulation (older systems) */
} encoding_type_t;

/**
 * @brief Get encoding name string
 * @param enc Encoding type
 * @return Encoding name
 */
static inline const char* encoding_name(encoding_type_t enc)
{
    switch (enc) {
        case ENCODING_GCR: return "GCR (Group Code Recording)";
        case ENCODING_MFM: return "MFM (Modified Frequency Modulation)";
        case ENCODING_FM: return "FM (Frequency Modulation)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * PLATFORM ENCODING MAP
 *============================================================================*/

/**
 * @brief Platform to encoding mapping
 */
typedef struct {
    const char* platform;
    encoding_type_t encoding;
    const char* notes;
} platform_encoding_t;

static const platform_encoding_t PLATFORM_ENCODINGS[] = {
    { "Commodore 64/128",  ENCODING_GCR, "5-to-4 GCR, variable speed zones" },
    { "Apple II",          ENCODING_GCR, "6-and-2 or 5-and-3 GCR" },
    { "PC/DOS",            ENCODING_MFM, "Standard MFM, 250-500 kbps" },
    { "Atari ST",          ENCODING_MFM, "Standard MFM" },
    { "Amiga",             ENCODING_MFM, "Custom MFM with checksums" },
    { "Amstrad CPC",       ENCODING_MFM, "Standard MFM" },
    { "ZX Spectrum +3",    ENCODING_MFM, "Standard MFM" },
    { "TRS-80",            ENCODING_FM,  "Single/Double density FM" },
    { "Apple Macintosh",   ENCODING_GCR, "Variable speed GCR" },
    { NULL, ENCODING_UNKNOWN, NULL }
};

/*============================================================================*
 * PIPELINE WORKFLOW
 *============================================================================*/

/**
 * FLUX PIPELINE ARCHITECTURE:
 * 
 * The pipeline converts raw flux to logical formats in stages:
 * 
 * STAGE 0: Hardware Capture
 *   Greaseweazle/KryoFlux/SCP → Raw flux transitions
 *   Output: Delta timings (hardware-specific format)
 * 
 * STAGE 1: Flux Normalization
 *   Raw flux → Normalized flux stream
 *   Output: Standardized delta timings
 * 
 * STAGE 2: Bitcell Decoding (THIS LAYER!)
 *   GCR: Flux → GCR bitcells → 5-to-4 nibbles
 *   MFM: Flux → MFM bitcells → Data bits
 *   Output: Normalized bitcells/nibbles
 * 
 * STAGE 3: Sector Decoding
 *   GCR: Bitcells → D64/D71/NIB sectors
 *   MFM: Bitcells → IMG/DSK/ADF sectors
 *   Output: Logical disk image
 * 
 * WHY PIPELINE FORMATS ARE CRITICAL:
 * 
 *   WITHOUT pipeline layer:
 *     ❌ Direct FLUX → Logical is too complex
 *     ❌ Cannot reuse decoders across hardware
 *     ❌ Hard to debug decoding issues
 *     ❌ No intermediate verification
 * 
 *   WITH pipeline layer:
 *     ✅ Clean separation of concerns
 *     ✅ Reusable decoder components
 *     ✅ Debuggable intermediate stages
 *     ✅ Format-independent flux handling
 *     ✅ Platform-agnostic architecture
 * 
 * GCRRAW (GCR Pipeline):
 *   - Handles C64 1541/1571 GCR
 *   - Handles Apple II GCR variants
 *   - 5-to-4 GCR decoding
 *   - No track layout assumptions
 *   - No DOS-specific code
 *   - Feeds D64, D71, NIB decoders
 * 
 * MFMRAW (MFM Pipeline):
 *   - Handles PC/DOS MFM
 *   - Handles Atari ST MFM
 *   - Handles Amiga custom MFM
 *   - No track layout assumptions
 *   - No DOS-specific code
 *   - Feeds IMG, DSK, ADF decoders
 * 
 * EXAMPLE WORKFLOW:
 * 
 *   C64 Preservation:
 *     1. Greaseweazle captures disk → disk.gwf (FLUX)
 *     2. GWFLUX reader → normalized flux
 *     3. GCRRAW decoder → GCR bitcells
 *     4. D64 decoder → logical disk image
 * 
 *   PC Preservation:
 *     1. KryoFlux captures disk → track00.0.raw (FLUX)
 *     2. KFS reader → normalized flux
 *     3. MFMRAW decoder → MFM bitcells
 *     4. IMG decoder → logical disk image
 * 
 * CRITICAL SUCCESS FACTORS:
 *   ✅ Platform-independent flux handling
 *   ✅ Reusable decoder components
 *   ✅ Clean separation of layers
 *   ✅ Debuggable pipeline stages
 *   ✅ Future-proof architecture
 */

/*============================================================================*
 * GCR ENCODING DETAILS
 *============================================================================*/

/**
 * GCR (Group Code Recording):
 * 
 * Used by: Commodore 64/128, Apple II, Mac
 * 
 * Commodore GCR (5-to-4):
 *   - 5 bits on disk → 4 data bits
 *   - Guarantees no more than 2 consecutive zeros
 *   - Enables reliable self-clocking
 *   - Variable speed zones (4 zones on 1541)
 * 
 * Apple GCR (6-and-2):
 *   - 6 bits on disk → 8 data bits (via table)
 *   - Different encoding table than C64
 *   - Sector interleaving for performance
 * 
 * Properties:
 *   ✅ Self-clocking (no separate clock track)
 *   ✅ Error detection via encoding constraints
 *   ✅ Higher capacity than FM
 *   ⚠️  More complex than MFM
 *   ⚠️  Platform-specific variants
 */

/*============================================================================*
 * MFM ENCODING DETAILS
 *============================================================================*/

/**
 * MFM (Modified Frequency Modulation):
 * 
 * Used by: PC/DOS, Atari ST, Amiga, CPC, most systems
 * 
 * How it works:
 *   - Each data bit → 2 flux transitions
 *   - Clock bits inserted between data bits (conditionally)
 *   - No clock if: (previous bit = 1) OR (current bit = 1)
 *   - Clock inserted if: (previous bit = 0) AND (current bit = 0)
 * 
 * Properties:
 *   ✅ Self-clocking
 *   ✅ Double capacity of FM
 *   ✅ Industry standard
 *   ✅ Simple decoder
 *   ✅ Platform-independent (mostly)
 * 
 * Standard PC MFM:
 *   - 250/300/500 kbps data rates
 *   - Track-based layout
 *   - Sector headers with CRC
 *   - Gap bytes for timing
 * 
 * Amiga MFM:
 *   - Custom sector format
 *   - 11 sectors/track (DD)
 *   - Special checksums
 *   - Track-based decoding
 */

#ifdef __cplusplus
}
#endif

#endif /* PIPELINE_FORMATS_H */
