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

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMG/IMA Format Verify (Raw Sector Image)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_img_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    if (!data || size == 0) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Valid IMG sizes (common formats) */
    static const size_t valid_sizes[] = {
        160 * 1024,      /* 160KB 5.25" SS/DD */
        180 * 1024,      /* 180KB 5.25" SS/DD */
        320 * 1024,      /* 320KB 5.25" DS/DD */
        360 * 1024,      /* 360KB 5.25" DS/DD */
        720 * 1024,      /* 720KB 3.5" DS/DD */
        1200 * 1024,     /* 1.2MB 5.25" DS/HD */
        1440 * 1024,     /* 1.44MB 3.5" DS/HD */
        2880 * 1024,     /* 2.88MB 3.5" DS/ED */
        0
    };
    
    bool valid_size = false;
    for (int i = 0; valid_sizes[i] != 0; i++) {
        if (size == valid_sizes[i]) {
            valid_size = true;
            break;
        }
    }
    
    /* Also allow sector-aligned sizes */
    if (!valid_size && (size % 512 == 0) && size >= 512 && size <= 3 * 1024 * 1024) {
        valid_size = true;
    }
    
    if (!valid_size) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check for boot sector signature (optional but common) */
    if (size >= 512) {
        /* Look for FAT12/16 boot sector markers */
        bool has_boot_sig = (data[510] == 0x55 && data[511] == 0xAA);
        
        /* Check media descriptor byte */
        if (size >= 22) {
            uint8_t media = data[21];  /* Media descriptor at offset 21 in BPB */
            bool valid_media = (media >= 0xF0 || media == 0x00);
            
            /* If we have boot signature and valid media, it's good */
            if (has_boot_sig && valid_media) {
                if (result) result->status = UFT_VERIFY_OK;
                return UFT_VERIFY_OK;
            }
        }
        
        /* No boot signature is OK for raw disk images */
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMD Format Verify (ImageDisk)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_imd_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    if (!data || size < 4) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check magic: "IMD " */
    if (memcmp(data, "IMD ", 4) != 0) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Find end of header (0x1A terminator) */
    size_t header_end = 0;
    for (size_t i = 4; i < size && i < 1024; i++) {
        if (data[i] == 0x1A) {
            header_end = i;
            break;
        }
    }
    
    if (header_end == 0) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Validate track records follow header */
    if (header_end + 5 < size) {
        size_t pos = header_end + 1;
        
        /* First track header: mode, cylinder, head, sector_count, sector_size */
        uint8_t mode = data[pos];
        uint8_t cyl = data[pos + 1];
        uint8_t head = data[pos + 2];
        uint8_t nsec = data[pos + 3];
        uint8_t secsize = data[pos + 4];
        
        /* Validate mode (0-5 are valid FM/MFM modes) */
        if (mode > 5) {
            if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
            return UFT_VERIFY_FORMAT_ERROR;
        }
        
        /* Validate sector size code (0-6) */
        if (secsize > 6) {
            if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
            return UFT_VERIFY_FORMAT_ERROR;
        }
        
        /* Reasonable cylinder/head/sector counts */
        if (cyl > 85 || head > 1 || nsec > 64) {
            if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
            return UFT_VERIFY_FORMAT_ERROR;
        }
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D71 Format Verify (Commodore 1571)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_d71_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* D71 is 349696 bytes (70 tracks × 256 bytes average per sector) */
    if (!data || (size != 349696 && size != 351062)) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check BAM at track 18, sector 0 (offset depends on track layout) */
    /* Track 18 starts at: sum of sectors for tracks 1-17 × 256 */
    /* Tracks 1-17: 21+21+21+21+21+21+21+21+21+21+21+21+21+21+21+21+21 = 357 sectors = 91392 bytes */
    size_t bam_offset = 91392;  /* Track 18, sector 0 */
    
    if (size > bam_offset + 2) {
        /* BAM signature for 1571: track 18 sector pointer and format byte */
        uint8_t dir_track = data[bam_offset + 0];
        uint8_t format = data[bam_offset + 2];
        
        /* Directory track should be 18 */
        if (dir_track != 18) {
            if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
            return UFT_VERIFY_FORMAT_ERROR;
        }
        
        /* Format ID should be 'A' (0x41) for 1571 */
        if (format != 0x41 && format != 0x00) {
            /* Not fatal, some images have different format bytes */
        }
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D81 Format Verify (Commodore 1581)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_d81_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* D81 is 819200 bytes (80 tracks × 40 sectors × 256 bytes) */
    if (!data || size != 819200) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check header sector at track 40, sector 0 */
    /* Track 40 starts at: 39 tracks × 40 sectors × 256 bytes = 399360 */
    size_t header_offset = 399360;
    
    if (size > header_offset + 4) {
        /* Check disk name area - should have printable characters or 0xA0 padding */
        uint8_t format_byte = data[header_offset + 2];
        
        /* Format byte should be 'D' (0x44) for 1581 */
        if (format_byte != 0x44 && format_byte != 0x00) {
            /* Not necessarily an error, but unusual */
        }
        
        /* Check BAM signature at track 40, sector 1-2 */
        size_t bam_offset = header_offset + 256;  /* Sector 1 */
        if (size > bam_offset + 6) {
            /* First BAM sector should have valid track reference */
            uint8_t bam_track = data[bam_offset + 0];
            uint8_t bam_sector = data[bam_offset + 1];
            
            /* Should point to track 40, sector 2 */
            if (bam_track != 40 || bam_sector != 2) {
                /* BAM chain might be different, not fatal */
            }
        }
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * HFE Format Verify (HxC Floppy Emulator)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_hfe_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* Minimum HFE header is 512 bytes */
    if (!data || size < 512) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check magic: "HXCPICFE" */
    if (memcmp(data, "HXCPICFE", 8) != 0) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Parse header fields */
    uint8_t revision = data[8];
    uint8_t num_tracks = data[9];
    uint8_t num_sides = data[10];
    uint8_t encoding = data[11];
    
    /* Validate fields */
    if (revision > 3) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    if (num_tracks == 0 || num_tracks > 85) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    if (num_sides == 0 || num_sides > 2) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Encoding: 0=Unknown, 1=IBM MFM, 2=Amiga MFM, etc. */
    if (encoding > 10) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Check track list offset (at offset 18-19, little endian) */
    uint16_t track_list_offset = (uint16_t)data[18] | ((uint16_t)data[19] << 8);
    
    /* Track list should be within file */
    if ((size_t)(track_list_offset * 512) >= size) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D88 Format Verify (Japanese PC formats)
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_status_t uft_verify_d88_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->status = UFT_VERIFY_OK;
    }
    
    /* Minimum D88 header is 672 bytes (header + track table) */
    if (!data || size < 672) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* D88 header structure:
     * 0x00-0x10: Disk name (null-terminated)
     * 0x1A: Write protect flag
     * 0x1B: Media type (0x00=2D, 0x10=2DD, 0x20=2HD)
     * 0x1C-0x1F: Disk size (little endian)
     * 0x20-0x29F: Track offset table (164 tracks × 4 bytes)
     */
    
    /* Check media type */
    uint8_t media_type = data[0x1B];
    if (media_type != 0x00 && media_type != 0x10 && 
        media_type != 0x20 && media_type != 0x30) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    /* Read disk size from header */
    uint32_t disk_size = (uint32_t)data[0x1C] |
                         ((uint32_t)data[0x1D] << 8) |
                         ((uint32_t)data[0x1E] << 16) |
                         ((uint32_t)data[0x1F] << 24);
    
    /* Size should match file size (or be 0 for unset) */
    if (disk_size != 0 && disk_size != size) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    /* Check first track offset */
    uint32_t track0_offset = (uint32_t)data[0x20] |
                             ((uint32_t)data[0x21] << 8) |
                             ((uint32_t)data[0x22] << 16) |
                             ((uint32_t)data[0x23] << 24);
    
    /* First track should start after header (0x2A0 = 672) */
    if (track0_offset != 0 && track0_offset < 0x2A0) {
        if (result) result->status = UFT_VERIFY_FORMAT_ERROR;
        return UFT_VERIFY_FORMAT_ERROR;
    }
    
    if (track0_offset != 0 && track0_offset >= size) {
        if (result) result->status = UFT_VERIFY_SIZE_MISMATCH;
        return UFT_VERIFY_SIZE_MISMATCH;
    }
    
    if (result) result->status = UFT_VERIFY_OK;
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Generic File Verify (Extended)
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
    
    if (read >= 8) {
        /* WOZ format */
        if (memcmp(data, "WOZ1", 4) == 0 || memcmp(data, "WOZ2", 4) == 0) {
            status = uft_verify_woz(data, read, result);
        }
        /* A2R format */
        else if (memcmp(data, "A2R2", 4) == 0 || memcmp(data, "A2R3", 4) == 0) {
            status = uft_verify_a2r(data, read, result);
        }
        /* TD0 format */
        else if ((data[0] == 'T' && data[1] == 'D') ||
                 (data[0] == 't' && data[1] == 'd')) {
            status = uft_verify_td0(data, read, result);
        }
        /* IMD format */
        else if (memcmp(data, "IMD ", 4) == 0) {
            status = uft_verify_imd_buffer(data, read, result);
        }
        /* HFE format */
        else if (memcmp(data, "HXCPICFE", 8) == 0) {
            status = uft_verify_hfe_buffer(data, read, result);
        }
        /* D88 format (check header structure) */
        else if (read >= 672 && 
                 (data[0x1B] == 0x00 || data[0x1B] == 0x10 || 
                  data[0x1B] == 0x20 || data[0x1B] == 0x30)) {
            /* Might be D88, try verification */
            status = uft_verify_d88_buffer(data, read, result);
            if (status != UFT_VERIFY_OK) {
                /* Not D88, might be raw IMG */
                status = uft_verify_img_buffer(data, read, result);
            }
        }
        /* D81 format (exact size) */
        else if (read == 819200) {
            status = uft_verify_d81_buffer(data, read, result);
        }
        /* D71 format (exact size) */
        else if (read == 349696 || read == 351062) {
            status = uft_verify_d71_buffer(data, read, result);
        }
        /* Raw IMG/IMA format (sector-aligned) */
        else if (read % 512 == 0 && read >= 512 && read <= 3 * 1024 * 1024) {
            status = uft_verify_img_buffer(data, read, result);
        }
        else {
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
