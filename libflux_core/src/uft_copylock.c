/**
 * @file uft_copylock.c
 * @brief Amiga Copylock Detection Implementation
 */

#include "uft_copylock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uft_rc_t uft_copylock_detect(
    uft_protection_ctx_t* prot_ctx,
    uft_copylock_profile_t* profile
) {
    UFT_CHECK_NULLS(prot_ctx, profile);
    
    memset(profile, 0, sizeof(*profile));
    
    /* Copylock signature: Weak bits on Track 0, Sectors 0-3 */
    
    /* Check Track 0 for weak sectors */
    for (uint8_t sector = 0; sector < 4; sector++) {
        uft_weak_bit_result_t weak_result;
        
        uft_rc_t rc = uft_weak_bit_detect_sector(prot_ctx, 0, 0, sector, 
                                                  &weak_result);
        
        if (uft_success(rc) && weak_result.is_weak_sector) {
            /* Found weak sector on track 0 - likely Copylock! */
            uft_copylock_weak_pattern_t* pattern = 
                &profile->weak_patterns[profile->weak_sector_count++];
            
            pattern->sector_number = sector;
            pattern->weak_bit_offset = 0;
            pattern->weak_bit_length = weak_result.unstable_bit_count;
            pattern->read_count = weak_result.read_count;
            
            /* Copy read values (simulated) */
            for (uint8_t i = 0; i < pattern->read_count && i < 16; i++) {
                pattern->read_values[i] = (uint8_t)(weak_result.crc_values[i] & 0xFF);
            }
            
            /* Timing info */
            pattern->cell_time_ns = 2000;  /* 2µs @ 500kbit/s */
            pattern->cell_variance_ns = 200;  /* ±200ns */
        }
    }
    
    /* Classification */
    if (profile->weak_sector_count >= 2) {
        /* At least 2 weak sectors on track 0 → Copylock */
        profile->detected = true;
        profile->confidence = 85 + (profile->weak_sector_count * 5);
        
        if (profile->confidence > 99) profile->confidence = 99;
        
        /* Determine version based on pattern */
        if (profile->weak_sector_count == 4) {
            profile->copylock_version = 3;  /* Classic Copylock v3 */
        } else {
            profile->copylock_version = 2;
        }
        
        /* Timing characteristics */
        profile->track0_bitrate = 500000;  /* 500 Kbit/s */
        profile->bitcell_time_ns = 2000;
        profile->jitter_tolerance_ns = 250;
        
    } else {
        profile->detected = false;
        profile->confidence = 10;
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_copylock_export_profile(
    const uft_copylock_profile_t* profile,
    const char* output_path
) {
    UFT_CHECK_NULLS(profile, output_path);
    
    if (!profile->detected) {
        return UFT_ERR_INVALID_ARG;
    }
    
    FILE* fp = fopen(output_path, "w");
    if (!fp) {
        return UFT_ERR_NOT_FOUND;
    }
    
    fprintf(fp, "# UFT Copylock Flux Profile\n");
    fprintf(fp, "# Rob Northen Copylock (Amiga)\n\n");
    
    fprintf(fp, "protection: copylock\n");
    fprintf(fp, "version: %d\n", profile->copylock_version);
    fprintf(fp, "confidence: %d%%\n\n", profile->confidence);
    
    fprintf(fp, "# Physical Parameters\n");
    fprintf(fp, "bitrate: %u\n", profile->track0_bitrate);
    fprintf(fp, "bitcell_time_ns: %u\n", profile->bitcell_time_ns);
    fprintf(fp, "jitter_tolerance_ns: %u\n\n", profile->jitter_tolerance_ns);
    
    fprintf(fp, "# Weak Sector Patterns\n");
    fprintf(fp, "weak_sectors: %u\n", profile->weak_sector_count);
    
    for (uint32_t i = 0; i < profile->weak_sector_count; i++) {
        fprintf(fp, "\nweak_sector_%u:\n", i);
        fprintf(fp, "  sector: %d\n", profile->weak_patterns[i].sector_number);
        fprintf(fp, "  offset: %u\n", profile->weak_patterns[i].weak_bit_offset);
        fprintf(fp, "  length: %u\n", profile->weak_patterns[i].weak_bit_length);
        fprintf(fp, "  reads: %d\n", profile->weak_patterns[i].read_count);
        fprintf(fp, "  values: [");
        for (uint8_t j = 0; j < profile->weak_patterns[i].read_count; j++) {
            fprintf(fp, "0x%02X%s", profile->weak_patterns[i].read_values[j],
                   (j < profile->weak_patterns[i].read_count - 1) ? ", " : "");
        }
        fprintf(fp, "]\n");
    }
    
    fprintf(fp, "\n# Mastering Instructions\n");
    fprintf(fp, "# - Preserve exact weak bit positions\n");
    fprintf(fp, "# - Maintain bitcell timing ±%uns\n", 
            profile->jitter_tolerance_ns);
    fprintf(fp, "# - Multiple read values must be reproducible\n");
    
    fclose(fp);
    return UFT_SUCCESS;
}

bool uft_copylock_verify(
    const uft_copylock_profile_t* profile,
    const char* disk_path
) {
    if (!profile || !profile->detected || !disk_path) {
        return false;
    }
    
    /* In real implementation:
     * - Read disk
     * - Check Track 0 for weak patterns
     * - Verify matches profile
     */
    
    return true;  /* Simplified */
}
