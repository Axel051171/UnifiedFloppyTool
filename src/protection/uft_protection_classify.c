/**
 * @file uft_protection_classify.c
 * @brief Unified Protection Classification API Implementation
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_protection_classify.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*===========================================================================
 * Protection Database
 *===========================================================================*/

static const uft_protection_db_entry_t protection_database[] = {
    /* Amiga Protections */
    {
        .type = UFT_PROT_COPYLOCK,
        .name = "CopyLock",
        .publisher = "Rob Northen Computing",
        .description = "LFSR-based protection with 11 sync markers and timing variations",
        .category = UFT_PCAT_LFSR_ENCODED,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1988,
        .requires_timing = true,
        .requires_flux = false,
        .reconstructable = true
    },
    {
        .type = UFT_PROT_COPYLOCK_OLD,
        .name = "CopyLock (Old)",
        .publisher = "Rob Northen Computing",
        .description = "Early CopyLock variant with 0x65xx sync patterns",
        .category = UFT_PCAT_LFSR_ENCODED,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1987,
        .requires_timing = true,
        .requires_flux = false,
        .reconstructable = true
    },
    {
        .type = UFT_PROT_SPEEDLOCK,
        .name = "Speedlock",
        .publisher = "Speedlock Associates",
        .description = "Variable-density protection with long/short bitcell regions",
        .category = UFT_PCAT_VARIABLE_DENSITY,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1989,
        .requires_timing = true,
        .requires_flux = true,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_PROTEC,
        .name = "PROTEC Longtrack",
        .publisher = "Various",
        .description = "Extended track length with 0x4454 sync",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1989,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_PROTOSCAN,
        .name = "Protoscan",
        .publisher = "Magnetic Fields",
        .description = "Longtrack protection used in Lotus series",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1990,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_TIERTEX,
        .name = "Tiertex",
        .publisher = "Tiertex",
        .description = "Longtrack protection used in Strider II",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1990,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_SILMARILS,
        .name = "Silmarils",
        .publisher = "Silmarils",
        .description = "French publisher longtrack with ROD0 signature",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1989,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_INFOGRAMES,
        .name = "Infogrames",
        .publisher = "Infogrames",
        .description = "Infogrames longtrack protection",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1988,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_PROLANCE,
        .name = "Prolance",
        .publisher = "Ubisoft",
        .description = "Longtrack protection used in B.A.T.",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1990,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_LONGTRACK_APP,
        .name = "APP",
        .publisher = "Amiga Power Pack",
        .description = "Amiga Power Pack longtrack protection",
        .category = UFT_PCAT_LONGTRACK,
        .platform = UFT_PLATFORM_AMIGA,
        .year_introduced = 1991,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    
    /* C64 Protections */
    {
        .type = UFT_PROT_VMAX_V1,
        .name = "V-MAX! v1",
        .publisher = "Vorpal",
        .description = "V-MAX! copy protection version 1",
        .category = UFT_PCAT_GCR_TIMING,
        .platform = UFT_PLATFORM_C64,
        .year_introduced = 1986,
        .requires_timing = true,
        .requires_flux = true,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_VMAX_V2,
        .name = "V-MAX! v2",
        .publisher = "Vorpal",
        .description = "V-MAX! copy protection version 2",
        .category = UFT_PCAT_GCR_TIMING,
        .platform = UFT_PLATFORM_C64,
        .year_introduced = 1987,
        .requires_timing = true,
        .requires_flux = true,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_VMAX_V3,
        .name = "V-MAX! v3",
        .publisher = "Vorpal",
        .description = "V-MAX! copy protection version 3",
        .category = UFT_PCAT_GCR_TIMING,
        .platform = UFT_PLATFORM_C64,
        .year_introduced = 1988,
        .requires_timing = true,
        .requires_flux = true,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_RAPIDLOK_V1,
        .name = "RapidLok v1",
        .publisher = "Rapidlok Systems",
        .description = "RapidLok copy protection version 1",
        .category = UFT_PCAT_GCR_TIMING,
        .platform = UFT_PLATFORM_C64,
        .year_introduced = 1985,
        .requires_timing = true,
        .requires_flux = true,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_VORPAL,
        .name = "Vorpal",
        .publisher = "Microsmith",
        .description = "Vorpal fast loader with protection",
        .category = UFT_PCAT_CUSTOM_FORMAT,
        .platform = UFT_PLATFORM_C64,
        .year_introduced = 1984,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_PIRATESLAYER,
        .name = "PirateSlayer",
        .publisher = "Various",
        .description = "PirateSlayer copy protection",
        .category = UFT_PCAT_CUSTOM_SYNC,
        .platform = UFT_PLATFORM_C64,
        .year_introduced = 1986,
        .requires_timing = true,
        .requires_flux = false,
        .reconstructable = false
    },
    
    /* Apple II Protections */
    {
        .type = UFT_PROT_APPLE_SPIRALDOS,
        .name = "Spiral DOS",
        .publisher = "Various",
        .description = "Non-standard sector interleaving",
        .category = UFT_PCAT_CUSTOM_FORMAT,
        .platform = UFT_PLATFORM_APPLE2,
        .year_introduced = 1982,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    },
    {
        .type = UFT_PROT_APPLE_HALFTRACK,
        .name = "Half-Track",
        .publisher = "Various",
        .description = "Data on half-tracks between standard tracks",
        .category = UFT_PCAT_HALFTRACK,
        .platform = UFT_PLATFORM_APPLE2,
        .year_introduced = 1983,
        .requires_timing = false,
        .requires_flux = true,
        .reconstructable = false
    },
    
    /* Atari ST Protections */
    {
        .type = UFT_PROT_COPYLOCK_ST,
        .name = "CopyLock ST",
        .publisher = "Rob Northen Computing",
        .description = "CopyLock adapted for Atari ST",
        .category = UFT_PCAT_LFSR_ENCODED,
        .platform = UFT_PLATFORM_ATARI_ST,
        .year_introduced = 1988,
        .requires_timing = true,
        .requires_flux = false,
        .reconstructable = true
    },
    {
        .type = UFT_PROT_FUZZY_BITS,
        .name = "Fuzzy Bits",
        .publisher = "Various",
        .description = "Intentionally weak/fuzzy bits",
        .category = UFT_PCAT_WEAK_BITS,
        .platform = UFT_PLATFORM_ATARI_ST,
        .year_introduced = 1988,
        .requires_timing = false,
        .requires_flux = true,
        .reconstructable = false
    },
    
    /* PC Protections */
    {
        .type = UFT_PROT_WEAK_SECTOR,
        .name = "Weak Sector",
        .publisher = "Various",
        .description = "Sector with intentionally weak data",
        .category = UFT_PCAT_WEAK_BITS,
        .platform = UFT_PLATFORM_PC,
        .year_introduced = 1985,
        .requires_timing = false,
        .requires_flux = true,
        .reconstructable = false
    },
    
    /* Sentinel */
    {
        .type = UFT_PROT_UNKNOWN,
        .name = NULL,
        .publisher = NULL,
        .description = NULL,
        .category = UFT_PCAT_NONE,
        .platform = UFT_PLATFORM_UNKNOWN,
        .year_introduced = 0,
        .requires_timing = false,
        .requires_flux = false,
        .reconstructable = false
    }
};

#define PROTECTION_DB_COUNT (sizeof(protection_database) / sizeof(protection_database[0]) - 1)

/*===========================================================================
 * Context Initialization
 *===========================================================================*/

void uft_protect_init_context(uft_protection_context_t *ctx) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->platform = UFT_PLATFORM_AUTO;
    ctx->quick_scan = false;
    ctx->deep_scan = true;
    ctx->start_track = 0;
    ctx->end_track = 0;  /* All tracks */
    
    ctx->detect_timing = true;
    ctx->detect_weak_bits = true;
    ctx->detect_longtrack = true;
    ctx->detect_gcr = true;
    
    ctx->include_raw_data = false;
    ctx->verbose = false;
    
    ctx->progress_cb = NULL;
    ctx->detection_cb = NULL;
    ctx->user_data = NULL;
}

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

uft_platform_t uft_protect_detect_platform(const uint8_t *track_data,
                                            uint32_t track_bits) {
    if (!track_data || track_bits < 1000) return UFT_PLATFORM_UNKNOWN;
    
    uint32_t track_bytes = track_bits / 8;
    
    /* Check for Amiga (11 sectors, ~105000 bits) */
    if (track_bits >= 100000 && track_bits <= 120000) {
        /* Look for Amiga sync 0x4489 */
        for (uint32_t i = 0; i < track_bytes - 2; i++) {
            if (track_data[i] == 0x44 && track_data[i+1] == 0x89) {
                return UFT_PLATFORM_AMIGA;
            }
        }
    }
    
    /* Check for C64/GCR (variable length, GCR encoded) */
    if (track_bits >= 40000 && track_bits <= 80000) {
        /* Look for GCR sync (0xFF 0xFF...) */
        int sync_count = 0;
        for (uint32_t i = 0; i < track_bytes - 1; i++) {
            if (track_data[i] == 0xFF && track_data[i+1] == 0xFF) {
                sync_count++;
            }
        }
        if (sync_count >= 10) {
            return UFT_PLATFORM_C64;
        }
    }
    
    /* Check for Apple II (13 or 16 sectors) */
    if (track_bits >= 48000 && track_bits <= 56000) {
        /* Look for Apple prologue D5 AA 96 */
        for (uint32_t i = 0; i < track_bytes - 3; i++) {
            if (track_data[i] == 0xD5 && 
                track_data[i+1] == 0xAA &&
                track_data[i+2] == 0x96) {
                return UFT_PLATFORM_APPLE2;
            }
        }
    }
    
    /* Check for PC/MFM (9/18 sectors) */
    if (track_bits >= 50000 && track_bits <= 100000) {
        /* Look for MFM sync 0xA1 */
        int a1_count = 0;
        for (uint32_t i = 0; i < track_bytes - 3; i++) {
            if (track_data[i] == 0xA1 && 
                track_data[i+1] == 0xA1 &&
                track_data[i+2] == 0xA1) {
                a1_count++;
            }
        }
        if (a1_count >= 9) {
            return UFT_PLATFORM_PC;
        }
    }
    
    return UFT_PLATFORM_UNKNOWN;
}

/*===========================================================================
 * Amiga Protection Detection
 *===========================================================================*/

int uft_protect_detect_amiga(const uint8_t *track_data,
                              uint32_t track_bits,
                              const uint16_t *timing_data,
                              uint8_t track,
                              uint8_t head,
                              uft_protection_analysis_t *result) {
    if (!track_data || !result) return -1;
    
    int detections = 0;
    
    /* Check CopyLock */
    uft_copylock_result_t copylock_result;
    if (uft_copylock_detect(track_data, track_bits, timing_data,
                            track, head, &copylock_result) == 0) {
        if (copylock_result.detected && result->detection_count < UFT_PROTECT_MAX_DETECTIONS) {
            uft_protection_detection_t *det = &result->detections[result->detection_count];
            memset(det, 0, sizeof(*det));
            
            det->type = (copylock_result.variant == UFT_COPYLOCK_OLD) ?
                        UFT_PROT_COPYLOCK_OLD : UFT_PROT_COPYLOCK;
            det->category = UFT_PCAT_LFSR_ENCODED;
            det->platform = UFT_PLATFORM_AMIGA;
            det->confidence = (uft_protection_confidence_t)(copylock_result.confidence * 25);
            det->confidence_pct = copylock_result.confidence * 25;
            det->track = track;
            det->head = head;
            
            snprintf(det->name, sizeof(det->name), "CopyLock");
            snprintf(det->variant, sizeof(det->variant), "%s",
                     uft_copylock_variant_name(copylock_result.variant));
            snprintf(det->detail, sizeof(det->detail), "%s", copylock_result.info);
            
            det->data.copylock = copylock_result;
            det->has_data = true;
            det->requires_timing = true;
            det->reconstructable = copylock_result.seed_valid;
            det->seed = copylock_result.lfsr_seed;
            
            result->detection_count++;
            detections++;
        }
    }
    
    /* Check Speedlock */
    if (timing_data) {
        uft_speedlock_result_t speedlock_result;
        if (uft_speedlock_detect(track_data, track_bits, timing_data,
                                  track, head, &speedlock_result) == 0) {
            if (speedlock_result.detected && result->detection_count < UFT_PROTECT_MAX_DETECTIONS) {
                uft_protection_detection_t *det = &result->detections[result->detection_count];
                memset(det, 0, sizeof(*det));
                
                det->type = UFT_PROT_SPEEDLOCK;
                det->category = UFT_PCAT_VARIABLE_DENSITY;
                det->platform = UFT_PLATFORM_AMIGA;
                det->confidence = (uft_protection_confidence_t)(speedlock_result.confidence * 25);
                det->confidence_pct = speedlock_result.confidence * 25;
                det->track = track;
                det->head = head;
                
                snprintf(det->name, sizeof(det->name), "Speedlock");
                snprintf(det->variant, sizeof(det->variant), "%s",
                         uft_speedlock_variant_name(speedlock_result.variant));
                snprintf(det->detail, sizeof(det->detail), "%s", speedlock_result.info);
                
                det->data.speedlock = speedlock_result;
                det->has_data = true;
                det->requires_timing = true;
                det->requires_flux = true;
                det->reconstructable = false;
                
                result->detection_count++;
                detections++;
            }
        }
    }
    
    /* Check Longtrack */
    uft_longtrack_result_t longtrack_result;
    if (uft_longtrack_detect(track_data, track_bits, track, head,
                              &longtrack_result) == 0) {
        if (longtrack_result.detected && result->detection_count < UFT_PROTECT_MAX_DETECTIONS) {
            uft_protection_detection_t *det = &result->detections[result->detection_count];
            memset(det, 0, sizeof(*det));
            
            /* Map longtrack type to protection type */
            switch (longtrack_result.primary.type) {
                case UFT_LONGTRACK_PROTEC:
                    det->type = UFT_PROT_LONGTRACK_PROTEC;
                    break;
                case UFT_LONGTRACK_PROTOSCAN:
                    det->type = UFT_PROT_LONGTRACK_PROTOSCAN;
                    break;
                case UFT_LONGTRACK_TIERTEX:
                    det->type = UFT_PROT_LONGTRACK_TIERTEX;
                    break;
                case UFT_LONGTRACK_SILMARILS:
                    det->type = UFT_PROT_LONGTRACK_SILMARILS;
                    break;
                case UFT_LONGTRACK_INFOGRAMES:
                    det->type = UFT_PROT_LONGTRACK_INFOGRAMES;
                    break;
                case UFT_LONGTRACK_PROLANCE:
                    det->type = UFT_PROT_LONGTRACK_PROLANCE;
                    break;
                case UFT_LONGTRACK_APP:
                    det->type = UFT_PROT_LONGTRACK_APP;
                    break;
                case UFT_LONGTRACK_SEVENCITIES:
                    det->type = UFT_PROT_LONGTRACK_SEVENCITIES;
                    break;
                case UFT_LONGTRACK_SUPERMETHANEBROS:
                    det->type = UFT_PROT_LONGTRACK_SMB_GCR;
                    break;
                default:
                    det->type = UFT_PROT_UNKNOWN;
                    break;
            }
            
            det->category = UFT_PCAT_LONGTRACK;
            det->platform = UFT_PLATFORM_AMIGA;
            det->confidence = (uft_protection_confidence_t)(longtrack_result.confidence * 25);
            det->confidence_pct = longtrack_result.confidence * 25;
            det->track = track;
            det->head = head;
            
            snprintf(det->name, sizeof(det->name), "%s",
                     uft_longtrack_type_name(longtrack_result.primary.type));
            snprintf(det->detail, sizeof(det->detail), "%s", longtrack_result.info);
            
            det->data.longtrack = longtrack_result;
            det->has_data = true;
            det->requires_timing = false;
            det->reconstructable = false;
            
            result->detection_count++;
            detections++;
        }
    }
    
    return detections;
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_protect_analyze_track(const uint8_t *track_data,
                               uint32_t track_bits,
                               const uint16_t *timing_data,
                               uint8_t track,
                               uint8_t head,
                               const uft_protection_context_t *ctx,
                               uft_protection_analysis_t *result) {
    if (!track_data || !result) return -1;
    
    uft_protection_context_t default_ctx;
    if (!ctx) {
        uft_protect_init_context(&default_ctx);
        ctx = &default_ctx;
    }
    
    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->requested_platform = ctx->platform;
    
    /* Detect platform if auto */
    if (ctx->platform == UFT_PLATFORM_AUTO) {
        result->detected_platform = uft_protect_detect_platform(track_data, track_bits);
    } else {
        result->detected_platform = ctx->platform;
    }
    
    /* Call platform-specific detection */
    int detections = 0;
    
    switch (result->detected_platform) {
        case UFT_PLATFORM_AMIGA:
            detections = uft_protect_detect_amiga(track_data, track_bits,
                                                   timing_data, track, head, result);
            break;
            
        case UFT_PLATFORM_C64:
            detections = uft_protect_detect_c64(track_data, track_bits,
                                                 timing_data, track, head, result);
            break;
            
        case UFT_PLATFORM_APPLE2:
            detections = uft_protect_detect_apple2(track_data, track_bits,
                                                    track, head, result);
            break;
            
        case UFT_PLATFORM_ATARI_ST:
            detections = uft_protect_detect_atari_st(track_data, track_bits,
                                                      timing_data, track, head, result);
            break;
            
        default:
            /* Try all platforms */
            detections += uft_protect_detect_amiga(track_data, track_bits,
                                                    timing_data, track, head, result);
            break;
    }
    
    /* Update result summary */
    result->tracks_analyzed = 1;
    result->is_protected = (result->detection_count > 0);
    result->is_standard = !result->is_protected;
    
    /* Find primary detection (highest confidence) */
    if (result->detection_count > 0) {
        result->primary = &result->detections[0];
        for (uint8_t i = 1; i < result->detection_count; i++) {
            if (result->detections[i].confidence_pct > result->primary->confidence_pct) {
                result->primary = &result->detections[i];
            }
        }
        
        result->tracks_protected = 1;
        
        /* Check if all reconstructable */
        result->all_reconstructable = true;
        for (uint8_t i = 0; i < result->detection_count; i++) {
            if (!result->detections[i].reconstructable) {
                result->all_reconstructable = false;
                break;
            }
        }
    }
    
    /* Generate summary */
    if (result->is_protected) {
        snprintf(result->summary, sizeof(result->summary),
                 "%d protection(s) detected on track %d/%d: %s",
                 result->detection_count, track, head,
                 result->primary ? result->primary->name : "Unknown");
    } else {
        snprintf(result->summary, sizeof(result->summary),
                 "No protection detected on track %d/%d", track, head);
    }
    
    /* Notify callback */
    if (ctx->detection_cb && result->detection_count > 0) {
        for (uint8_t i = 0; i < result->detection_count; i++) {
            ctx->detection_cb(&result->detections[i], ctx->user_data);
        }
    }
    
    return detections;
}

/*===========================================================================
 * Stub Implementations for Other Platforms
 *===========================================================================*/

int uft_protect_detect_c64(const uint8_t *track_data,
                            uint32_t track_bits,
                            const uint16_t *timing_data,
                            uint8_t track,
                            uint8_t head,
                            uft_protection_analysis_t *result) {
    /* TODO: Integrate with existing C64 protection suite */
    (void)track_data;
    (void)track_bits;
    (void)timing_data;
    (void)track;
    (void)head;
    (void)result;
    return 0;
}

int uft_protect_detect_apple2(const uint8_t *track_data,
                               uint32_t track_bits,
                               uint8_t track,
                               uint8_t head,
                               uft_protection_analysis_t *result) {
    /* TODO: Integrate with existing Apple II protection suite */
    (void)track_data;
    (void)track_bits;
    (void)track;
    (void)head;
    (void)result;
    return 0;
}

int uft_protect_detect_atari_st(const uint8_t *track_data,
                                 uint32_t track_bits,
                                 const uint16_t *timing_data,
                                 uint8_t track,
                                 uint8_t head,
                                 uft_protection_analysis_t *result) {
    /* TODO: Integrate with existing Atari ST protection suite */
    (void)track_data;
    (void)track_bits;
    (void)timing_data;
    (void)track;
    (void)head;
    (void)result;
    return 0;
}

/*===========================================================================
 * Quick Check
 *===========================================================================*/

bool uft_protect_quick_check(const uint8_t *track_data,
                              uint32_t track_bits,
                              uft_platform_t platform) {
    if (!track_data) return false;
    
    /* Check for longtrack */
    if (platform == UFT_PLATFORM_AMIGA || platform == UFT_PLATFORM_AUTO) {
        if (uft_longtrack_is_long(track_bits)) {
            return true;
        }
        
        /* Quick CopyLock check */
        if (uft_copylock_quick_check(track_data, track_bits) >= 3) {
            return true;
        }
    }
    
    return false;
}

/*===========================================================================
 * Database Functions
 *===========================================================================*/

const uft_protection_db_entry_t* uft_protect_get_db_entry(uft_protection_type_t type) {
    for (size_t i = 0; i < PROTECTION_DB_COUNT; i++) {
        if (protection_database[i].type == type) {
            return &protection_database[i];
        }
    }
    return NULL;
}

int uft_protect_get_platform_protections(uft_platform_t platform,
                                          const uft_protection_db_entry_t **entries,
                                          int max_entries) {
    if (!entries || max_entries <= 0) return 0;
    
    int count = 0;
    for (size_t i = 0; i < PROTECTION_DB_COUNT && count < max_entries; i++) {
        if (protection_database[i].platform == platform) {
            entries[count++] = &protection_database[i];
        }
    }
    return count;
}

/*===========================================================================
 * Classification Functions
 *===========================================================================*/

uft_protection_category_t uft_protect_get_category(uft_protection_type_t type) {
    const uft_protection_db_entry_t *entry = uft_protect_get_db_entry(type);
    return entry ? entry->category : UFT_PCAT_NONE;
}

uft_platform_t uft_protect_get_platform(uft_protection_type_t type) {
    const uft_protection_db_entry_t *entry = uft_protect_get_db_entry(type);
    return entry ? entry->platform : UFT_PLATFORM_UNKNOWN;
}

bool uft_protect_requires_timing(uft_protection_type_t type) {
    const uft_protection_db_entry_t *entry = uft_protect_get_db_entry(type);
    return entry ? entry->requires_timing : false;
}

bool uft_protect_is_reconstructable(uft_protection_type_t type) {
    const uft_protection_db_entry_t *entry = uft_protect_get_db_entry(type);
    return entry ? entry->reconstructable : false;
}

/*===========================================================================
 * Name Functions
 *===========================================================================*/

const char* uft_protect_type_name(uft_protection_type_t type) {
    const uft_protection_db_entry_t *entry = uft_protect_get_db_entry(type);
    return entry && entry->name ? entry->name : "Unknown";
}

const char* uft_protect_category_name(uft_protection_category_t cat) {
    switch (cat) {
        case UFT_PCAT_NONE: return "None";
        case UFT_PCAT_VARIABLE_DENSITY: return "Variable Density";
        case UFT_PCAT_TIMING_SENSITIVE: return "Timing Sensitive";
        case UFT_PCAT_LONGTRACK: return "Longtrack";
        case UFT_PCAT_SHORTTRACK: return "Shorttrack";
        case UFT_PCAT_HALFTRACK: return "Half-Track";
        case UFT_PCAT_EXTRA_TRACKS: return "Extra Tracks";
        case UFT_PCAT_LFSR_ENCODED: return "LFSR Encoded";
        case UFT_PCAT_ENCRYPTED: return "Encrypted";
        case UFT_PCAT_SIGNATURE: return "Signature";
        case UFT_PCAT_CUSTOM_SYNC: return "Custom Sync";
        case UFT_PCAT_CUSTOM_FORMAT: return "Custom Format";
        case UFT_PCAT_INVALID_DATA: return "Invalid Data";
        case UFT_PCAT_WEAK_BITS: return "Weak Bits";
        case UFT_PCAT_NO_FLUX: return "No Flux";
        case UFT_PCAT_GCR_TIMING: return "GCR Timing";
        case UFT_PCAT_GCR_INVALID: return "Invalid GCR";
        case UFT_PCAT_FAT_TRACK: return "Fat Track";
        case UFT_PCAT_MULTI_TECHNIQUE: return "Multi-Technique";
        default: return "Unknown";
    }
}

const char* uft_protect_platform_name(uft_platform_t platform) {
    switch (platform) {
        case UFT_PLATFORM_UNKNOWN: return "Unknown";
        case UFT_PLATFORM_AMIGA: return "Amiga";
        case UFT_PLATFORM_C64: return "Commodore 64";
        case UFT_PLATFORM_APPLE2: return "Apple II";
        case UFT_PLATFORM_ATARI_ST: return "Atari ST";
        case UFT_PLATFORM_ATARI_8BIT: return "Atari 8-bit";
        case UFT_PLATFORM_PC: return "IBM PC";
        case UFT_PLATFORM_BBC: return "BBC Micro";
        case UFT_PLATFORM_MSX: return "MSX";
        case UFT_PLATFORM_SPECTRUM: return "ZX Spectrum";
        case UFT_PLATFORM_CPC: return "Amstrad CPC";
        case UFT_PLATFORM_AUTO: return "Auto-Detect";
        default: return "Unknown";
    }
}

const char* uft_protect_confidence_name(uft_protection_confidence_t conf) {
    switch (conf) {
        case UFT_PCONF_NONE: return "Not Detected";
        case UFT_PCONF_POSSIBLE: return "Possible";
        case UFT_PCONF_LIKELY: return "Likely";
        case UFT_PCONF_PROBABLE: return "Probable";
        case UFT_PCONF_CERTAIN: return "Certain";
        default: return "Unknown";
    }
}

/*===========================================================================
 * Reporting
 *===========================================================================*/

size_t uft_protect_report(const uft_protection_analysis_t *result,
                           char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    size_t w = 0;
    
    w += snprintf(buffer + w, buffer_size - w,
        "=== Protection Analysis Report ===\n\n"
        "Platform: %s (detected: %s)\n"
        "Protected: %s\n"
        "Tracks analyzed: %d\n"
        "Tracks protected: %d\n"
        "Detections: %d\n\n",
        uft_protect_platform_name(result->requested_platform),
        uft_protect_platform_name(result->detected_platform),
        result->is_protected ? "YES" : "NO",
        result->tracks_analyzed,
        result->tracks_protected,
        result->detection_count);
    
    if (w >= buffer_size) return buffer_size - 1;
    
    for (uint8_t i = 0; i < result->detection_count && w < buffer_size; i++) {
        const uft_protection_detection_t *det = &result->detections[i];
        
        w += snprintf(buffer + w, buffer_size - w,
            "[%d] %s\n"
            "    Variant: %s\n"
            "    Category: %s\n"
            "    Confidence: %d%% (%s)\n"
            "    Track: %d, Head: %d\n"
            "    Requires timing: %s\n"
            "    Requires flux: %s\n"
            "    Reconstructable: %s\n",
            i + 1, det->name,
            det->variant[0] ? det->variant : "N/A",
            uft_protect_category_name(det->category),
            det->confidence_pct,
            uft_protect_confidence_name(det->confidence),
            det->track, det->head,
            det->requires_timing ? "Yes" : "No",
            det->requires_flux ? "Yes" : "No",
            det->reconstructable ? "Yes" : "No");
        
        if (det->reconstructable && det->seed != 0) {
            w += snprintf(buffer + w, buffer_size - w,
                "    Seed: 0x%06X\n", det->seed);
        }
        
        if (det->detail[0]) {
            w += snprintf(buffer + w, buffer_size - w,
                "    Detail: %s\n", det->detail);
        }
        
        w += snprintf(buffer + w, buffer_size - w, "\n");
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "Summary: %s\n", result->summary);
    }
    
    return w;
}

size_t uft_protect_export_json(const uft_protection_analysis_t *result,
                                char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size == 0) return 0;
    
    size_t w = 0;
    
    w += snprintf(buffer + w, buffer_size - w,
        "{\n"
        "  \"platform\": \"%s\",\n"
        "  \"detected_platform\": \"%s\",\n"
        "  \"is_protected\": %s,\n"
        "  \"tracks_analyzed\": %d,\n"
        "  \"tracks_protected\": %d,\n"
        "  \"detection_count\": %d,\n"
        "  \"all_reconstructable\": %s,\n",
        uft_protect_platform_name(result->requested_platform),
        uft_protect_platform_name(result->detected_platform),
        result->is_protected ? "true" : "false",
        result->tracks_analyzed,
        result->tracks_protected,
        result->detection_count,
        result->all_reconstructable ? "true" : "false");
    
    if (w >= buffer_size) return buffer_size - 1;
    
    w += snprintf(buffer + w, buffer_size - w, "  \"detections\": [\n");
    
    for (uint8_t i = 0; i < result->detection_count && w < buffer_size; i++) {
        const uft_protection_detection_t *det = &result->detections[i];
        
        w += snprintf(buffer + w, buffer_size - w,
            "    {\n"
            "      \"name\": \"%s\",\n"
            "      \"variant\": \"%s\",\n"
            "      \"category\": \"%s\",\n"
            "      \"confidence\": %d,\n"
            "      \"track\": %d,\n"
            "      \"head\": %d,\n"
            "      \"requires_timing\": %s,\n"
            "      \"requires_flux\": %s,\n"
            "      \"reconstructable\": %s",
            det->name,
            det->variant[0] ? det->variant : "",
            uft_protect_category_name(det->category),
            det->confidence_pct,
            det->track, det->head,
            det->requires_timing ? "true" : "false",
            det->requires_flux ? "true" : "false",
            det->reconstructable ? "true" : "false");
        
        if (det->reconstructable && det->seed != 0) {
            w += snprintf(buffer + w, buffer_size - w,
                ",\n      \"seed\": %u", det->seed);
        }
        
        w += snprintf(buffer + w, buffer_size - w,
            "\n    }%s\n",
            (i < result->detection_count - 1) ? "," : "");
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "  ],\n"
            "  \"summary\": \"%s\"\n"
            "}\n",
            result->summary);
    }
    
    return w;
}

void uft_protect_free_result(uft_protection_analysis_t *result) {
    if (result) {
        memset(result, 0, sizeof(*result));
    }
}
