// SPDX-License-Identifier: MIT
/*
 * hxc_mfm.c - MFM Decoder
 * 
 * Professional MFM (Modified Frequency Modulation) decoder.
 * MFM is the most common encoding for floppy disks.
 * 
 * MFM Encoding:
 *   - Clock bit inserted between data bits if no flux transition
 *   - Data: 1 = flux transition, 0 = no transition
 *   - Clock: Inserted to limit run length
 * 
 * Algorithm:
 *   - Read MFM-encoded bitstream
 *   - Extract clock and data bits
 *   - Decode to raw bytes
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "../include/hxc_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================*
 * MFM DECODING
 *============================================================================*/

/**
 * @brief Decode MFM bitstream to bytes
 * 
 * MFM encoding:
 *   - Every data bit has a clock bit before it
 *   - Clock bit = 1 if previous data bit = 0 AND current data bit = 0
 *   - Data bit = original data
 * 
 * To decode:
 *   - Take every other bit (the data bits)
 *   - Ignore clock bits
 * 
 * @param mfm_bits MFM-encoded bits
 * @param mfm_bit_count Number of MFM bits
 * @param bytes_out Decoded bytes (allocated by this function)
 * @param byte_count_out Number of decoded bytes
 * @return 0 on success
 */
int hxc_decode_mfm(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    uint8_t **bytes_out,
    size_t *byte_count_out
) {
    if (!mfm_bits || !bytes_out || !byte_count_out) {
        return -1;
    }
    
    /* MFM has 2 bits per data bit (clock + data) */
    size_t data_bit_count = mfm_bit_count / 2;
    size_t byte_count = data_bit_count / 8;
    
    if (byte_count == 0) {
        *bytes_out = NULL;
        *byte_count_out = 0;
        return 0;
    }
    
    uint8_t *bytes = calloc(byte_count, 1);
    if (!bytes) {
        return -1;
    }
    
    /* Extract data bits (every other bit) */
    for (size_t bit_idx = 0; bit_idx < data_bit_count; bit_idx++) {
        /* MFM bit position: clock bit + data bit */
        size_t mfm_bit_pos = bit_idx * 2 + 1;  /* +1 to skip clock bit */
        
        /* Get bit from MFM stream */
        size_t byte_idx = mfm_bit_pos / 8;
        size_t bit_in_byte = 7 - (mfm_bit_pos % 8);  /* MSB first */
        
        if (byte_idx >= (mfm_bit_count + 7) / 8) {
            break;
        }
        
        uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
        
        /* Store in output byte */
        size_t out_byte_idx = bit_idx / 8;
        size_t out_bit_in_byte = 7 - (bit_idx % 8);
        
        if (bit) {
            bytes[out_byte_idx] |= (1 << out_bit_in_byte);
        }
    }
    
    *bytes_out = bytes;
    *byte_count_out = byte_count;
    
    return 0;
}

/**
 * @brief Encode bytes to MFM bitstream
 * 
 * Useful for writing disks.
 * 
 * @param bytes Input bytes
 * @param byte_count Number of bytes
 * @param mfm_bits_out MFM bitstream (allocated by this function)
 * @param mfm_bit_count_out Number of MFM bits
 * @return 0 on success
 */
int hxc_encode_mfm(
    const uint8_t *bytes,
    size_t byte_count,
    uint8_t **mfm_bits_out,
    size_t *mfm_bit_count_out
) {
    if (!bytes || !mfm_bits_out || !mfm_bit_count_out) {
        return -1;
    }
    
    /* MFM needs 2 bits per data bit */
    size_t data_bit_count = byte_count * 8;
    size_t mfm_bit_count = data_bit_count * 2;
    size_t mfm_byte_count = (mfm_bit_count + 7) / 8;
    
    uint8_t *mfm_bits = calloc(mfm_byte_count, 1);
    if (!mfm_bits) {
        return -1;
    }
    
    uint8_t prev_bit = 0;
    
    for (size_t bit_idx = 0; bit_idx < data_bit_count; bit_idx++) {
        /* Get data bit */
        size_t byte_idx = bit_idx / 8;
        size_t bit_in_byte = 7 - (bit_idx % 8);
        uint8_t data_bit = (bytes[byte_idx] >> bit_in_byte) & 1;
        
        /* Calculate clock bit */
        uint8_t clock_bit = (prev_bit == 0 && data_bit == 0) ? 1 : 0;
        
        /* Write clock bit */
        size_t mfm_clock_pos = bit_idx * 2;
        size_t clock_byte = mfm_clock_pos / 8;
        size_t clock_bit_in_byte = 7 - (mfm_clock_pos % 8);
        
        if (clock_bit) {
            mfm_bits[clock_byte] |= (1 << clock_bit_in_byte);
        }
        
        /* Write data bit */
        size_t mfm_data_pos = bit_idx * 2 + 1;
        size_t data_byte = mfm_data_pos / 8;
        size_t data_bit_in_byte = 7 - (mfm_data_pos % 8);
        
        if (data_bit) {
            mfm_bits[data_byte] |= (1 << data_bit_in_byte);
        }
        
        prev_bit = data_bit;
    }
    
    *mfm_bits_out = mfm_bits;
    *mfm_bit_count_out = mfm_bit_count;
    
    return 0;
}

/*============================================================================*
 * IBM MFM SECTOR DECODING
 *============================================================================*/

/**
 * @brief Find IBM MFM sector marker
 * 
 * IBM MFM sectors start with:
 *   - Address Mark: A1 A1 A1 (with missing clock bits)
 *   - Followed by: FE (address) or FB (data)
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Bit count
 * @param start_bit Start position
 * @return Bit position of marker, or -1 if not found
 */
int hxc_find_ibm_sector_marker(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit
) {
    /* IBM Address Mark: A1 A1 A1 with special encoding */
    /* This is a simplified search - full impl needs clock bit check */
    
    for (size_t bit_pos = start_bit; 
         bit_pos + 32 * 8 <= mfm_bit_count; 
         bit_pos++) {
        
        /* Decode a few bytes and check for A1 pattern */
        uint8_t bytes[4];
        
        for (int i = 0; i < 4; i++) {
            uint8_t byte_val = 0;
            
            for (int b = 0; b < 8; b++) {
                size_t mfm_pos = bit_pos + (i * 8 + b) * 2 + 1;  /* Data bits only */
                size_t byte_idx = mfm_pos / 8;
                size_t bit_in_byte = 7 - (mfm_pos % 8);
                
                if (byte_idx < (mfm_bit_count + 7) / 8) {
                    uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
                    byte_val = (byte_val << 1) | bit;
                }
            }
            
            bytes[i] = byte_val;
        }
        
        /* Check for A1 A1 A1 FE/FB pattern */
        if (bytes[0] == 0xA1 && bytes[1] == 0xA1 && bytes[2] == 0xA1) {
            if (bytes[3] == 0xFE || bytes[3] == 0xFB) {
                return (int)bit_pos;
            }
        }
    }
    
    return -1;
}

/**
 * @brief Decode IBM MFM sector
 * 
 * IBM sector format:
 *   - Address Mark: A1 A1 A1 FE
 *   - ID Field: C H R N CRC
 *   - Data Mark: A1 A1 A1 FB
 *   - Data Field: 512 bytes + CRC
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Bit count
 * @param marker_pos Position of sector marker
 * @param sector_out Decoded sector (output)
 * @return 0 on success
 */
int hxc_decode_ibm_sector(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t marker_pos,
    hxc_sector_t *sector_out
) {
    if (!mfm_bits || !sector_out) {
        return -1;
    }
    
    memset(sector_out, 0, sizeof(*sector_out));
    
    /* Decode ID field (after A1 A1 A1 FE) */
    /* Format: C H R N CRC CRC */
    /* C = Cylinder, H = Head, R = Sector, N = Size code */
    
    size_t id_start = marker_pos + (4 * 8 * 2);  /* Skip A1 A1 A1 FE */
    
    uint8_t id_bytes[6];
    for (int i = 0; i < 6; i++) {
        uint8_t byte_val = 0;
        
        for (int b = 0; b < 8; b++) {
            size_t mfm_pos = id_start + (i * 8 + b) * 2 + 1;
            size_t byte_idx = mfm_pos / 8;
            size_t bit_in_byte = 7 - (mfm_pos % 8);
            
            if (byte_idx >= (mfm_bit_count + 7) / 8) {
                return -1;
            }
            
            uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
            byte_val = (byte_val << 1) | bit;
        }
        
        id_bytes[i] = byte_val;
    }
    
    sector_out->cylinder = id_bytes[0];
    sector_out->head = id_bytes[1];
    sector_out->sector = id_bytes[2];
    sector_out->size_code = id_bytes[3];
    
    /* Calculate sector size from size code */
    /* N=0: 128, N=1: 256, N=2: 512, N=3: 1024 */
    sector_out->data_size = 128 << id_bytes[3];
    
    /* For full implementation: find data mark and decode sector data */
    /* This is simplified for now */
    
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
