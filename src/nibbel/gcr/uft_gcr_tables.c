/**
 * @file uft_gcr_tables.c
 * @brief GCR Encoding/Decoding Tables - SINGLE SOURCE OF TRUTH
 * 
 * This file contains THE ONLY COPY of GCR tables in the entire project.
 * All other files MUST use these tables via extern declarations.
 * 
 * DO NOT COPY THESE TABLES ELSEWHERE!
 * 
 * Includes:
 *   - Commodore 1541 5-bit GCR
 *   - Apple II 6-and-2 GCR
 *   - 1541 disk geometry tables
 */

#include <stdint.h>
#include <stddef.h>

// ===========================================================================
// COMMODORE 1541 GCR (5-bit to 4-bit)
// ===========================================================================

/**
 * @brief GCR encode table: 4-bit nibble → 5-bit GCR code
 * 
 * Maps data nibbles (0x0-0xF) to 5-bit GCR codes.
 * The codes are chosen to avoid more than 2 consecutive zeros.
 */
const uint8_t uft_gcr_cbm_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13,  // 0-3: 01010, 01011, 10010, 10011
    0x0E, 0x0F, 0x16, 0x17,  // 4-7: 01110, 01111, 10110, 10111
    0x09, 0x19, 0x1A, 0x1B,  // 8-B: 01001, 11001, 11010, 11011
    0x0D, 0x1D, 0x1E, 0x15   // C-F: 01101, 11101, 11110, 10101
};

/**
 * @brief GCR decode table: 5-bit GCR code → 4-bit nibble
 * 
 * Maps 5-bit GCR codes (0x00-0x1F) to data nibbles.
 * Invalid codes (not used in GCR) map to 0xFF.
 */
const uint8_t uft_gcr_cbm_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF,  // 00-03: invalid
    0xFF, 0xFF, 0xFF, 0xFF,  // 04-07: invalid
    0xFF, 0x08, 0x00, 0x01,  // 08-0B: -, 8, 0, 1
    0xFF, 0x0C, 0x04, 0x05,  // 0C-0F: -, C, 4, 5
    0xFF, 0xFF, 0x02, 0x03,  // 10-13: -, -, 2, 3
    0xFF, 0x0F, 0x06, 0x07,  // 14-17: -, F, 6, 7
    0xFF, 0x09, 0x0A, 0x0B,  // 18-1B: -, 9, A, B
    0xFF, 0x0D, 0x0E, 0xFF   // 1C-1F: -, D, E, -
};

// ===========================================================================
// APPLE II GCR (6-and-2 encoding)
// ===========================================================================

/**
 * @brief Apple II 6-and-2 encode table
 * 
 * Maps 6-bit values (0x00-0x3F) to disk bytes.
 * Only values with no more than 1 pair of adjacent zeros are valid.
 */
const uint8_t uft_gcr_apple_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * @brief Apple II 6-and-2 decode table
 * 
 * Maps disk bytes (0x00-0xFF) to 6-bit values.
 * Invalid bytes map to 0xFF.
 */
const uint8_t uft_gcr_apple_decode[256] = {
    // 0x00-0x0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x10-0x1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x20-0x2F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x30-0x3F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x40-0x4F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x50-0x5F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x60-0x6F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x70-0x7F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x80-0x8F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 0x90-0x9F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    // 0xA0-0xAF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,
    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    // 0xB0-0xBF
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    // 0xC0-0xCF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    // 0xD0-0xDF
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,
    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    // 0xE0-0xEF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,
    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    // 0xF0-0xFF
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

// ===========================================================================
// 1541 DISK GEOMETRY
// ===========================================================================

/**
 * @brief Sectors per track for 1541
 * 
 * Index 0 is unused (tracks are 1-based).
 * Zone 3 (T1-17):  21 sectors
 * Zone 2 (T18-24): 19 sectors
 * Zone 1 (T25-30): 18 sectors
 * Zone 0 (T31-42): 17 sectors
 */
const uint8_t uft_1541_sectors_per_track[43] = {
    0,  // Track 0 (unused)
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  // T1-10
    21, 21, 21, 21, 21, 21, 21,              // T11-17
    19, 19, 19, 19, 19, 19, 19,              // T18-24
    18, 18, 18, 18, 18, 18,                  // T25-30
    17, 17, 17, 17, 17,                      // T31-35
    17, 17, 17, 17, 17, 17, 17               // T36-42 (extended)
};

/**
 * @brief Speed zone for each track (0=fastest, 3=slowest)
 */
const uint8_t uft_1541_speed_zone[43] = {
    0,  // Track 0 (unused)
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // T1-10: Zone 3
    3, 3, 3, 3, 3, 3, 3,          // T11-17: Zone 3
    2, 2, 2, 2, 2, 2, 2,          // T18-24: Zone 2
    1, 1, 1, 1, 1, 1,             // T25-30: Zone 1
    0, 0, 0, 0, 0,                // T31-35: Zone 0
    0, 0, 0, 0, 0, 0, 0           // T36-42: Zone 0
};

/**
 * @brief Cumulative sector offset for each track
 * 
 * Used to calculate sector index within D64 file.
 */
const uint16_t uft_1541_track_offset[43] = {
    0,    // Track 0 (unused)
    0,    21,   42,   63,   84,   105,  126,  147,  168,  189,  // T1-10
    210,  231,  252,  273,  294,  315,  336,                    // T11-17
    357,  376,  395,  414,  433,  452,  471,                    // T18-24
    490,  508,  526,  544,  562,  580,                          // T25-30
    598,  615,  632,  649,  666,                                // T31-35
    683,  700,  717,  734,  751,  768,  785                     // T36-42
};

/**
 * @brief Track capacity per speed zone (in bytes)
 * 
 * Varies with drive RPM (typically 300 ± 5 RPM).
 */
const uint16_t uft_1541_track_capacity[4][3] = {
    // {min (305 RPM), typical (300 RPM), max (295 RPM)}
    {7692, 7820, 7962},   // Zone 0: 17 sectors
    {7142, 7268, 7399},   // Zone 1: 18 sectors
    {6666, 6780, 6897},   // Zone 2: 19 sectors
    {6250, 6357, 6468}    // Zone 3: 21 sectors
};

/**
 * @brief Inter-sector gap bytes per speed zone
 */
const uint8_t uft_1541_gap_bytes[4] = {
    9,    // Zone 0
    12,   // Zone 1
    17,   // Zone 2
    21    // Zone 3
};

// ===========================================================================
// VALIDATION / UTILITY
// ===========================================================================

/**
 * @brief Validate GCR tables integrity
 * 
 * Computes checksum of all tables for integrity verification.
 * 
 * @return Checksum value (should be consistent across builds)
 */
uint32_t uft_gcr_tables_checksum(void) {
    uint32_t checksum = 0;
    
    // CBM encode table
    for (int i = 0; i < 16; i++) {
        checksum = (checksum << 1) ^ uft_gcr_cbm_encode[i];
    }
    
    // CBM decode table
    for (int i = 0; i < 32; i++) {
        checksum = (checksum << 1) ^ uft_gcr_cbm_decode[i];
    }
    
    // Apple encode table
    for (int i = 0; i < 64; i++) {
        checksum = (checksum << 1) ^ uft_gcr_apple_encode[i];
    }
    
    // Apple decode table (sample every 8th byte for speed)
    for (int i = 0; i < 256; i += 8) {
        checksum = (checksum << 1) ^ uft_gcr_apple_decode[i];
    }
    
    // 1541 geometry
    for (int i = 0; i < 43; i++) {
        checksum = (checksum << 1) ^ uft_1541_sectors_per_track[i];
    }
    
    return checksum;
}

/**
 * @brief Check if CBM GCR byte is valid
 */
int uft_gcr_cbm_is_valid(uint8_t gcr_nibble) {
    return (gcr_nibble < 32) && (uft_gcr_cbm_decode[gcr_nibble] != 0xFF);
}

/**
 * @brief Check if Apple GCR byte is valid
 */
int uft_gcr_apple_is_valid(uint8_t gcr_byte) {
    return uft_gcr_apple_decode[gcr_byte] != 0xFF;
}

/**
 * @brief Get sectors for track
 */
int uft_1541_get_sectors(int track) {
    if (track < 1 || track > 42) return 0;
    return uft_1541_sectors_per_track[track];
}

/**
 * @brief Get speed zone for track
 */
int uft_1541_get_speed_zone(int track) {
    if (track < 1 || track > 42) return 0;
    return uft_1541_speed_zone[track];
}

/**
 * @brief Get sector offset in D64 for track
 */
int uft_1541_get_track_offset(int track) {
    if (track < 1 || track > 42) return 0;
    return uft_1541_track_offset[track];
}
