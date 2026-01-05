// SPDX-License-Identifier: MIT
/*
 * hxc_mfm_sectors.c - Complete IBM MFM Sector Decoder
 * 
 * Professional implementation of IBM MFM sector detection and decoding.
 * Supports complete sector extraction with CRC verification.
 * 
 * IBM MFM Sector Format:
 *   - Address Mark:   A1 A1 A1 FE (with special clock bits)
 *   - ID Field:       C H R N CRC CRC
 *   - Gap 2:          22 bytes of 0x4E
 *   - Data Mark:      A1 A1 A1 FB/F8 (FB=normal, F8=deleted)
 *   - Data Field:     N bytes + CRC CRC
 *   - Gap 3:          Variable bytes of 0x4E
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "../include/hxc_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/*============================================================================*
 * CRC CALCULATION (CRC-16-CCITT for IBM MFM)
 *============================================================================*/

/**
 * @brief Calculate CRC-16-CCITT
 * 
 * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * Initial value: 0xFFFF (for IBM MFM with A1 sync)
 * 
 * @param data Data buffer
 * @param len Data length
 * @param init Initial CRC value
 * @return CRC-16 value
 */
static uint16_t calc_crc16_ccitt(const uint8_t *data, size_t len, uint16_t init)
{
    uint16_t crc = init;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Calculate CRC for IBM MFM sector ID field
 * 
 * ID field includes: A1 A1 A1 FE C H R N
 * CRC is initialized with special value for A1 sync marks.
 * 
 * @param id_field ID field (4 bytes: C H R N)
 * @return CRC-16 value
 */
static uint16_t calc_id_crc(const uint8_t id_field[4])
{
    /* Special CRC initialization for A1 A1 A1 FE prefix */
    /* This accounts for the sync marks with missing clock bits */
    uint16_t crc = 0xCDB4;  /* Pre-calculated CRC of A1 A1 A1 FE */
    
    crc = calc_crc16_ccitt(id_field, 4, crc);
    
    return crc;
}

/**
 * @brief Calculate CRC for IBM MFM data field
 * 
 * Data field includes: A1 A1 A1 FB/F8 + data
 * 
 * @param data_mark Data mark (FB or F8)
 * @param data Sector data
 * @param data_len Data length
 * @return CRC-16 value
 */
static uint16_t calc_data_crc(uint8_t data_mark, const uint8_t *data, size_t data_len)
{
    /* Special CRC initialization for A1 A1 A1 */
    uint16_t crc = 0xE295;  /* Pre-calculated CRC of A1 A1 A1 */
    
    /* Add data mark */
    uint8_t mark = data_mark;
    crc = calc_crc16_ccitt(&mark, 1, crc);
    
    /* Add data */
    crc = calc_crc16_ccitt(data, data_len, crc);
    
    return crc;
}

/*============================================================================*
 * IBM MFM SYNC PATTERN DETECTION
 *============================================================================*/

/**
 * @brief Find IBM MFM sync pattern (A1 with missing clock bit)
 * 
 * IBM MFM uses special A1 bytes with intentionally missing clock bits
 * to create a unique sync pattern that cannot occur in normal data.
 * 
 * Normal A1: 0100 0100 0100 0001 (MFM encoded)
 * Sync A1:   0100 0100 0100 0001 (with missing clock between bits 4-5)
 * 
 * We search for the pattern: 0x4489 in MFM stream
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param start_bit Start search position
 * @param marker_type Output marker type (0xFE=address, 0xFB=data, 0xF8=deleted)
 * @return Bit position after sync, or -1 if not found
 */
static int find_ibm_sync(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit,
    uint8_t *marker_type
) {
    /* Search for three A1 sync marks followed by marker byte */
    const uint16_t sync_pattern = 0x4489;  /* A1 with missing clock */
    
    for (size_t bit_pos = start_bit; 
         bit_pos + (4 * 16) <= mfm_bit_count; 
         bit_pos++) {
        
        /* Check for three consecutive sync patterns */
        bool found_sync = true;
        
        for (int i = 0; i < 3; i++) {
            uint16_t word = 0;
            
            /* Read 16 MFM bits */
            for (int b = 0; b < 16; b++) {
                size_t mfm_pos = bit_pos + (i * 16) + b;
                size_t byte_idx = mfm_pos / 8;
                size_t bit_in_byte = 7 - (mfm_pos % 8);
                
                if (byte_idx >= (mfm_bit_count + 7) / 8) {
                    found_sync = false;
                    break;
                }
                
                uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
                word = (word << 1) | bit;
            }
            
            if (word != sync_pattern) {
                found_sync = false;
                break;
            }
        }
        
        if (found_sync) {
            /* Read marker byte (FE, FB, F8) after the three A1s */
            size_t marker_pos = bit_pos + (3 * 16);
            uint8_t marker = 0;
            
            /* Decode MFM to get marker byte (take data bits only) */
            for (int b = 0; b < 8; b++) {
                size_t mfm_pos = marker_pos + (b * 2) + 1;  /* Skip clock bits */
                size_t byte_idx = mfm_pos / 8;
                size_t bit_in_byte = 7 - (mfm_pos % 8);
                
                if (byte_idx < (mfm_bit_count + 7) / 8) {
                    uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
                    marker = (marker << 1) | bit;
                }
            }
            
            /* Verify marker is valid (FE, FB, or F8) */
            if (marker == 0xFE || marker == 0xFB || marker == 0xF8) {
                *marker_type = marker;
                return (int)(marker_pos + 16);  /* Position after marker */
            }
        }
    }
    
    return -1;
}

/*============================================================================*
 * MFM FIELD DECODING
 *============================================================================*/

/**
 * @brief Decode MFM field to bytes
 * 
 * Extracts data bits from MFM stream (every other bit).
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param start_bit Start position (in MFM bits)
 * @param byte_count Number of bytes to decode
 * @param bytes_out Output buffer
 * @return 0 on success
 */
static int decode_mfm_field(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit,
    size_t byte_count,
    uint8_t *bytes_out
) {
    for (size_t i = 0; i < byte_count; i++) {
        uint8_t byte_val = 0;
        
        for (int b = 0; b < 8; b++) {
            /* Position of data bit (skip clock bits) */
            size_t mfm_pos = start_bit + (i * 8 + b) * 2 + 1;
            size_t byte_idx = mfm_pos / 8;
            size_t bit_in_byte = 7 - (mfm_pos % 8);
            
            if (byte_idx >= (mfm_bit_count + 7) / 8) {
                return -1;
            }
            
            uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
            byte_val = (byte_val << 1) | bit;
        }
        
        bytes_out[i] = byte_val;
    }
    
    return 0;
}

/*============================================================================*
 * PUBLIC API - COMPLETE SECTOR DECODER
 *============================================================================*/

/**
 * @brief Decode complete IBM MFM sector
 * 
 * Professional sector decoder with full CRC verification.
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param start_bit Start search position
 * @param sector_out Decoded sector (output)
 * @return Bit position after sector, or -1 on error
 */
int hxc_decode_ibm_sector_complete(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit,
    hxc_sector_t *sector_out
) {
    if (!mfm_bits || !sector_out) {
        return -1;
    }
    
    memset(sector_out, 0, sizeof(*sector_out));
    
    /* Find address mark (A1 A1 A1 FE) */
    uint8_t marker_type;
    int id_start = find_ibm_sync(mfm_bits, mfm_bit_count, start_bit, &marker_type);
    
    if (id_start < 0 || marker_type != 0xFE) {
        return -1;  /* No address mark found */
    }
    
    /* Decode ID field (C H R N CRC CRC) */
    uint8_t id_field[6];
    if (decode_mfm_field(mfm_bits, mfm_bit_count, id_start, 6, id_field) < 0) {
        return -1;
    }
    
    sector_out->cylinder = id_field[0];
    sector_out->head = id_field[1];
    sector_out->sector = id_field[2];
    sector_out->size_code = id_field[3];
    
    /* Calculate sector size from size code */
    sector_out->data_size = 128 << sector_out->size_code;
    
    /* Verify ID CRC */
    uint16_t calc_id_crc_val = calc_id_crc(id_field);
    uint16_t stored_id_crc = ((uint16_t)id_field[4] << 8) | id_field[5];
    
    bool id_crc_ok = (calc_id_crc_val == stored_id_crc);
    
    /* Search for data mark after ID field */
    size_t data_search_start = id_start + (6 * 8 * 2);  /* After ID + CRC */
    
    int data_start = find_ibm_sync(mfm_bits, mfm_bit_count, 
                                   data_search_start, &marker_type);
    
    if (data_start < 0) {
        sector_out->crc_valid = id_crc_ok;
        return -1;  /* No data mark found */
    }
    
    /* Check if deleted data */
    bool deleted = (marker_type == 0xF8);
    
    /* Allocate data buffer */
    sector_out->data = malloc(sector_out->data_size + 2);  /* +2 for CRC */
    if (!sector_out->data) {
        return -1;
    }
    
    /* Decode data field + CRC */
    if (decode_mfm_field(mfm_bits, mfm_bit_count, data_start,
                        sector_out->data_size + 2, sector_out->data) < 0) {
        free(sector_out->data);
        sector_out->data = NULL;
        return -1;
    }
    
    /* Verify data CRC */
    uint16_t calc_data_crc_val = calc_data_crc(marker_type, sector_out->data, 
                                               sector_out->data_size);
    
    uint16_t stored_data_crc = ((uint16_t)sector_out->data[sector_out->data_size] << 8) | 
                               sector_out->data[sector_out->data_size + 1];
    
    bool data_crc_ok = (calc_data_crc_val == stored_data_crc);
    
    sector_out->crc_valid = id_crc_ok && data_crc_ok;
    
    /* Return position after this sector */
    return data_start + ((sector_out->data_size + 2) * 8 * 2);
}

/**
 * @brief Scan entire track for sectors
 * 
 * Scans MFM track and extracts all sectors.
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param disk_out Decoded disk (output)
 * @return 0 on success
 */
int hxc_scan_track_sectors(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    hxc_disk_t *disk_out
) {
    if (!mfm_bits || !disk_out) {
        return -1;
    }
    
    memset(disk_out, 0, sizeof(*disk_out));
    
    /* Allocate max sectors (typically 18-21 for HD, 9 for DD) */
    const uint32_t MAX_SECTORS = 32;
    disk_out->sectors = calloc(MAX_SECTORS, sizeof(*disk_out->sectors));
    if (!disk_out->sectors) {
        return -1;
    }
    
    /* Scan track */
    size_t bit_pos = 0;
    uint32_t sector_idx = 0;
    
    while (bit_pos < mfm_bit_count && sector_idx < MAX_SECTORS) {
        hxc_sector_t sector;
        
        int next_pos = hxc_decode_ibm_sector_complete(mfm_bits, mfm_bit_count,
                                                      bit_pos, &sector);
        
        if (next_pos < 0) {
            /* No more sectors found, try advancing */
            bit_pos += 1000;  /* Skip ahead */
            
            if (bit_pos >= mfm_bit_count) {
                break;
            }
            
            continue;
        }
        
        disk_out->sectors[sector_idx] = sector;
        sector_idx++;
        
        bit_pos = next_pos;
    }
    
    disk_out->sector_count = sector_idx;
    
    /* Detect geometry from sectors */
    if (sector_idx > 0) {
        uint8_t max_cyl = 0, max_head = 0, max_sec = 0;
        
        for (uint32_t i = 0; i < sector_idx; i++) {
            if (disk_out->sectors[i].cylinder > max_cyl) {
                max_cyl = disk_out->sectors[i].cylinder;
            }
            if (disk_out->sectors[i].head > max_head) {
                max_head = disk_out->sectors[i].head;
            }
            if (disk_out->sectors[i].sector > max_sec) {
                max_sec = disk_out->sectors[i].sector;
            }
        }
        
        disk_out->cylinders = max_cyl + 1;
        disk_out->heads = max_head + 1;
        disk_out->sectors_per_track = max_sec;
    }
    
    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
