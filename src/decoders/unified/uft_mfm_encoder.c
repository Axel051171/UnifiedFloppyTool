/**
 * @file uft_mfm_encoder.c
 * @brief MFM Encoding Implementation
 * 
 * AUDIT FIX: TODO in uft_mfm_decoder_v2.c:488
 * 
 * Implements MFM encoding for writing sectors to disk.
 */

#include "uft/decoders/uft_mfm_decoder.h"
#include "uft/uft_sector.h"
#include "uft/core/uft_platform.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define MFM_SYNC_PATTERN    0xA1  // With missing clock
#define MFM_INDEX_PATTERN   0xC2  // With missing clock
#define MFM_GAP_BYTE        0x4E
#define MFM_PRE_SYNC_BYTE   0x00

// IBM format gaps
#define MFM_GAP1_SIZE       50   // After index
#define MFM_GAP2_SIZE       22   // After ID
#define MFM_GAP3_SIZE       54   // After data (DD), 84 (HD)
#define MFM_GAP4A_SIZE      80   // Post-index

// ============================================================================
// CRC-CCITT (IBM)
// ============================================================================

static uint16_t crc_ccitt_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static uint16_t crc_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc_ccitt_update(crc, data[i]);
    }
    return crc;
}

// ============================================================================
// MFM ENCODING
// ============================================================================

/**
 * @brief Encode a byte to MFM
 * 
 * MFM encoding rules:
 * - Data 1 -> 01
 * - Data 0 after 0 -> 10
 * - Data 0 after 1 -> 00
 * 
 * @param byte Data byte
 * @param prev_bit Previous bit (for clock calculation)
 * @return 16-bit MFM encoded value
 */
static uint16_t mfm_encode_byte(uint8_t byte, uint8_t* prev_bit) {
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t data_bit = (byte >> i) & 1;
        uint8_t clock_bit;
        
        if (data_bit) {
            clock_bit = 0;
        } else {
            clock_bit = (*prev_bit == 0) ? 1 : 0;
        }
        
        result = (result << 2) | (clock_bit << 1) | data_bit;
        *prev_bit = data_bit;
    }
    
    return result;
}

/**
 * @brief Encode sync byte with missing clock (A1 -> 4489)
 */
static uint16_t mfm_encode_sync(void) {
    // A1 with missing clock at bit 4 = 0x4489
    return 0x4489;
}

/**
 * @brief Write MFM data to buffer
 * 
 * @param buf Output buffer
 * @param pos Current bit position (updated)
 * @param data 16-bit MFM value
 */
static void write_mfm_word(uint8_t* buf, size_t* pos, uint16_t data) {
    for (int i = 15; i >= 0; i--) {
        size_t byte_idx = *pos / 8;
        size_t bit_idx = 7 - (*pos % 8);
        
        if (data & (1 << i)) {
            buf[byte_idx] |= (1 << bit_idx);
        }
        (*pos)++;
    }
}

/**
 * @brief Encode a complete sector
 * 
 * @param sector Sector to encode
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param pos Current bit position (updated)
 * @return UFT_OK on success
 */
static uft_error_t encode_sector(const uft_sector_t* sector,
                                  uint8_t* buf, size_t buf_size,
                                  size_t* pos, uint8_t* prev_bit) {
    /* FIX: Bounds checking - calculate required space */
    /* Sector needs: 12 sync + 4 sync_marks + 4 id + 2 crc + 22 gap2 + 
     * 12 sync + 4 sync_marks + data + 2 crc + 54 gap3 */
    /* Each byte = 16 MFM bits, so minimum ~1200+ bits per sector */
    size_t required_bits = (12 + 4 + 4 + 2 + 22 + 12 + 4 + 2 + 54) * 16;
    if (sector->data_size > 0) {
        required_bits += sector->data_size * 16;
    } else {
        required_bits += 512 * 16;  /* Default sector size */
    }
    
    if ((*pos + required_bits) / 8 > buf_size) {
        return UFT_ERROR_BUFFER_TOO_SMALL;
    }
    
    /* Helper macro for bounds-checked write */
    #define WRITE_CHECKED(word) do { \
        if ((*pos + 16) / 8 > buf_size) return UFT_ERROR_BUFFER_TOO_SMALL; \
        write_mfm_word(buf, pos, (word)); \
    } while(0)
    
    // Gap 2 (sync field)
    for (int i = 0; i < 12; i++) {
        write_mfm_word(buf, pos, mfm_encode_byte(0x00, prev_bit));
    }
    
    // Address mark (3x A1 sync + FE)
    write_mfm_word(buf, pos, mfm_encode_sync());
    write_mfm_word(buf, pos, mfm_encode_sync());
    write_mfm_word(buf, pos, mfm_encode_sync());
    write_mfm_word(buf, pos, mfm_encode_byte(0xFE, prev_bit));
    
    // ID field
    uint8_t id_field[4];
    id_field[0] = sector->id.cylinder;
    id_field[1] = sector->id.head;
    id_field[2] = sector->id.sector;
    id_field[3] = sector->id.size_code;
    
    for (int i = 0; i < 4; i++) {
        write_mfm_word(buf, pos, mfm_encode_byte(id_field[i], prev_bit));
    }
    
    // ID CRC
    uint16_t id_crc = crc_ccitt(id_field, 4);
    // Include sync marks in CRC
    id_crc = crc_ccitt_update(id_crc, 0xA1);
    id_crc = crc_ccitt_update(id_crc, 0xA1);
    id_crc = crc_ccitt_update(id_crc, 0xA1);
    id_crc = crc_ccitt_update(id_crc, 0xFE);
    
    // Recalculate properly
    uint8_t crc_buf[8] = {0xA1, 0xA1, 0xA1, 0xFE, 
                          id_field[0], id_field[1], id_field[2], id_field[3]};
    id_crc = crc_ccitt(crc_buf, 8);
    
    write_mfm_word(buf, pos, mfm_encode_byte((id_crc >> 8) & 0xFF, prev_bit));
    write_mfm_word(buf, pos, mfm_encode_byte(id_crc & 0xFF, prev_bit));
    
    // Gap 2
    for (int i = 0; i < 22; i++) {
        write_mfm_word(buf, pos, mfm_encode_byte(0x4E, prev_bit));
    }
    
    // Sync field before data
    for (int i = 0; i < 12; i++) {
        write_mfm_word(buf, pos, mfm_encode_byte(0x00, prev_bit));
    }
    
    // Data address mark (3x A1 + FB)
    write_mfm_word(buf, pos, mfm_encode_sync());
    write_mfm_word(buf, pos, mfm_encode_sync());
    write_mfm_word(buf, pos, mfm_encode_sync());
    write_mfm_word(buf, pos, mfm_encode_byte(0xFB, prev_bit));
    
    // Data
    if (!sector->data || sector->data_size == 0) {
        return UFT_ERROR_FORMAT;
    }
    
    for (size_t i = 0; i < sector->data_size; i++) {
        write_mfm_word(buf, pos, mfm_encode_byte(sector->data[i], prev_bit));
    }
    
    // Data CRC
    uint16_t data_crc = 0xFFFF;
    data_crc = crc_ccitt_update(data_crc, 0xA1);
    data_crc = crc_ccitt_update(data_crc, 0xA1);
    data_crc = crc_ccitt_update(data_crc, 0xA1);
    data_crc = crc_ccitt_update(data_crc, 0xFB);
    for (size_t i = 0; i < sector->data_size; i++) {
        data_crc = crc_ccitt_update(data_crc, sector->data[i]);
    }
    
    write_mfm_word(buf, pos, mfm_encode_byte((data_crc >> 8) & 0xFF, prev_bit));
    write_mfm_word(buf, pos, mfm_encode_byte(data_crc & 0xFF, prev_bit));
    
    // Gap 3
    for (int i = 0; i < 54; i++) {
        write_mfm_word(buf, pos, mfm_encode_byte(0x4E, prev_bit));
    }
    
    #undef WRITE_CHECKED
    return UFT_OK;
}

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Encode a complete track to MFM
 * 
 * @param sectors Array of sectors to encode
 * @param sector_count Number of sectors
 * @param buf Output buffer (should be ~12500 bytes for DD)
 * @param buf_size Buffer size in bytes
 * @param out_bits Number of bits written
 * @return UFT_OK on success
 */
uft_error_t uft_mfm_encode_track(const uft_sector_t* sectors,
                                  int sector_count,
                                  uint8_t* buf,
                                  size_t buf_size,
                                  size_t* out_bits) {
    if (!sectors || !buf || !out_bits) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    memset(buf, 0, buf_size);
    size_t pos = 0;
    uint8_t prev_bit = 0;
    
    // Gap 4A (post-index)
    for (int i = 0; i < 80 && pos/8 < buf_size; i++) {
        write_mfm_word(buf, &pos, mfm_encode_byte(0x4E, &prev_bit));
    }
    
    // Sync field
    for (int i = 0; i < 12 && pos/8 < buf_size; i++) {
        write_mfm_word(buf, &pos, mfm_encode_byte(0x00, &prev_bit));
    }
    
    // Index mark (3x C2 + FC)
    write_mfm_word(buf, &pos, 0x5224);  // C2 with missing clock
    write_mfm_word(buf, &pos, 0x5224);
    write_mfm_word(buf, &pos, 0x5224);
    write_mfm_word(buf, &pos, mfm_encode_byte(0xFC, &prev_bit));
    
    // Gap 1
    for (int i = 0; i < 50 && pos/8 < buf_size; i++) {
        write_mfm_word(buf, &pos, mfm_encode_byte(0x4E, &prev_bit));
    }
    
    // Encode all sectors
    for (int s = 0; s < sector_count; s++) {
        uft_error_t err = encode_sector(&sectors[s], buf, buf_size, &pos, &prev_bit);
        if (err != UFT_OK) {
            return err;
        }
    }
    
    // Fill remaining with gap
    while (pos/8 + 2 < buf_size) {
        write_mfm_word(buf, &pos, mfm_encode_byte(0x4E, &prev_bit));
    }
    
    *out_bits = pos;
    return UFT_OK;
}

/**
 * @brief Convert MFM bitstream to flux transitions
 * 
 * @param mfm_bits MFM encoded bits
 * @param bit_count Number of bits
 * @param bit_cell_ns Bit cell duration in nanoseconds
 * @param flux Output flux array
 * @param max_flux Maximum flux values
 * @param out_count Number of flux values written
 * @return UFT_OK on success
 */
uft_error_t uft_mfm_to_flux(const uint8_t* mfm_bits,
                            size_t bit_count,
                            uint32_t bit_cell_ns,
                            uint32_t* flux,
                            size_t max_flux,
                            size_t* out_count) {
    if (!mfm_bits || !flux || !out_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    size_t flux_idx = 0;
    uint32_t time_since_flux = 0;
    
    for (size_t i = 0; i < bit_count && flux_idx < max_flux; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);
        bool bit = (mfm_bits[byte_idx] >> bit_idx) & 1;
        
        time_since_flux += bit_cell_ns;
        
        if (bit) {
            flux[flux_idx++] = time_since_flux;
            time_since_flux = 0;
        }
    }
    
    *out_count = flux_idx;
    return UFT_OK;
}
