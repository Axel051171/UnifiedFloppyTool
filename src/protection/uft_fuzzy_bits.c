/**
 * @file uft_fuzzy_bits.c
 * @brief Fuzzy bit copy protection detection and analysis implementation
 * 
 * Based on Dungeon Master / Chaos Strikes Back copy protection
 * Source: http://dmweb.free.fr/community/documentation/copy-protection/copy-protection-atari-st/
 */

#include "uft_fuzzy_bits.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

/** Dungeon Master sector 7 expected markers */
static const uint8_t DM_MARKER_PACE_FB[] = { 0x07, 'P', 'A', 'C', 'E', '/', 'F', 'B' };
static const uint8_t DM_MARKER_SERI[]    = { 0x09, 'S', 'e', 'r', 'i' };
static const uint8_t DM_MARKER_FB_END[]  = { 'F', 'B' };

/** Expected fuzzy byte value (before/after flip) */
#define DM_FUZZY_BYTE_NORMAL    0x68
#define DM_FUZZY_BYTE_FLIPPED   0xE8

/*============================================================================
 * CRC Calculation
 *============================================================================*/

uint8_t uft_calc_dm_serial_crc(const uint8_t serial[4])
{
    /* CRC-8: poly=0x01, init=0x2D, refin=false, refout=false, xorout=0x00 */
    uint8_t crc = 0x2D;
    
    for (int i = 0; i < 4; i++) {
        crc ^= serial[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x01;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/*============================================================================
 * Flux Timing Analysis
 *============================================================================*/

bool uft_is_valid_mfm_timing(double timing_us, double tolerance_pct)
{
    if (tolerance_pct <= 0.0) {
        tolerance_pct = 10.0;  /* Default 10% tolerance */
    }
    
    double tol_4 = UFT_MFM_FLUX_4US * tolerance_pct / 100.0;
    double tol_6 = UFT_MFM_FLUX_6US * tolerance_pct / 100.0;
    double tol_8 = UFT_MFM_FLUX_8US * tolerance_pct / 100.0;
    
    /* Check if timing matches any valid MFM spacing */
    if (fabs(timing_us - UFT_MFM_FLUX_4US) <= tol_4) return true;
    if (fabs(timing_us - UFT_MFM_FLUX_6US) <= tol_6) return true;
    if (fabs(timing_us - UFT_MFM_FLUX_8US) <= tol_8) return true;
    
    return false;
}

bool uft_detect_dm_fuzzy_pattern(const uft_flux_timing_t *timings, size_t count)
{
    if (!timings || count < 10) {
        return false;
    }
    
    /*
     * Dungeon Master fuzzy pattern:
     * - Timing gradually shifts from 4µs to 5.5µs
     * - Then compensates: 6µs to 4.5µs
     * - Compensating pairs always sum to ~10µs
     * - Creates ambiguous timing at ~5µs boundary
     */
    
    int ambiguous_count = 0;
    int compensating_pairs = 0;
    
    for (size_t i = 0; i < count - 1; i++) {
        /* Count ambiguous timings */
        if (timings[i].is_ambiguous || uft_is_fuzzy_timing(timings[i].timing_us)) {
            ambiguous_count++;
        }
        
        /* Check for compensating pairs (sum ~10µs) */
        double pair_sum = timings[i].timing_us + timings[i + 1].timing_us;
        if (fabs(pair_sum - 10.0) < 0.5) {
            compensating_pairs++;
        }
    }
    
    /* DM pattern: high percentage of ambiguous timings with compensating pairs */
    double ambiguous_pct = (double)ambiguous_count / count * 100.0;
    double pair_pct = (double)compensating_pairs / (count / 2) * 100.0;
    
    return (ambiguous_pct > 30.0 && pair_pct > 40.0);
}

/*============================================================================
 * Serial Number Extraction
 *============================================================================*/

bool uft_extract_dm_serial(const uint8_t *sector_data, uft_dm_serial_t *serial)
{
    if (!sector_data || !serial) {
        return false;
    }
    
    memset(serial, 0, sizeof(*serial));
    
    /* Verify PACE/FB marker at offset 0 */
    if (memcmp(sector_data, DM_MARKER_PACE_FB, sizeof(DM_MARKER_PACE_FB)) != 0) {
        return false;
    }
    
    /* Verify "Seri" marker at offset 8 */
    if (memcmp(sector_data + 8, DM_MARKER_SERI, sizeof(DM_MARKER_SERI)) != 0) {
        return false;
    }
    
    /* Extract serial number (offset 0x0D, 4 bytes) */
    memcpy(serial->bytes, sector_data + 0x0D, 4);
    
    /* Extract CRC (offset 0x11, 1 byte) */
    serial->crc = sector_data[0x11];
    
    /* Verify CRC */
    uint8_t calc_crc = uft_calc_dm_serial_crc(serial->bytes);
    serial->crc_valid = (calc_crc == serial->crc);
    
    return true;
}

/*============================================================================
 * Fuzzy Sector Analysis
 *============================================================================*/

/**
 * @brief Internal: Read sector using context-provided function
 */
static int read_sector_internal(void *ctx, uint8_t track, uint8_t sector, 
                                uint8_t *buffer)
{
    /* TODO: Use actual UFT read function */
    /* This is a placeholder for integration */
    (void)ctx;
    (void)track;
    (void)sector;
    (void)buffer;
    return -1;  /* UFT_ERR_NOT_IMPLEMENTED */
}

int uft_analyze_fuzzy_sector(void *ctx, uint8_t track, uint8_t sector,
                             int read_count, uft_fuzzy_sector_t *result)
{
    if (!ctx || !result || read_count < 2) {
        return -1;  /* UFT_ERR_INVALID_PARAM */
    }
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->sector = sector;
    
    uint8_t read_buffer[512];
    uint8_t first_read[512];
    
    /* First read - establish baseline */
    int err = read_sector_internal(ctx, track, sector, first_read);
    if (err != 0) {
        return err;
    }
    
    memcpy(result->data, first_read, 512);
    
    /* Initialize byte analysis */
    for (int i = 0; i < 512; i++) {
        result->bytes[i].value_min = first_read[i];
        result->bytes[i].value_max = first_read[i];
        result->bytes[i].read_count = 1;
        result->bytes[i].variation_count = 1;
        result->bytes[i].is_fuzzy = false;
    }
    
    /* Additional reads - detect variation */
    for (int r = 1; r < read_count; r++) {
        err = read_sector_internal(ctx, track, sector, read_buffer);
        if (err != 0) {
            continue;  /* Ignore read errors, continue analysis */
        }
        
        for (int i = 0; i < 512; i++) {
            result->bytes[i].read_count++;
            
            if (read_buffer[i] < result->bytes[i].value_min) {
                result->bytes[i].value_min = read_buffer[i];
                result->bytes[i].variation_count++;
            }
            if (read_buffer[i] > result->bytes[i].value_max) {
                result->bytes[i].value_max = read_buffer[i];
                result->bytes[i].variation_count++;
            }
            
            /* Mark as fuzzy if any variation detected */
            if (result->bytes[i].value_min != result->bytes[i].value_max) {
                result->bytes[i].is_fuzzy = true;
            }
        }
    }
    
    /* Count total fuzzy bytes */
    result->fuzzy_count = 0;
    for (int i = 0; i < 512; i++) {
        if (result->bytes[i].is_fuzzy) {
            result->fuzzy_count++;
        }
    }
    
    /* Determine if copy protection is present */
    result->is_protected = (result->fuzzy_count > 0);
    
    return 0;  /* UFT_ERR_OK */
}

bool uft_has_fuzzy_bits(void *ctx, uint8_t track, uint8_t sector)
{
    uint8_t read1[512], read2[512];
    
    if (read_sector_internal(ctx, track, sector, read1) != 0) {
        return false;
    }
    
    if (read_sector_internal(ctx, track, sector, read2) != 0) {
        return false;
    }
    
    /* Any difference indicates fuzzy bits */
    return (memcmp(read1, read2, 512) != 0);
}

/*============================================================================
 * Copy Protection Detection
 *============================================================================*/

/**
 * @brief Internal: Read all sector IDs from track
 */
static int read_sector_ids_internal(void *ctx, uint8_t track,
                                    uint8_t *sector_numbers, int max_sectors)
{
    /* TODO: Use actual UFT read_address function */
    (void)ctx;
    (void)track;
    (void)sector_numbers;
    (void)max_sectors;
    return 0;
}

bool uft_has_invalid_sector_number(void *ctx, uint8_t track, uint8_t *found_sector)
{
    uint8_t sector_numbers[20];
    int count = read_sector_ids_internal(ctx, track, sector_numbers, 20);
    
    for (int i = 0; i < count; i++) {
        /* $F5, $F6, $F7 cannot be written by WD1772 */
        if (sector_numbers[i] >= 0xF5 && sector_numbers[i] <= 0xF7) {
            if (found_sector) {
                *found_sector = sector_numbers[i];
            }
            return true;
        }
    }
    
    if (found_sector) {
        *found_sector = 0;
    }
    return false;
}

int uft_detect_dm_protection(void *ctx, uft_copy_protection_t *result)
{
    if (!ctx || !result) {
        return -1;  /* UFT_ERR_INVALID_PARAM */
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Check for fuzzy bits in sector 7 */
    result->has_fuzzy_sector = uft_has_fuzzy_bits(ctx, 
        UFT_DM_FUZZY_TRACK, UFT_DM_FUZZY_SECTOR);
    
    /* Analyze fuzzy sector in detail */
    if (result->has_fuzzy_sector) {
        uft_analyze_fuzzy_sector(ctx, UFT_DM_FUZZY_TRACK, UFT_DM_FUZZY_SECTOR,
                                 5, &result->fuzzy);
    }
    
    /* Check for sector 247 */
    uint8_t invalid_sector = 0;
    result->has_sector_247 = uft_has_invalid_sector_number(ctx, 
        UFT_DM_SECTOR247_TRACK, &invalid_sector);
    
    if (result->has_sector_247 && invalid_sector == 247) {
        result->has_sector_247 = true;
    }
    
    /* Extract serial number from sector 7 data */
    if (result->has_fuzzy_sector) {
        uint8_t sector_data[512];
        if (read_sector_internal(ctx, UFT_DM_FUZZY_TRACK, 
                                 UFT_DM_FUZZY_SECTOR, sector_data) == 0) {
            result->has_fb_markers = uft_extract_dm_serial(sector_data, 
                                                           &result->serial);
            
            /* Check for "FB" end marker */
            if (sector_data[0x1FE] == 'F' && sector_data[0x1FF] == 'B') {
                result->has_fb_markers = true;
            }
        }
    }
    
    /* Determine protection type */
    if (result->has_fuzzy_sector && result->has_sector_247 && result->has_fb_markers) {
        strncpy(result->protection_type, "FTL/First Byte", 31); result->protection_type[31] = '\0';
    } else if (result->has_fuzzy_sector) {
        strncpy(result->protection_type, "Fuzzy Bits", 31); result->protection_type[31] = '\0';
    } else if (result->has_sector_247) {
        strncpy(result->protection_type, "Invalid Sector", 31); result->protection_type[31] = '\0';
    } else {
        strncpy(result->protection_type, "None", 31); result->protection_type[31] = '\0';
    }
    
    return 0;  /* UFT_ERR_OK */
}

/*============================================================================
 * Preservation Functions (Stubs - require hardware support)
 *============================================================================*/

int uft_capture_fuzzy_flux(void *ctx, uint8_t track, uint8_t sector,
                           uft_flux_timing_t *timings, size_t max_timings,
                           size_t *timing_count)
{
    /* 
     * TODO: Implement flux-level capture
     * Requires KryoFlux, SuperCard Pro, or similar hardware
     */
    (void)ctx;
    (void)track;
    (void)sector;
    (void)timings;
    (void)max_timings;
    
    if (timing_count) {
        *timing_count = 0;
    }
    
    return -2;  /* UFT_ERR_NOT_SUPPORTED */
}

int uft_write_fuzzy_flux(void *ctx, uint8_t track, uint8_t sector,
                         const uft_flux_timing_t *timings, size_t timing_count)
{
    /*
     * TODO: Implement flux-level writing
     * Requires special hardware that can control flux timing precisely
     */
    (void)ctx;
    (void)track;
    (void)sector;
    (void)timings;
    (void)timing_count;
    
    return -2;  /* UFT_ERR_NOT_SUPPORTED */
}
