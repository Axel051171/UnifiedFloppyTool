/**
 * @file uft_protection.c
 * @brief Copy Protection Detection Implementation
 * 
 * Detects various copy protection schemes used on floppy disks.
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/protection/uft_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * CopyLock Sync Mark Table
 *============================================================================*/

const uint16_t uft_copylock_sync_marks[UFT_COPYLOCK_SECTORS] = {
    0x8a91, 0x8a44, 0x8a45, 0x8a51, 0x8912,
    0x8911, 0x8914, 0x8915, 0x8944, 0x8945, 0x8951
};

/*============================================================================
 * LFSR Functions
 *============================================================================*/

uint32_t uft_lfsr_advance(uint32_t state, int steps) {
    if (steps >= 0) {
        while (steps--) {
            state = uft_lfsr_next(state);
        }
    } else {
        while (steps++) {
            state = uft_lfsr_prev(state);
        }
    }
    return state;
}

/*============================================================================
 * Context Management
 *============================================================================*/

void uft_protection_init(uft_protection_ctx_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

void uft_protection_free(uft_protection_ctx_t* ctx) {
    if (!ctx) return;
    
    /* Free weak bit regions */
    if (ctx->weakbits.regions) {
        free(ctx->weakbits.regions);
        ctx->weakbits.regions = NULL;
    }
    
    /* Free report hits */
    if (ctx->report.hits) {
        free(ctx->report.hits);
        ctx->report.hits = NULL;
    }
}

/*============================================================================
 * CopyLock Detection
 *============================================================================*/

static bool find_sync_mark(const uint8_t* data, size_t size, 
                           uint16_t pattern, size_t* offset) {
    if (!data || size < 2) return false;
    
    for (size_t i = 0; i < size - 1; i++) {
        uint16_t word = ((uint16_t)data[i] << 8) | data[i + 1];
        if (word == pattern) {
            if (offset) *offset = i;
            return true;
        }
    }
    return false;
}

bool uft_detect_copylock(uft_protection_ctx_t* ctx) {
    if (!ctx || !ctx->track_data || ctx->track_size < 100) return false;
    
    ctx->copylock.detected = false;
    ctx->copylock.num_sectors = 0;
    
    /* Look for CopyLock sync marks */
    for (int s = 0; s < UFT_COPYLOCK_SECTORS; s++) {
        size_t offset;
        if (find_sync_mark(ctx->track_data, ctx->track_size,
                           uft_copylock_sync_marks[s], &offset)) {
            ctx->copylock.sync_marks[ctx->copylock.num_sectors] = 
                uft_copylock_sync_marks[s];
            ctx->copylock.num_sectors++;
        }
    }
    
    /* Need at least 6 CopyLock sectors for positive detection */
    if (ctx->copylock.num_sectors >= 6) {
        ctx->copylock.detected = true;
        ctx->detected |= UFT_PROT_COPYLOCK;
        ctx->confidence = UFT_CONF_HIGH;
        
        /* Try to recover LFSR seed */
        /* The seed is typically embedded in sector 0 */
        ctx->copylock.seed = 0;  /* TODO: Extract from data */
    }
    
    return ctx->copylock.detected;
}

/*============================================================================
 * SpeedLock Detection
 *============================================================================*/

bool uft_detect_speedlock(uft_protection_ctx_t* ctx) {
    if (!ctx || !ctx->flux_data || ctx->flux_count < 100) return false;
    
    ctx->speedlock.detected = false;
    
    /* SpeedLock uses intentional timing variations */
    /* Look for alternating fast/slow sectors */
    
    double total_time = 0;
    for (size_t i = 0; i < ctx->flux_count; i++) {
        total_time += (double)ctx->flux_data[i] / ctx->sample_clock;
    }
    
    /* Estimate sectors from timing */
    double expected_sector_time = 0.002;  /* ~2ms per sector */
    int estimated_sectors = (int)(total_time / expected_sector_time);
    
    if (estimated_sectors >= 9 && estimated_sectors <= 12) {
        /* Analyze timing variance */
        double* sector_times = calloc(estimated_sectors, sizeof(double));
        if (!sector_times) return false;
        
        /* Simple sector time estimation */
        size_t flux_per_sector = ctx->flux_count / estimated_sectors;
        
        double min_time = 1e9, max_time = 0;
        
        for (int s = 0; s < estimated_sectors; s++) {
            double sector_time = 0;
            for (size_t i = s * flux_per_sector; 
                 i < (s + 1) * flux_per_sector && i < ctx->flux_count; i++) {
                sector_time += (double)ctx->flux_data[i] / ctx->sample_clock;
            }
            sector_times[s] = sector_time;
            
            if (sector_time < min_time) min_time = sector_time;
            if (sector_time > max_time) max_time = sector_time;
        }
        
        /* SpeedLock typically has 5% timing variation */
        double variance_ratio = (max_time - min_time) / ((max_time + min_time) / 2);
        
        if (variance_ratio > 0.03 && variance_ratio < 0.15) {
            ctx->speedlock.detected = true;
            ctx->speedlock.variant = 1;  /* Standard SpeedLock */
            ctx->detected |= UFT_PROT_TIMING_BASED;
            ctx->confidence = UFT_CONF_MEDIUM;
        }
        
        free(sector_times);
    }
    
    return ctx->speedlock.detected;
}

/*============================================================================
 * Long Track Detection
 *============================================================================*/

bool uft_detect_longtrack(uft_protection_ctx_t* ctx) {
    if (!ctx || !ctx->flux_data || ctx->flux_count < 100) return false;
    
    ctx->longtrack.detected = false;
    
    /* Calculate track time from flux data */
    double track_time_ns = 0;
    for (size_t i = 0; i < ctx->flux_count; i++) {
        track_time_ns += (double)ctx->flux_data[i] * 1e9 / ctx->sample_clock;
    }
    
    double track_time_ms = track_time_ns / 1e6;
    
    /* Standard track times */
    double expected_dd = 200.0;  /* 300 RPM = 200ms */
    double expected_hd = 166.67; /* 360 RPM = 166.67ms */
    
    /* Detect based on expected rotation speed */
    double expected_ms = expected_dd;  /* Assume DD */
    
    ctx->longtrack.track_length_ms = track_time_ms;
    ctx->longtrack.expected_length_ms = expected_ms;
    ctx->longtrack.ratio = track_time_ms / expected_ms;
    
    /* Long track if > 105% of expected */
    if (ctx->longtrack.ratio > 1.05) {
        ctx->longtrack.detected = true;
        ctx->detected |= UFT_PROT_LONG_TRACK;
        ctx->confidence = UFT_CONF_HIGH;
    }
    
    return ctx->longtrack.detected;
}

/*============================================================================
 * Weak Bits Detection
 *============================================================================*/

bool uft_detect_weakbits(uft_protection_ctx_t* ctx) {
    if (!ctx || !ctx->track_data || ctx->track_size < 100) return false;
    
    ctx->weakbits.detected = false;
    
    /* Weak bits are detected by comparing multiple reads */
    /* Without multiple reads, we can only detect patterns that suggest weak bits */
    
    /* Look for suspicious patterns:
     * - Long runs of alternating bits
     * - Unusual byte patterns (0x55, 0xAA repeated)
     */
    
    size_t suspicious_count = 0;
    size_t region_start = 0;
    bool in_region = false;
    
    for (size_t i = 0; i < ctx->track_size - 1; i++) {
        bool suspicious = false;
        
        /* Check for alternating pattern */
        if (ctx->track_data[i] == 0x55 || ctx->track_data[i] == 0xAA) {
            if (ctx->track_data[i + 1] == ctx->track_data[i] ||
                ctx->track_data[i + 1] == (ctx->track_data[i] ^ 0xFF)) {
                suspicious = true;
            }
        }
        
        if (suspicious) {
            if (!in_region) {
                region_start = i;
                in_region = true;
            }
            suspicious_count++;
        } else if (in_region) {
            /* End of suspicious region */
            size_t region_len = i - region_start;
            
            if (region_len >= 8) {
                /* Record weak bit region */
                if (!ctx->weakbits.regions) {
                    ctx->weakbits.regions = calloc(UFT_MAX_WEAK_REGIONS,
                                                    sizeof(uft_weak_region_t));
                }
                
                if (ctx->weakbits.regions && 
                    ctx->weakbits.num_regions < UFT_MAX_WEAK_REGIONS) {
                    uft_weak_region_t* region = 
                        &ctx->weakbits.regions[ctx->weakbits.num_regions];
                    region->offset = (uint32_t)region_start;
                    region->length = (uint32_t)(region_len * 8);
                    ctx->weakbits.num_regions++;
                }
            }
            
            in_region = false;
        }
    }
    
    if (ctx->weakbits.num_regions > 0) {
        ctx->weakbits.detected = true;
        ctx->detected |= UFT_PROT_WEAK_BITS;
        ctx->confidence = UFT_CONF_MEDIUM;
    }
    
    return ctx->weakbits.detected;
}

/*============================================================================
 * Custom Sync Detection
 *============================================================================*/

bool uft_detect_custom_sync(uft_protection_ctx_t* ctx) {
    if (!ctx || !ctx->track_data || ctx->track_size < 100) return false;
    
    ctx->custom_sync.detected = false;
    ctx->custom_sync.num_patterns = 0;
    
    /* Standard sync patterns */
    const uint16_t standard_syncs[] = {
        0x4489,  /* MFM A1 */
        0x5224,  /* MFM C2 */
        0xAAAA,  /* Clock pattern */
    };
    
    /* Search for non-standard patterns that appear multiple times */
    for (size_t i = 0; i < ctx->track_size - 2; i++) {
        uint16_t pattern = ((uint16_t)ctx->track_data[i] << 8) | 
                           ctx->track_data[i + 1];
        
        /* Skip standard patterns */
        bool is_standard = false;
        for (size_t s = 0; s < sizeof(standard_syncs) / sizeof(standard_syncs[0]); s++) {
            if (pattern == standard_syncs[s]) {
                is_standard = true;
                break;
            }
        }
        
        if (is_standard) continue;
        
        /* Count occurrences */
        int count = 0;
        for (size_t j = i; j < ctx->track_size - 1; j++) {
            uint16_t p = ((uint16_t)ctx->track_data[j] << 8) | ctx->track_data[j + 1];
            if (p == pattern) count++;
        }
        
        /* Non-standard pattern appearing 5+ times is suspicious */
        if (count >= 5 && ctx->custom_sync.num_patterns < 16) {
            bool already_recorded = false;
            for (int p = 0; p < ctx->custom_sync.num_patterns; p++) {
                if (ctx->custom_sync.patterns[p] == pattern) {
                    already_recorded = true;
                    break;
                }
            }
            
            if (!already_recorded) {
                ctx->custom_sync.patterns[ctx->custom_sync.num_patterns++] = pattern;
            }
        }
    }
    
    if (ctx->custom_sync.num_patterns > 0) {
        ctx->custom_sync.detected = true;
        ctx->detected |= UFT_PROT_UNUSUAL_SYNC;
        ctx->confidence = UFT_CONF_LOW;
    }
    
    return ctx->custom_sync.detected;
}

/*============================================================================
 * All Protection Detection
 *============================================================================*/

uft_protection_type_t uft_detect_all_protections(uft_protection_ctx_t* ctx) {
    if (!ctx) return UFT_PROT_NONE;
    
    ctx->detected = UFT_PROT_NONE;
    ctx->confidence = UFT_CONF_NONE;
    
    /* Run all detectors */
    uft_detect_copylock(ctx);
    uft_detect_speedlock(ctx);
    uft_detect_longtrack(ctx);
    uft_detect_weakbits(ctx);
    uft_detect_custom_sync(ctx);
    
    return ctx->detected;
}

/*============================================================================
 * CopyLock Reconstruction
 *============================================================================*/

size_t uft_copylock_reconstruct(uint32_t seed, uint8_t* output, bool old_style) {
    if (!output) return 0;
    
    uint32_t state = seed;
    size_t pos = 0;
    
    /* Generate CopyLock track data */
    for (int sector = 0; sector < UFT_COPYLOCK_SECTORS; sector++) {
        /* Write sync mark */
        output[pos++] = (uft_copylock_sync_marks[sector] >> 8) & 0xFF;
        output[pos++] = uft_copylock_sync_marks[sector] & 0xFF;
        
        /* Generate sector data from LFSR */
        for (int i = 0; i < 512; i++) {
            output[pos++] = state & 0xFF;
            state = uft_lfsr_next(state);
            
            if (old_style) {
                state = uft_lfsr_next(state);
            }
        }
    }
    
    return pos;
}

/*============================================================================
 * Utilities
 *============================================================================*/

const char* uft_protection_name(uft_protection_type_t type) {
    switch (type) {
        case UFT_PROT_NONE:             return "None";
        case UFT_PROT_WEAK_BITS:        return "Weak Bits";
        case UFT_PROT_FLUX_REVERSAL:    return "Flux Reversal";
        case UFT_PROT_EXTRA_SECTORS:    return "Extra Sectors";
        case UFT_PROT_MISSING_SECTORS:  return "Missing Sectors";
        case UFT_PROT_DUPLICATE_SECTORS:return "Duplicate Sectors";
        case UFT_PROT_BAD_SECTORS:      return "Bad Sectors";
        case UFT_PROT_DELETED_DATA:     return "Deleted Data";
        case UFT_PROT_NONSTANDARD_SIZE: return "Non-standard Size";
        case UFT_PROT_LONG_TRACK:       return "Long Track";
        case UFT_PROT_SHORT_TRACK:      return "Short Track";
        case UFT_PROT_HALF_TRACK:       return "Half Track";
        case UFT_PROT_EXTRA_TRACK:      return "Extra Track";
        case UFT_PROT_VARIABLE_DENSITY: return "Variable Density";
        case UFT_PROT_SPEED_VARIATION:  return "Speed Variation";
        case UFT_PROT_TIMING_BASED:     return "Timing-based";
        case UFT_PROT_NONSTANDARD_GAP:  return "Non-standard Gap";
        case UFT_PROT_UNUSUAL_SYNC:     return "Unusual Sync";
        case UFT_PROT_MIXED_FORMAT:     return "Mixed Format";
        case UFT_PROT_PROLOK:           return "ProLok";
        case UFT_PROT_SOFTGUARD:        return "SoftGuard";
        case UFT_PROT_SPIRADISC:        return "Spiradisc";
        case UFT_PROT_COPYLOCK:         return "CopyLock";
        case UFT_PROT_EVERLOCK:         return "Everlock";
        case UFT_PROT_FBCOPY:           return "Fat Bits";
        case UFT_PROT_V_MAX:            return "V-Max";
        case UFT_PROT_RAPIDLOK:         return "RapidLok";
        default:                        return "Unknown";
    }
}

void uft_protection_print(const uft_protection_ctx_t* ctx, bool verbose) {
    if (!ctx) return;
    
    printf("Protection Analysis: Track %d.%d\n", ctx->track_number, ctx->head);
    printf("  Detected: 0x%08X\n", ctx->detected);
    printf("  Confidence: %d%%\n", ctx->confidence);
    
    if (ctx->copylock.detected) {
        printf("  CopyLock: %d sectors, seed=0x%08X\n",
               ctx->copylock.num_sectors, ctx->copylock.seed);
    }
    
    if (ctx->speedlock.detected) {
        printf("  SpeedLock: variant %d\n", ctx->speedlock.variant);
    }
    
    if (ctx->longtrack.detected) {
        printf("  Long Track: %.2fms (%.1f%% of expected)\n",
               ctx->longtrack.track_length_ms, ctx->longtrack.ratio * 100);
    }
    
    if (ctx->weakbits.detected) {
        printf("  Weak Bits: %zu regions\n", ctx->weakbits.num_regions);
        if (verbose && ctx->weakbits.regions) {
            for (size_t i = 0; i < ctx->weakbits.num_regions && i < 5; i++) {
                printf("    Region %zu: offset=%u, len=%u bits\n", i,
                       ctx->weakbits.regions[i].offset,
                       ctx->weakbits.regions[i].length);
            }
        }
    }
    
    if (ctx->custom_sync.detected) {
        printf("  Custom Sync: %d patterns\n", ctx->custom_sync.num_patterns);
        if (verbose) {
            for (int i = 0; i < ctx->custom_sync.num_patterns; i++) {
                printf("    0x%04X\n", ctx->custom_sync.patterns[i]);
            }
        }
    }
}
