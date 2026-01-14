/**
 * @file uft_format_verify.c
 * @brief Format-specific verify functions for WOZ, A2R, TD0
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN - Parser Matrix Verify Gap
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include "uft/uft_format_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-32 Implementation (IEEE 802.3)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t format_verify_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-16 Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint16_t format_verify_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * WOZ Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_woz(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    /* Initialize result if provided */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* Check minimum size */
    if (!data || size < 12) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check magic bytes: "WOZ1" or "WOZ2" */
    if (memcmp(data, "WOZ1", 4) != 0 && memcmp(data, "WOZ2", 4) != 0) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check header bytes: 0xFF 0x0A 0x0D 0x0A */
    if (data[4] != 0xFF || data[5] != 0x0A || 
        data[6] != 0x0D || data[7] != 0x0A) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Extract stored CRC32 (bytes 8-11, little-endian) */
    uint32_t stored_crc = (uint32_t)data[8] |
                          ((uint32_t)data[9] << 8) |
                          ((uint32_t)data[10] << 16) |
                          ((uint32_t)data[11] << 24);
    
    /* Calculate CRC32 of data after header (bytes 12 to end) */
    if (size > 12) {
        uint32_t calc_crc = format_verify_crc32(data + 12, size - 12);
        
        if (stored_crc != calc_crc) {
            if (result) result->status = UFT_VERIFY_CRC_ERROR;
            return UFT_VERIFY_CRC_ERROR;
        }
    }
    
    /* Look for INFO chunk (must be first) */
    if (size >= 20) {
        if (memcmp(data + 12, "INFO", 4) != 0) {
            if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
            return UFT_VERIFY_FORMAT_ERROR;
        }
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * A2R Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_a2r(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* Check minimum size */
    if (!data || size < 8) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check magic bytes: "A2R2" or "A2R3" */
    if (memcmp(data, "A2R2", 4) != 0 && memcmp(data, "A2R3", 4) != 0) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check header bytes: 0xFF 0x0A 0x0D 0x0A */
    if (data[4] != 0xFF || data[5] != 0x0A || 
        data[6] != 0x0D || data[7] != 0x0A) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Look for INFO chunk */
    if (size >= 16) {
        /* A2R has INFO chunk after header */
        bool found_info = false;
        for (size_t i = 8; i + 4 <= size && i < 64; i++) {
            if (memcmp(data + i, "INFO", 4) == 0) {
                found_info = true;
                break;
            }
        }
        if (!found_info) {
            if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
            return UFT_VERIFY_FORMAT_ERROR;
        }
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TD0 Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_td0(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* Check minimum size */
    if (!data || size < 12) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check magic bytes: "TD" (normal) or "td" (compressed) */
    if (!((data[0] == 'T' && data[1] == 'D') ||
          (data[0] == 't' && data[1] == 'd'))) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check version (byte 4) - should be 10, 11, 20, or 21 */
    uint8_t version = data[4];
    if (version != 10 && version != 11 && version != 20 && version != 21) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Verify header CRC16 (bytes 0-10, CRC at 10-11) */
    uint16_t stored_crc = (uint16_t)data[10] | ((uint16_t)data[11] << 8);
    uint16_t calc_crc = format_verify_crc16(data, 10);
    
    if (stored_crc != calc_crc) {
        if (result) result->status = UFT_VERIFY_CRC_ERROR;
        return UFT_VERIFY_CRC_ERROR;
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Generic File Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_file(
    const char *path,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    if (!path) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Open file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (result) result->status = UFT_VERIFY_READ_ERROR;
        return UFT_VERIFY_READ_ERROR;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {
        fclose(f);
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Read file */
    uint8_t *data = (uint8_t *)malloc((size_t)file_size);
    if (!data) {
        fclose(f);
        if (result) result->status = UFT_VERIFY_READ_ERROR;
        return UFT_VERIFY_READ_ERROR;
    }
    
    size_t read = fread(data, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        free(data);
        if (result) result->status = UFT_VERIFY_READ_ERROR;
        return UFT_VERIFY_READ_ERROR;
    }
    
    /* Auto-detect and verify */
    uft_verify_status_t status;
    
    if (read >= 4) {
        if (memcmp(data, "WOZ1", 4) == 0 || memcmp(data, "WOZ2", 4) == 0) {
            status = uft_verify_woz(data, read, result);
        } else if (memcmp(data, "A2R2", 4) == 0 || memcmp(data, "A2R3", 4) == 0) {
            status = uft_verify_a2r(data, read, result);
        } else if ((data[0] == 'T' && data[1] == 'D') ||
                   (data[0] == 't' && data[1] == 'd')) {
            status = uft_verify_td0(data, read, result);
        } else {
            status = UFT_VERIFY_FORMAT_ERROR;
            if (result) result->status = status;
        }
    } else {
        status = UFT_VERIFY_SIZE_MISMATCH;
        if (result) result->status = status;
    }
    
    free(data);
    return status;
}
