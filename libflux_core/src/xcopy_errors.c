// SPDX-License-Identifier: MIT
/*
 * xcopy_errors.c - X-Copy Error Taxonomy Implementation
 * 
 * Based on original X-Copy source code (xio.s lines 2613-2632)
 * Converted from 68000 assembler to C
 */

#include "uft/uft_error.h"
#include "xcopy_errors.h"
#include <stdio.h>
#include <string.h>

/*============================================================================*
 * ERROR MESSAGES (Original X-Copy strings)
 *============================================================================*/

static const char* error_messages_en[] = {
    "No error",
    "1: More or less than 11 sectors!",
    "2: No sync found!",
    "3: No sync after gap found!",
    "4: Header checksum error!",
    "5: Error in header/format long!",
    "6: Datablock checksum error!",
    "7: Long track!",                    // ⭐ COPY PROTECTION!
    "8: Verify error!"
};

static const char* error_messages_de[] = {
    "Kein Fehler",
    "1: Weniger oder mehr als 11 Sektoren!",
    "2: Kein Sync gefunden!",
    "3: Kein Sync nach GAP gefunden!",
    "4: Header Prüfsummen Fehler!",
    "5: Fehler im Header/Format Long!",
    "6: Datenblock Prüfsummen Fehler!",
    "7: Langer Track!",                  // ⭐ KOPIERSCHUTZ!
    "8: Verify Fehler!"
};

/*============================================================================*
 * ERROR MESSAGE RETRIEVAL
 *============================================================================*/

const char* xcopy_error_message(xcopy_error_t error)
{
    if (error < 0 || error > 8) {
        return "Unknown error";
    }
    return error_messages_en[error];
}

const char* xcopy_error_message_de(xcopy_error_t error)
{
    if (error < 0 || error > 8) {
        return "Unbekannter Fehler";
    }
    return error_messages_de[error];
}

/*============================================================================*
 * TRACK ANALYSIS (FULL IMPLEMENTATION)
 *============================================================================*/

#include "mfm_decode.h"

/**
 * @brief Check gap timing between sectors
 */
static bool check_gap_timing(
    const uint32_t *sync_positions,
    int sync_count,
    uint32_t *min_gap_out,
    uint32_t *max_gap_out
) {
    if (sync_count < 2) {
        return false;
    }
    
    uint32_t min_gap = 0xFFFFFFFF;
    uint32_t max_gap = 0;
    
    for (int i = 0; i < sync_count - 1; i++) {
        uint32_t gap = sync_positions[i + 1] - sync_positions[i];
        
        if (gap < min_gap) min_gap = gap;
        if (gap > max_gap) max_gap = gap;
    }
    
    if (min_gap_out) *min_gap_out = min_gap;
    if (max_gap_out) *max_gap_out = max_gap;
    
    /* Check if gaps are reasonable (200-2000 bytes) */
    return (min_gap >= 200 && max_gap <= 2000);
}

/**
 * @brief Full X-Copy style track analysis with MFM decoding
 */
int xcopy_analyze_track(
    const uint8_t *track_data,
    size_t track_length,
    xcopy_track_error_t *error)
{
    if (!track_data || !error) {
        return -1;
    }
    
    /* Initialize error structure */
    memset(error, 0, sizeof(*error));
    error->track_length = track_length;
    error->expected_length = 11 * 544 + 700 * 11;  /* 11 sectors + gaps */
    error->expected_sectors = 11;
    
    /* Use MFM decoder for full analysis */
    mfm_track_t mfm_track;
    if (mfm_analyze_track(track_data, track_length, &mfm_track) < 0) {
        error->error_code = XCOPY_ERR_NO_SYNC;
        error->sync_found = false;
        return 0;
    }
    
    /* Check if sync marks were found */
    if (mfm_track.sync_count == 0) {
        error->error_code = XCOPY_ERR_NO_SYNC;
        error->sync_found = false;
        error->is_protected = true;  /* No sync = unusual */
        return 0;
    }
    
    error->sync_found = true;
    error->sector_count = mfm_track.sector_count;
    
    /* Check sector count (X-Copy Error 1) */
    if (mfm_track.sector_count != 11) {
        error->error_code = XCOPY_ERR_SECTOR_COUNT;
        error->is_protected = true;  /* Unusual sector count = copy protection! */
        return 0;
    }
    
    /* Check gap timing (X-Copy Error 3) */
    uint32_t min_gap, max_gap;
    if (!check_gap_timing(mfm_track.sync_positions, mfm_track.sync_count,
                         &min_gap, &max_gap)) {
        error->error_code = XCOPY_ERR_GAP_SYNC;
        error->gap_valid = false;
        error->is_protected = true;
        return 0;
    }
    error->gap_valid = true;
    
    /* Check for CRC errors */
    error->crc_errors = mfm_track.crc_errors;
    
    if (error->crc_errors > 0) {
        /* Determine if header or data CRC error */
        bool has_header_error = false;
        bool has_data_error = false;
        
        for (int i = 0; i < mfm_track.sector_count; i++) {
            if (!mfm_track.sectors[i].header_checksum_valid) {
                has_header_error = true;
            }
            if (!mfm_track.sectors[i].data_checksum_valid) {
                has_data_error = true;
            }
        }
        
        if (has_header_error) {
            error->error_code = XCOPY_ERR_HEADER_CRC;
            return 0;
        }
        
        if (has_data_error) {
            error->error_code = XCOPY_ERR_DATA_CRC;
            return 0;
        }
    }
    
    /* Check for long track (X-Copy Error 7 - COPY PROTECTION!) */
    if (mfm_track.has_long_track || track_length > error->expected_length * 1.15) {
        error->error_code = XCOPY_ERR_LONG_TRACK;
        error->is_protected = true;  /* Long track = Rob Northen copylock! */
        return 0;
    }
    
    /* Verify sector format fields */
    for (int i = 0; i < mfm_track.sector_count; i++) {
        mfm_sector_t *sector = &mfm_track.sectors[i];
        
        /* Check format byte (should be 0xFF) */
        if (sector->format_type != 0xFF) {
            error->error_code = XCOPY_ERR_HEADER_FMT;
            error->is_protected = true;
            return 0;
        }
        
        /* Check sector number range */
        if (sector->sector_number >= 11) {
            error->error_code = XCOPY_ERR_HEADER_FMT;
            error->is_protected = true;
            return 0;
        }
    }
    
    /* All checks passed - clean track! */
    error->error_code = XCOPY_ERR_NONE;
    error->is_protected = false;
    
    return 0;
}

/*============================================================================*
 * UFM INTEGRATION
 *============================================================================*/

uint32_t xcopy_error_to_ufm_flags(xcopy_error_t error)
{
    // Map X-Copy errors to UFM copy-protection flags
    // From flux_core.h:
    //   UFM_CP_WEAK_BITS     = 1u << 0
    //   UFM_CP_LONGTRACK     = 1u << 1
    //   UFM_CP_SHORTTRACK    = 1u << 2
    //   UFM_CP_BAD_CRC       = 1u << 3
    //   UFM_CP_DUP_IDS       = 1u << 4
    //   UFM_CP_NONSTD_GAP    = 1u << 5
    //   UFM_CP_SYNC_ANOMALY  = 1u << 6
    
    uint32_t flags = 0;
    
    switch (error) {
        case XCOPY_ERR_LONG_TRACK:
            flags |= (1u << 1);  // UFM_CP_LONGTRACK
            break;
            
        case XCOPY_ERR_SECTOR_COUNT:
            // Non-standard sector count could indicate various protections
            flags |= (1u << 1);  // UFM_CP_LONGTRACK (related)
            break;
            
        case XCOPY_ERR_NO_SYNC:
        case XCOPY_ERR_GAP_SYNC:
            flags |= (1u << 6);  // UFM_CP_SYNC_ANOMALY
            break;
            
        case XCOPY_ERR_HEADER_CRC:
        case XCOPY_ERR_DATA_CRC:
            flags |= (1u << 3);  // UFM_CP_BAD_CRC
            break;
            
        case XCOPY_ERR_HEADER_FMT:
            flags |= (1u << 5);  // UFM_CP_NONSTD_GAP
            break;
            
        default:
            break;
    }
    
    return flags;
}

int xcopy_analyze_for_ufm(
    const uint8_t *track_data,
    size_t track_length,
    uint32_t *cp_flags)
{
    xcopy_track_error_t error;
    
    int ret = xcopy_analyze_track(track_data, track_length, &error);
    if (ret != 0) {
        return ret;
    }
    
    *cp_flags = xcopy_error_to_ufm_flags(error.error_code);
    
    return 0;
}

/*============================================================================*
 * ERROR STATISTICS
 *============================================================================*/

void xcopy_stats_init(xcopy_error_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
}

void xcopy_stats_add(xcopy_error_stats_t *stats, 
                     const xcopy_track_error_t *error)
{
    stats->total_tracks++;
    
    if (error->error_code >= 0 && error->error_code <= 8) {
        stats->error_counts[error->error_code]++;
    }
    
    if (error->is_protected) {
        stats->protected_tracks++;
    }
    
    if (error->error_code == XCOPY_ERR_NONE) {
        stats->clean_tracks++;
    }
}

void xcopy_stats_print(const xcopy_error_stats_t *stats)
{
    printf("X-Copy Error Statistics:\n");
    printf("═══════════════════════════════════════════\n");
    printf("Total tracks:     %u\n", stats->total_tracks);
    printf("Clean tracks:     %u (%.1f%%)\n", 
           stats->clean_tracks,
           stats->total_tracks > 0 ? 
               (100.0f * stats->clean_tracks / stats->total_tracks) : 0.0f);
    printf("Protected tracks: %u (%.1f%%)\n",
           stats->protected_tracks,
           stats->total_tracks > 0 ?
               (100.0f * stats->protected_tracks / stats->total_tracks) : 0.0f);
    printf("\n");
    printf("Error breakdown:\n");
    
    for (int i = 1; i <= 8; i++) {
        if (stats->error_counts[i] > 0) {
            printf("  %s: %u\n",
                   xcopy_error_message(i),
                   stats->error_counts[i]);
        }
    }
    
    printf("═══════════════════════════════════════════\n");
}
