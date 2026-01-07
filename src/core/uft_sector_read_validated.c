/**
 * @file uft_sector_read_validated.c
 * @brief Validated Sector Read with CRC Checking (P1-002)
 * 
 * Combines sector reading with automatic CRC validation.
 * Provides statistics and error tracking for forensic analysis.
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#include "uft_sector_read_validated.h"
#include "uft/core/uft_crc_validate.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Static Helpers
 *===========================================================================*/

static uft_crc_type_t encoding_to_crc_type(uft_encoding_id_t encoding) {
    switch (encoding) {
        case UFT_ENC_MFM:
        case UFT_ENC_FM:
            return UFT_CRC_CCITT;
        case UFT_ENC_GCR_CBM:
        case UFT_ENC_GCR_APPLE:
            return UFT_CRC_CHECKSUM;
        default:
            return UFT_CRC_CCITT;
    }
}

/*===========================================================================
 * Main Functions
 *===========================================================================*/

void uft_validated_reader_init(uft_validated_reader_t *reader) {
    if (!reader) return;
    
    memset(reader, 0, sizeof(*reader));
    reader->validate_crc = true;
    reader->collect_stats = true;
    reader->retry_on_crc_error = true;
    reader->max_retries = 3;
    
    uft_crc_stats_init(&reader->stats);
}

void uft_validated_reader_reset_stats(uft_validated_reader_t *reader) {
    if (!reader) return;
    uft_crc_stats_init(&reader->stats);
}

uft_error_t uft_sector_read_validated(
    uft_validated_reader_t *reader,
    uft_disk_t *disk,
    int cylinder,
    int head, 
    int sector,
    uint8_t *buffer,
    size_t buffer_size,
    uft_sector_result_t *result)
{
    if (!reader || !disk || !buffer || !result) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    memset(result, 0, sizeof(*result));
    result->cylinder = cylinder;
    result->head = head;
    result->sector = sector;
    
    int retries = 0;
    uft_error_t err;
    
    do {
        /* Read raw sector data */
        size_t bytes_read = 0;
        err = uft_disk_read_raw_sector(disk, cylinder, head, sector,
                                       buffer, buffer_size, &bytes_read);
        
        if (err != UFT_OK) {
            result->status = UFT_SECTOR_READ_ERROR;
            result->error_code = err;
            retries++;
            continue;
        }
        
        result->data_size = bytes_read;
        
        /* CRC Validation */
        if (reader->validate_crc && bytes_read >= 2) {
            uft_crc_type_t crc_type = encoding_to_crc_type(disk->encoding);
            
            /* Extract CRC from end of sector data */
            size_t data_len = bytes_read - 2;
            const uint8_t *crc_bytes = buffer + data_len;
            
            uft_crc_result_t crc_result;
            bool crc_ok = uft_validate_sector_crc(buffer, data_len, 
                                                   crc_bytes, &crc_result);
            
            result->crc_expected = crc_result.expected;
            result->crc_calculated = crc_result.calculated;
            result->crc_valid = crc_ok;
            
            /* Update statistics */
            if (reader->collect_stats) {
                if (crc_ok) {
                    reader->stats.sectors_valid++;
                } else {
                    reader->stats.sectors_invalid++;
                }
                reader->stats.sectors_checked++;
            }
            
            if (!crc_ok) {
                result->status = UFT_SECTOR_CRC_ERROR;
                retries++;
                
                if (!reader->retry_on_crc_error) {
                    break;
                }
                continue;
            }
        }
        
        /* Success */
        result->status = UFT_SECTOR_OK;
        return UFT_OK;
        
    } while (retries < reader->max_retries);
    
    /* All retries exhausted */
    result->retries_used = retries;
    return (result->status == UFT_SECTOR_CRC_ERROR) ? 
           UFT_ERROR_CRC : UFT_ERROR_FILE_READ;
}

uft_error_t uft_track_read_validated(
    uft_validated_reader_t *reader,
    uft_disk_t *disk,
    int cylinder,
    int head,
    uft_track_result_t *result)
{
    if (!reader || !disk || !result) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    memset(result, 0, sizeof(*result));
    result->cylinder = cylinder;
    result->head = head;
    
    /* Get track geometry */
    int sectors_per_track = disk->sectors_per_track;
    if (sectors_per_track <= 0) {
        sectors_per_track = 18;  /* Default */
    }
    
    uint8_t sector_buffer[8192];  /* Max sector size */
    uft_sector_result_t sector_result;
    
    for (int s = 0; s < sectors_per_track; s++) {
        uft_error_t err = uft_sector_read_validated(
            reader, disk, cylinder, head, s,
            sector_buffer, sizeof(sector_buffer),
            &sector_result);
        
        result->sectors_read++;
        
        if (err == UFT_OK && sector_result.crc_valid) {
            result->sectors_valid++;
        } else {
            result->sectors_with_errors++;
            
            if (result->error_count < UFT_MAX_TRACK_ERRORS) {
                result->error_sectors[result->error_count] = s;
                result->error_codes[result->error_count] = err;
                result->error_count++;
            }
        }
    }
    
    /* Calculate validity percentage */
    if (result->sectors_read > 0) {
        result->validity_percent = 
            (float)result->sectors_valid * 100.0f / (float)result->sectors_read;
    }
    
    return (result->sectors_valid == result->sectors_read) ? 
           UFT_OK : UFT_ERROR_BAD_SECTOR;
}

void uft_validated_reader_get_stats(
    const uft_validated_reader_t *reader,
    uft_crc_stats_t *stats)
{
    if (!reader || !stats) return;
    *stats = reader->stats;
}

double uft_validated_reader_success_rate(const uft_validated_reader_t *reader) {
    if (!reader || reader->stats.sectors_checked == 0) {
        return 0.0;
    }
    return uft_crc_stats_validity_pct(&reader->stats);
}

void uft_sector_result_print(const uft_sector_result_t *result, FILE *out) {
    if (!result || !out) return;
    
    fprintf(out, "Sector C%d:H%d:S%d: ", 
            result->cylinder, result->head, result->sector);
    
    switch (result->status) {
        case UFT_SECTOR_OK:
            fprintf(out, "OK");
            break;
        case UFT_SECTOR_CRC_ERROR:
            fprintf(out, "CRC ERROR (expected %04X, got %04X)",
                    result->crc_expected, result->crc_calculated);
            break;
        case UFT_SECTOR_READ_ERROR:
            fprintf(out, "READ ERROR (code %d)", result->error_code);
            break;
        case UFT_SECTOR_NOT_FOUND:
            fprintf(out, "NOT FOUND");
            break;
        default:
            fprintf(out, "UNKNOWN STATUS");
    }
    
    if (result->retries_used > 0) {
        fprintf(out, " [%d retries]", result->retries_used);
    }
    
    fprintf(out, "\n");
}

void uft_track_result_print(const uft_track_result_t *result, FILE *out) {
    if (!result || !out) return;
    
    fprintf(out, "Track C%d:H%d: %d/%d valid (%.1f%%)",
            result->cylinder, result->head,
            result->sectors_valid, result->sectors_read,
            result->validity_percent);
    
    if (result->error_count > 0) {
        fprintf(out, " - Errors on sectors:");
        for (int i = 0; i < result->error_count && i < 10; i++) {
            fprintf(out, " %d", result->error_sectors[i]);
        }
        if (result->error_count > 10) {
            fprintf(out, "...");
        }
    }
    
    fprintf(out, "\n");
}
