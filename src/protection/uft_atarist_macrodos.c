/**
 * @file uft_atarist_macrodos.c
 * @brief Atari ST Macrodos Protection Implementation
 */

#include "uft/protection/uft_atarist_macrodos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard Atari ST sector gaps */
#define ST_GAP1_SIZE    60      /**< Post-index gap */
#define ST_GAP2_SIZE    22      /**< Post-ID gap */
#define ST_GAP3_SIZE    40      /**< Post-data gap */
#define ST_GAP4_SIZE    652     /**< Pre-index gap */

/* Macrodos signatures */
static const uint8_t MACRODOS_SIG_V1[] = { 0x4D, 0x41, 0x43 };  /* 'MAC' */
static const uint8_t MACRODOS_SIG_V2[] = { 0x4D, 0x44, 0x32 };  /* 'MD2' */
static const uint8_t MACRODOS_SIG_V3[] = { 0x4D, 0x44, 0x33 };  /* 'MD3' */

/* Standard data marks */
#define ST_ID_MARK      0xFE
#define ST_DATA_MARK    0xFB
#define ST_DEL_MARK     0xF8

/* Macrodos modified marks */
#define MACRO_DATA_MARK 0xFA    /**< Non-standard data mark */

/*===========================================================================
 * MFM Helpers
 *===========================================================================*/

/**
 * @brief Find sync pattern in MFM data
 */
static int find_sync(const uint8_t *data, size_t size, size_t start)
{
    /* Look for A1 A1 A1 pattern */
    for (size_t i = start; i < size - 3; i++) {
        if (data[i] == 0xA1 && data[i+1] == 0xA1 && data[i+2] == 0xA1) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Calculate CRC16-CCITT
 */
static uint16_t calc_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_macrodos_detect_track(const uint8_t *track_data, size_t size,
                              uint8_t track, uint8_t side,
                              uft_macrodos_track_t *result)
{
    if (!track_data || !result || size < 512) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->side = side;
    result->total_bits = size * 8;
    
    /* Scan for sectors */
    int pos = 0;
    while (result->sector_count < 11 && pos >= 0) {
        pos = find_sync(track_data, size, pos);
        if (pos < 0 || pos + 10 >= (int)size) break;
        
        uint8_t mark = track_data[pos + 3];
        
        if (mark == ST_ID_MARK) {
            /* ID field found */
            uft_macrodos_sector_t *sec = &result->sectors[result->sector_count];
            
            sec->track = track_data[pos + 4];
            sec->side = track_data[pos + 5];
            sec->sector = track_data[pos + 6];
            sec->size_code = track_data[pos + 7];
            sec->id_mark = mark;
            sec->crc_id = (track_data[pos + 8] << 8) | track_data[pos + 9];
            sec->position_bits = pos * 8;
            
            /* Verify ID CRC */
            uint16_t calc_crc = calc_crc16(track_data + pos + 3, 5);
            sec->crc_valid = (calc_crc == sec->crc_id);
            
            /* Look for data mark */
            int data_pos = find_sync(track_data, size, pos + 10);
            if (data_pos > 0 && data_pos < pos + 100) {
                sec->data_mark = track_data[data_pos + 3];
                sec->gap_before = (data_pos - pos - 10) / 2;
                
                /* Check for non-standard data mark */
                if (sec->data_mark == MACRO_DATA_MARK) {
                    result->has_modified_marks = true;
                }
            }
            
            result->sector_count++;
        }
        
        pos++;
    }
    
    /* Analyze gap pattern */
    if (result->sector_count >= 2) {
        for (int i = 0; i < result->sector_count - 1; i++) {
            int gap = result->sectors[i+1].position_bits - 
                      result->sectors[i].position_bits - (512 * 8);
            gap = gap / 8;  /* Convert to bytes */
            
            /* Non-standard gap indicates protection */
            if (gap < ST_GAP3_SIZE - 10 || gap > ST_GAP3_SIZE + 10) {
                result->has_custom_gaps = true;
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Full Disk Analysis
 *===========================================================================*/

int uft_macrodos_analyze_disk(const uint8_t *disk_data, size_t size,
                              uft_macrodos_result_t *result)
{
    if (!disk_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    float confidence = 0.0f;
    
    /* Search for Macrodos signatures */
    for (size_t i = 0; i < size - 3 && i < 512; i++) {
        if (memcmp(disk_data + i, MACRODOS_SIG_V3, 3) == 0) {
            result->version = UFT_MACRODOS_V3;
            confidence += 0.4f;
            break;
        }
        if (memcmp(disk_data + i, MACRODOS_SIG_V2, 3) == 0) {
            result->version = UFT_MACRODOS_V2;
            confidence += 0.35f;
            break;
        }
        if (memcmp(disk_data + i, MACRODOS_SIG_V1, 3) == 0) {
            result->version = UFT_MACRODOS_V1;
            confidence += 0.3f;
            break;
        }
    }
    
    /* Analyze tracks for protection features */
    size_t track_size = 9 * 512;  /* 9 sectors * 512 bytes */
    int tracks = (size - 512) / track_size;  /* Skip boot sector */
    
    for (int t = 0; t < tracks && t < 80; t++) {
        for (int s = 0; s < 2; s++) {
            uft_macrodos_track_t track_info;
            size_t offset = 512 + (t * 2 + s) * track_size;
            
            if (offset + track_size > size) continue;
            
            if (uft_macrodos_detect_track(disk_data + offset, track_size,
                                          t, s, &track_info) == 0) {
                if (track_info.has_custom_gaps) {
                    result->protected_tracks++;
                    confidence += 0.1f;
                    
                    if (result->technique_count < 8) {
                        result->techniques[result->technique_count++] = 
                            UFT_MACRO_TECH_SECTOR_GAP;
                    }
                }
                
                if (track_info.has_modified_marks) {
                    confidence += 0.15f;
                    
                    if (result->technique_count < 8) {
                        result->techniques[result->technique_count++] = 
                            UFT_MACRO_TECH_DATA_MARK;
                    }
                }
            }
        }
    }
    
    /* Limit confidence to 1.0 */
    if (confidence > 1.0f) confidence = 1.0f;
    
    result->confidence = confidence;
    result->detected = confidence >= 0.4f;
    
    if (result->detected) {
        result->key_track = 0;  /* Usually boot track */
        result->key_side = 0;
    }
    
    return 0;
}

/*===========================================================================
 * Gap Analysis
 *===========================================================================*/

int uft_macrodos_analyze_gaps(const uft_macrodos_track_t *track,
                              uint16_t *pattern, bool *is_protected)
{
    if (!track || !pattern || !is_protected) return -1;
    
    *is_protected = false;
    
    for (int i = 0; i < track->sector_count && i < 9; i++) {
        pattern[i] = track->sectors[i].gap_after;
        
        /* Check for non-standard gap */
        if (pattern[i] < ST_GAP3_SIZE - 10 || pattern[i] > ST_GAP3_SIZE + 10) {
            *is_protected = true;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Timing Detection
 *===========================================================================*/

int uft_macrodos_detect_timing(const uint32_t *flux_intervals, size_t count,
                               uint32_t *read_times, size_t max_sectors,
                               float *timing_score)
{
    if (!flux_intervals || !read_times || !timing_score) return -1;
    
    *timing_score = 0.0f;
    
    /* Calculate cumulative time for each sector */
    uint64_t cumulative = 0;
    size_t sector_idx = 0;
    uint32_t sector_start = 0;
    
    /* Look for sector boundaries (sync patterns) */
    int consecutive_short = 0;
    
    for (size_t i = 0; i < count && sector_idx < max_sectors; i++) {
        cumulative += flux_intervals[i];
        
        /* Detect sync pattern (short intervals) */
        if (flux_intervals[i] < 2000) {  /* <2µs indicates sync */
            consecutive_short++;
            if (consecutive_short >= 3) {
                /* End of sector, record time */
                read_times[sector_idx++] = cumulative - sector_start;
                sector_start = cumulative;
                consecutive_short = 0;
            }
        } else {
            consecutive_short = 0;
        }
    }
    
    /* Analyze timing variance */
    if (sector_idx >= 2) {
        uint32_t min_time = UINT32_MAX, max_time = 0;
        
        for (size_t i = 0; i < sector_idx; i++) {
            if (read_times[i] < min_time) min_time = read_times[i];
            if (read_times[i] > max_time) max_time = read_times[i];
        }
        
        float variance = (float)(max_time - min_time) / min_time;
        
        /* High variance suggests timing protection */
        if (variance > 0.1f) {
            *timing_score = variance;
        }
    }
    
    return sector_idx;
}

/*===========================================================================
 * Encryption
 *===========================================================================*/

int uft_macrodos_extract_key(const uint8_t *key_sector, size_t size,
                             uint32_t *seed, uint8_t *key, size_t key_size)
{
    if (!key_sector || !seed || !key || size < 512) return -1;
    
    /* Macrodos key is typically in first sector */
    /* Seed at offset 0x100, key at 0x110 */
    if (size >= 0x120) {
        *seed = (key_sector[0x100] << 24) | (key_sector[0x101] << 16) |
                (key_sector[0x102] << 8) | key_sector[0x103];
        
        size_t copy_size = key_size < 16 ? key_size : 16;
        memcpy(key, key_sector + 0x110, copy_size);
        
        return 0;
    }
    
    return -1;
}

int uft_macrodos_decrypt(const uint8_t *encrypted, size_t size,
                         const uint8_t *key, size_t key_size,
                         uint8_t *decrypted)
{
    if (!encrypted || !key || !decrypted) return -1;
    
    /* Macrodos uses XOR with rotating key and offset */
    for (size_t i = 0; i < size; i++) {
        decrypted[i] = encrypted[i] ^ key[i % key_size] ^ (uint8_t)(i & 0xFF);
    }
    
    return 0;
}

/*===========================================================================
 * Checksum
 *===========================================================================*/

bool uft_macrodos_verify_checksum(const uint8_t *sector_data, size_t size,
                                  uint16_t expected)
{
    if (!sector_data || size < 512) return false;
    
    /* Macrodos uses modified CRC or simple sum */
    uint16_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += sector_data[i];
        sum = (sum << 1) | (sum >> 15);  /* Rotate left */
    }
    
    return sum == expected;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

const char *uft_macrodos_version_name(uft_macrodos_version_t version)
{
    switch (version) {
        case UFT_MACRODOS_V1:   return "Macrodos v1";
        case UFT_MACRODOS_V2:   return "Macrodos v2";
        case UFT_MACRODOS_V3:   return "Macrodos v3";
        case UFT_MACRODOS_PLUS: return "Macrodos+";
        default:               return "Unknown";
    }
}

const char *uft_macrodos_technique_name(uft_macrodos_technique_t tech)
{
    switch (tech) {
        case UFT_MACRO_TECH_NONE:         return "None";
        case UFT_MACRO_TECH_SECTOR_GAP:   return "Sector Gap";
        case UFT_MACRO_TECH_TRACK_TIMING: return "Track Timing";
        case UFT_MACRO_TECH_DATA_MARK:    return "Data Mark";
        case UFT_MACRO_TECH_CHECKSUM:     return "Checksum";
        case UFT_MACRO_TECH_ENCRYPTION:   return "Encryption";
        default:                          return "Unknown";
    }
}

int uft_macrodos_report_json(const uft_macrodos_result_t *result,
                             char *buffer, size_t size)
{
    if (!result || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"detected\": %s,\n"
        "  \"version\": \"%s\",\n"
        "  \"confidence\": %.4f,\n"
        "  \"key_track\": %u,\n"
        "  \"key_side\": %u,\n"
        "  \"uses_encryption\": %s,\n"
        "  \"technique_count\": %u,\n"
        "  \"protected_tracks\": %u,\n"
        "  \"protected_sectors\": %u\n"
        "}",
        result->detected ? "true" : "false",
        uft_macrodos_version_name(result->version),
        result->confidence,
        result->key_track,
        result->key_side,
        result->uses_encryption ? "true" : "false",
        result->technique_count,
        result->protected_tracks,
        result->protected_sectors
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
