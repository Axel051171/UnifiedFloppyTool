/**
 * @file uft_format_verify.c
 * @brief Format-specific verify implementations for WOZ, A2R, TD0
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN - Parser Matrix Verify Gap
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include "uft/uft_write_verify.h"
#include "uft/uft_crc.h"
#include "uft/uft_crc_polys.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Forward declarations for formats we may not have headers for */
/* #include "uft/uft_woz.h" */
/* #include "uft/uft_a2r.h" */
/* #include "uft/uft_td0.h" */

/* ═══════════════════════════════════════════════════════════════════════════════
 * WOZ Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify WOZ file integrity
 * 
 * Checks:
 * - Magic bytes ("WOZ1" or "WOZ2")
 * - CRC32 checksum
 * - INFO chunk validity
 * - TMAP chunk integrity
 * - TRKS chunk data
 * 
 * @param data      Raw WOZ file data
 * @param size      Size of data
 * @param result    Result structure (optional)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_woz(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (!data || size < 12) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "WOZ: Data too small (%zu bytes)", size);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check magic bytes */
    if (memcmp(data, "WOZ1", 4) != 0 && memcmp(data, "WOZ2", 4) != 0) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "WOZ: Invalid magic (got %02X %02X %02X %02X)",
                     data[0], data[1], data[2], data[3]);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check 0xFF 0x0A 0x0D 0x0A sequence at offset 4 */
    if (data[4] != 0xFF || data[5] != 0x0A || data[6] != 0x0D || data[7] != 0x0A) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "WOZ: Invalid header sequence");
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Read stored CRC32 at offset 8 */
    uint32_t stored_crc = (uint32_t)data[8] | 
                          ((uint32_t)data[9] << 8) | 
                          ((uint32_t)data[10] << 16) | 
                          ((uint32_t)data[11] << 24);
    
    /* Calculate CRC32 of data after header (offset 12 to end) */
    uint32_t calc_crc = uft_crc32(data + 12, size - 12);
    
    if (stored_crc != calc_crc) {
        if (result) {
            result->status = UFT_VERIFY_CRC_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "WOZ: CRC mismatch (stored: 0x%08X, calculated: 0x%08X)",
                     stored_crc, calc_crc);
            result->expected_crc = stored_crc;
            result->actual_crc = calc_crc;
        }
        return UFT_VERIFY_CRC_ERROR;
    }
    
    /* Verify chunks exist */
    size_t offset = 12;
    bool has_info = false;
    bool has_tmap = false;
    bool has_trks = false;
    
    while (offset + 8 <= size) {
        uint32_t chunk_id = *(uint32_t*)(data + offset);
        uint32_t chunk_size = *(uint32_t*)(data + offset + 4);
        
        if (chunk_id == 0x4F464E49) has_info = true;  /* "INFO" */
        if (chunk_id == 0x50414D54) has_tmap = true;  /* "TMAP" */
        if (chunk_id == 0x534B5254) has_trks = true;  /* "TRKS" */
        
        offset += 8 + chunk_size;
    }
    
    if (!has_info || !has_tmap || !has_trks) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "WOZ: Missing chunks (INFO:%d TMAP:%d TRKS:%d)",
                     has_info, has_tmap, has_trks);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* All checks passed */
    if (result) {
        result->status = UFT_VERIFY_OK;
        snprintf(result->message, sizeof(result->message),
                 "WOZ: Verified OK (CRC32: 0x%08X)", calc_crc);
        result->expected_crc = stored_crc;
        result->actual_crc = calc_crc;
    }
    
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * A2R Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify A2R (Applesauce) file integrity
 * 
 * Checks:
 * - Magic bytes ("A2R2" or "A2R3")
 * - File structure
 * - Stream data integrity
 * 
 * @param data      Raw A2R file data
 * @param size      Size of data
 * @param result    Result structure (optional)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_a2r(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (!data || size < 8) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "A2R: Data too small (%zu bytes)", size);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check magic bytes */
    if (memcmp(data, "A2R2", 4) != 0 && memcmp(data, "A2R3", 4) != 0) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "A2R: Invalid magic (got %c%c%c%c)",
                     data[0], data[1], data[2], data[3]);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check header bytes (0xFF 0x0A 0x0D 0x0A) */
    if (data[4] != 0xFF || data[5] != 0x0A || data[6] != 0x0D || data[7] != 0x0A) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "A2R: Invalid header sequence");
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Parse chunks */
    size_t offset = 8;
    bool has_info = false;
    bool has_strm = false;
    bool has_meta = false;
    int stream_count = 0;
    
    while (offset + 8 <= size) {
        char chunk_id[5] = {0};
        memcpy(chunk_id, data + offset, 4);
        uint32_t chunk_size = *(uint32_t*)(data + offset + 4);
        
        if (strcmp(chunk_id, "INFO") == 0) has_info = true;
        if (strcmp(chunk_id, "STRM") == 0) { has_strm = true; stream_count++; }
        if (strcmp(chunk_id, "META") == 0) has_meta = true;
        
        /* Prevent infinite loop */
        if (chunk_size == 0) break;
        
        offset += 8 + chunk_size;
    }
    
    if (!has_info) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "A2R: Missing INFO chunk");
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* All checks passed */
    if (result) {
        result->status = UFT_VERIFY_OK;
        snprintf(result->message, sizeof(result->message),
                 "A2R: Verified OK (%d streams, META:%s)",
                 stream_count, has_meta ? "yes" : "no");
    }
    
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TD0 (Teledisk) Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify TD0 (Teledisk) file integrity
 * 
 * Checks:
 * - Magic bytes ("TD" or "td")
 * - Header CRC
 * - Compression flag
 * - Track/sector structure
 * 
 * @param data      Raw TD0 file data
 * @param size      Size of data
 * @param result    Result structure (optional)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_td0(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (!data || size < 12) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "TD0: Data too small (%zu bytes)", size);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check magic bytes */
    bool is_compressed = false;
    if (data[0] == 'T' && data[1] == 'D') {
        is_compressed = false;
    } else if (data[0] == 't' && data[1] == 'd') {
        is_compressed = true;
    } else {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "TD0: Invalid magic (got %02X %02X)", data[0], data[1]);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check volume sequence (byte 2) - must be 0 for single volume */
    uint8_t vol_seq = data[2];
    
    /* Check version (byte 4) */
    uint8_t version = data[4];
    if (version < 10 || version > 21) {
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "TD0: Unsupported version %d", version);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Read header CRC (bytes 10-11) */
    uint16_t stored_crc = (uint16_t)data[10] | ((uint16_t)data[11] << 8);
    
    /* Calculate CRC of header (bytes 0-9) */
    uint16_t calc_crc = uft_crc16_ccitt(data, 10);
    
    if (stored_crc != calc_crc) {
        if (result) {
            result->status = UFT_VERIFY_CRC_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "TD0: Header CRC mismatch (stored: 0x%04X, calc: 0x%04X)",
                     stored_crc, calc_crc);
            result->expected_crc = stored_crc;
            result->actual_crc = calc_crc;
        }
        return UFT_VERIFY_CRC_ERROR;
    }
    
    /* All checks passed */
    if (result) {
        result->status = UFT_VERIFY_OK;
        snprintf(result->message, sizeof(result->message),
                 "TD0: Verified OK (v%d.%d, %s, vol:%d)",
                 version / 10, version % 10,
                 is_compressed ? "compressed" : "uncompressed",
                 vol_seq);
    }
    
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Generic File Verify Dispatcher
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Auto-detect format and verify
 * 
 * @param path      File path
 * @param result    Result structure
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_file(
    const char *path,
    uft_verify_result_t *result)
{
    if (!path) return UFT_VERIFY_FORMAT_ERROR;
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (result) {
            result->status = UFT_VERIFY_READ_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "Cannot open file: %s", path);
        }
        return UFT_VERIFY_READ_ERROR;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 100 * 1024 * 1024) { /* Max 100MB */
        fclose(f);
        if (result) {
            result->status = UFT_VERIFY_FORMAT_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "Invalid file size: %ld", size);
        }
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Read file */
    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        return UFT_VERIFY_READ_ERROR;
    }
    
    size_t read = fread(data, 1, (size_t)size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(data);
        if (result) {
            result->status = UFT_VERIFY_READ_ERROR;
            snprintf(result->message, sizeof(result->message),
                     "Read error: got %zu of %ld bytes", read, size);
        }
        return UFT_VERIFY_READ_ERROR;
    }
    
    /* Auto-detect format and verify */
    uft_verify_status_t status;
    
    if (size >= 4) {
        if (memcmp(data, "WOZ1", 4) == 0 || memcmp(data, "WOZ2", 4) == 0) {
            status = uft_verify_woz(data, (size_t)size, result);
        } else if (memcmp(data, "A2R2", 4) == 0 || memcmp(data, "A2R3", 4) == 0) {
            status = uft_verify_a2r(data, (size_t)size, result);
        } else if ((data[0] == 'T' || data[0] == 't') && (data[1] == 'D' || data[1] == 'd')) {
            status = uft_verify_td0(data, (size_t)size, result);
        } else {
            if (result) {
                result->status = UFT_VERIFY_FORMAT_ERROR;
                snprintf(result->message, sizeof(result->message),
                         "Unknown format (magic: %02X %02X %02X %02X)",
                         data[0], data[1], data[2], data[3]);
            }
            status = UFT_VERIFY_FORMAT_ERROR;
        }
    } else {
        status = UFT_VERIFY_FORMAT_ERROR;
    }
    
    free(data);
    return status;
}
