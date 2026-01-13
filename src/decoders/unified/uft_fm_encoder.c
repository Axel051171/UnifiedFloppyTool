#include "uft_error.h"
#include "uft_error_compat.h"
#include "uft_track.h"
/**
 * @file uft_fm_encoder.c
 * @brief FM (Frequency Modulation) Encoding Implementation
 * 
 * Part of UnifiedFloppyTool v3.3.7
 * 
 * FM encoding is the predecessor to MFM, used in early floppy formats
 * like IBM 3740 (8" SD), TRS-80, and some CP/M systems.
 * 
 * FM Encoding Rules (simpler than MFM):
 * - Every data bit is preceded by a clock bit
 * - Clock bit is ALWAYS 1
 * - Data 0 -> 10 (clock=1, data=0)
 * - Data 1 -> 11 (clock=1, data=1)
 * 
 * This gives half the density of MFM but is more robust.
 */

#include "uft/decoders/uft_fm_decoder.h"
#include "uft/uft_sector.h"
#include "uft/uft_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define FM_CLOCK_BIT        1       // Clock is always 1 in FM
#define FM_SYNC_PATTERN     0xFC    // Index address mark
#define FM_IDAM_PATTERN     0xFE    // ID address mark
#define FM_DAM_PATTERN      0xFB    // Data address mark
#define FM_DDAM_PATTERN     0xF8    // Deleted data address mark
#define FM_GAP_BYTE         0xFF    // Gap fill byte

// IBM 3740 format (8" SD)
#define FM_GAP1_SIZE        40      // Pre-index gap
#define FM_GAP2_SIZE        11      // Post-ID gap
#define FM_GAP3_SIZE        27      // Post-data gap
#define FM_GAP4A_SIZE       26      // Post-index gap

// Data rates
#define FM_SD_DATA_RATE     125000  // 125 kbps (Single Density)
#define FM_BITCELL_NS       4000    // 4 µs per bit cell

// ============================================================================
// CRC-CCITT (Same as MFM - IBM standard)
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
// FM ENCODING CORE
// ============================================================================

/**
 * @brief Encode a single byte to FM
 * 
 * FM encoding: Each data bit is preceded by clock bit (always 1)
 * Result: 16 bits per byte (8 clock + 8 data interleaved)
 * 
 * Example: 0x5A (01011010)
 *   Bit 7: data=0 -> 10
 *   Bit 6: data=1 -> 11
 *   Bit 5: data=0 -> 10
 *   Bit 4: data=1 -> 11
 *   Bit 3: data=1 -> 11
 *   Bit 2: data=0 -> 10
 *   Bit 1: data=1 -> 11
 *   Bit 0: data=0 -> 10
 *   Result: 10111011 11101110 = 0xBBEE
 * 
 * @param byte Data byte to encode
 * @return 16-bit FM encoded value
 */
static uint16_t fm_encode_byte(uint8_t byte) {
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t data_bit = (byte >> i) & 1;
        // FM: clock is always 1
        result = (result << 2) | (FM_CLOCK_BIT << 1) | data_bit;
    }
    
    return result;
}

/**
 * @brief Encode FM sync/address mark with missing clock
 * 
 * Address marks in FM have a missing clock pulse to make them unique.
 * This allows the controller to synchronize.
 * 
 * @param mark Address mark byte (0xFC, 0xFE, 0xFB, or 0xF8)
 * @return 16-bit FM encoded address mark (with missing clock)
 */
static uint16_t fm_encode_address_mark(uint8_t mark) {
    /*
     * Address marks have a missing clock in specific positions:
     * 
     * 0xFE (IDAM): 11111110 -> with clock: C7=0
     *   Normal would be: 11 11 11 11 11 11 11 10
     *   With missing C7: 01 11 11 11 11 11 11 10 = 0x7FFE
     * 
     * 0xFB (DAM):  11111011 -> with clock: C5=0
     *   Normal would be: 11 11 11 11 10 11 11 11
     *   With missing C5: 11 11 11 11 00 11 11 11 = 0xFF37
     * 
     * 0xF8 (DDAM): 11111000 -> with clock: Similar pattern
     * 0xFC (IAM):  11111100 -> Index address mark
     */
    
    switch (mark) {
        case FM_IDAM_PATTERN:    // 0xFE - ID Address Mark
            return 0xF57E;       // Missing clock at bit 5
            
        case FM_DAM_PATTERN:     // 0xFB - Data Address Mark
            return 0xF56F;       // Missing clock at bit 5
            
        case FM_DDAM_PATTERN:    // 0xF8 - Deleted Data Address Mark
            return 0xF56A;       // Missing clock at bit 5
            
        case FM_SYNC_PATTERN:    // 0xFC - Index Address Mark
            return 0xF77A;       // Missing clock at bit 4
            
        default:
            // Fallback: encode normally (shouldn't happen)
            return fm_encode_byte(mark);
    }
}

// ============================================================================
// BUFFER WRITING HELPERS
// ============================================================================

/**
 * @brief Write FM-encoded byte to buffer
 * 
 * @param buf Output buffer
 * @param pos Current position (will be updated)
 * @param max_len Maximum buffer length
 * @param byte Byte to encode
 * @return true on success
 */
static bool fm_write_byte(uint8_t* buf, size_t* pos, size_t max_len, uint8_t byte) {
    if (*pos + 2 > max_len) return false;
    
    uint16_t encoded = fm_encode_byte(byte);
    buf[(*pos)++] = (uint8_t)(encoded >> 8);
    buf[(*pos)++] = (uint8_t)(encoded & 0xFF);
    return true;
}

/**
 * @brief Write FM-encoded address mark to buffer
 */
static bool fm_write_address_mark(uint8_t* buf, size_t* pos, size_t max_len, uint8_t mark) {
    if (*pos + 2 > max_len) return false;
    
    uint16_t encoded = fm_encode_address_mark(mark);
    buf[(*pos)++] = (uint8_t)(encoded >> 8);
    buf[(*pos)++] = (uint8_t)(encoded & 0xFF);
    return true;
}

/**
 * @brief Write gap bytes
 */
static bool fm_write_gap(uint8_t* buf, size_t* pos, size_t max_len, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!fm_write_byte(buf, pos, max_len, FM_GAP_BYTE)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Write sync bytes (0x00)
 */
static bool fm_write_sync(uint8_t* buf, size_t* pos, size_t max_len, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!fm_write_byte(buf, pos, max_len, 0x00)) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Encode a sector ID field in FM format
 * 
 * ID field format:
 *   Sync bytes (6x 0x00)
 *   IDAM (0xFE with missing clock)
 *   Cylinder
 *   Head
 *   Sector
 *   Size code
 *   CRC (2 bytes)
 * 
 * @param cyl Cylinder number
 * @param head Head number  
 * @param sector Sector number
 * @param size_code Size code (0=128, 1=256, 2=512, 3=1024)
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_sector_id(
    uint8_t cyl,
    uint8_t head,
    uint8_t sector,
    uint8_t size_code,
    uint8_t* output,
    size_t output_len
) {
    if (!output || output_len < 24) return 0;  // Need at least 12 bytes * 2
    
    size_t pos = 0;
    
    // Write sync bytes
    if (!fm_write_sync(output, &pos, output_len, 6)) return 0;
    
    // Write IDAM with missing clock
    if (!fm_write_address_mark(output, &pos, output_len, FM_IDAM_PATTERN)) return 0;
    
    // Calculate CRC over ID field
    uint8_t id_data[5] = { FM_IDAM_PATTERN, cyl, head, sector, size_code };
    uint16_t crc = crc_ccitt(id_data, 5);
    
    // Write ID bytes
    if (!fm_write_byte(output, &pos, output_len, cyl)) return 0;
    if (!fm_write_byte(output, &pos, output_len, head)) return 0;
    if (!fm_write_byte(output, &pos, output_len, sector)) return 0;
    if (!fm_write_byte(output, &pos, output_len, size_code)) return 0;
    
    // Write CRC
    if (!fm_write_byte(output, &pos, output_len, (uint8_t)(crc >> 8))) return 0;
    if (!fm_write_byte(output, &pos, output_len, (uint8_t)(crc & 0xFF))) return 0;
    
    return pos;
}

/**
 * @brief Encode sector data field in FM format
 * 
 * Data field format:
 *   Sync bytes (6x 0x00)
 *   DAM (0xFB with missing clock) or DDAM (0xF8)
 *   Data bytes
 *   CRC (2 bytes)
 * 
 * @param data Sector data
 * @param data_len Data length (128, 256, 512, or 1024)
 * @param deleted true for deleted data mark
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_sector_data(
    const uint8_t* data,
    size_t data_len,
    bool deleted,
    uint8_t* output,
    size_t output_len
) {
    if (!data || !output) return 0;
    
    // Need: sync(6)*2 + dam(1)*2 + data*2 + crc(2)*2 = 12 + 2 + data*2 + 4
    size_t needed = 18 + data_len * 2;
    if (output_len < needed) return 0;
    
    size_t pos = 0;
    
    // Write sync bytes
    if (!fm_write_sync(output, &pos, output_len, 6)) return 0;
    
    // Write DAM/DDAM with missing clock
    uint8_t dam = deleted ? FM_DDAM_PATTERN : FM_DAM_PATTERN;
    if (!fm_write_address_mark(output, &pos, output_len, dam)) return 0;
    
    // Calculate CRC over DAM + data
    uint16_t crc = 0xFFFF;
    crc = crc_ccitt_update(crc, dam);
    for (size_t i = 0; i < data_len; i++) {
        crc = crc_ccitt_update(crc, data[i]);
    }
    
    // Write data
    for (size_t i = 0; i < data_len; i++) {
        if (!fm_write_byte(output, &pos, output_len, data[i])) return 0;
    }
    
    // Write CRC
    if (!fm_write_byte(output, &pos, output_len, (uint8_t)(crc >> 8))) return 0;
    if (!fm_write_byte(output, &pos, output_len, (uint8_t)(crc & 0xFF))) return 0;
    
    return pos;
}

/**
 * @brief Encode a complete FM sector (ID + gap + data)
 * 
 * @param sector Sector structure with data
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_sector(
    const uft_sector_t* sector,
    uint8_t* output,
    size_t output_len
) {
    if (!sector || !output) return 0;
    
    size_t pos = 0;
    size_t written;
    
    // Encode ID field
    written = uft_fm_encode_sector_id(
        sector->cylinder,
        sector->head,
        sector->sector_num,
        sector->size_code,
        output + pos,
        output_len - pos
    );
    if (written == 0) return 0;
    pos += written;
    
    // Write gap 2
    if (!fm_write_gap(output, &pos, output_len, FM_GAP2_SIZE)) return 0;
    
    // Encode data field
    bool deleted = (sector->flags & UFT_SECTOR_FLAG_DELETED) != 0;
    written = uft_fm_encode_sector_data(
        sector->data,
        sector->data_size,
        deleted,
        output + pos,
        output_len - pos
    );
    if (written == 0) return 0;
    pos += written;
    
    // Write gap 3
    if (!fm_write_gap(output, &pos, output_len, FM_GAP3_SIZE)) return 0;
    
    return pos;
}

/**
 * @brief Encode a complete FM track
 * 
 * @param sectors Array of sectors
 * @param sector_count Number of sectors
 * @param output Output buffer
 * @param output_len Output buffer length
 * @return Number of bytes written, or 0 on error
 */
size_t uft_fm_encode_track(
    const uft_sector_t* sectors,
    size_t sector_count,
    uint8_t* output,
    size_t output_len
) {
    if (!sectors || sector_count == 0 || !output) return 0;
    
    size_t pos = 0;
    
    // Write gap 4A (post-index)
    if (!fm_write_gap(output, &pos, output_len, FM_GAP4A_SIZE)) return 0;
    
    // Write index address mark
    if (!fm_write_sync(output, &pos, output_len, 6)) return 0;
    if (!fm_write_address_mark(output, &pos, output_len, FM_SYNC_PATTERN)) return 0;
    
    // Write gap 1
    if (!fm_write_gap(output, &pos, output_len, FM_GAP1_SIZE)) return 0;
    
    // Write all sectors
    for (size_t i = 0; i < sector_count; i++) {
        size_t written = uft_fm_encode_sector(&sectors[i], output + pos, output_len - pos);
        if (written == 0) return 0;
        pos += written;
    }
    
    // Fill remainder with gap 4 pattern
    while (pos + 2 <= output_len) {
        if (!fm_write_byte(output, &pos, output_len, FM_GAP_BYTE)) break;
    }
    
    return pos;
}

/**
 * @brief Get FM encoded track length for given parameters
 * 
 * @param sector_count Number of sectors
 * @param sector_size Sector data size in bytes
 * @return Required buffer size in bytes (FM encoded, so 2x raw size)
 */
size_t uft_fm_track_size(size_t sector_count, size_t sector_size) {
    // Gap4A + IAM + Gap1 + sectors*(ID + Gap2 + Data + Gap3) + Gap4
    // All values are doubled for FM encoding
    
    size_t per_sector = 
        6 * 2 +              // Sync
        1 * 2 +              // IDAM
        4 * 2 +              // ID bytes
        2 * 2 +              // CRC
        FM_GAP2_SIZE * 2 +   // Gap 2
        6 * 2 +              // Sync
        1 * 2 +              // DAM
        sector_size * 2 +    // Data
        2 * 2 +              // CRC
        FM_GAP3_SIZE * 2;    // Gap 3
    
    return 
        FM_GAP4A_SIZE * 2 +  // Gap 4A
        6 * 2 +              // Sync
        1 * 2 +              // IAM
        FM_GAP1_SIZE * 2 +   // Gap 1
        sector_count * per_sector +
        256 * 2;             // Gap 4 (fill to end)
}
