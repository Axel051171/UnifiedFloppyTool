/**
 * @file uft_protection_ext.c
 * @brief Extended Copy Protection Detection - Longtrack Variants Implementation
 * 
 * Implements specific longtrack detection algorithms based on analysis of
 * 
 * This is a clean-room reimplementation based on documented algorithms.
 * 
 * @copyright UFT Project
 */

#include "uft_protection_ext.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Longtrack Definitions Table
 *============================================================================*/

const uft_longtrack_def_t uft_longtrack_defs[] = {
    /* type, name, sync, sync_bits, min, default, pattern, sig, sig_len */
    { UFT_LONGTRACK_PROTEC,     "PROTEC",           0x4454,     16, 107200, 110000, 0x33, NULL, 0 },
    { UFT_LONGTRACK_PROTOSCAN,  "Protoscan",        0x41244124, 32, 102400, 105500, 0x00, NULL, 0 },
    { UFT_LONGTRACK_TIERTEX,    "Tiertex",          0x41244124, 32, 99328,  100150, 0x00, NULL, 0 },
    { UFT_LONGTRACK_SILMARILS,  "Silmarils",        0xa144,     16, 104128, 110000, 0x00, "ROD0", 4 },
    { UFT_LONGTRACK_INFOGRAMES, "Infogrames",       0xa144,     16, 104160, 105500, 0x00, NULL, 0 },
    { UFT_LONGTRACK_PROLANCE,   "Prolance",         0x8945,     16, 109152, 110000, 0x00, NULL, 0 },
    { UFT_LONGTRACK_APP,        "APP",              0x924a,     16, 110000, 111000, 0xdc, NULL, 0 },
    { UFT_LONGTRACK_SEVENCITIES,"Seven Cities",     0x9251,     16, 101500, 101500, 0x00, NULL, 0 },
    { UFT_LONGTRACK_SUPERMETHANEBROS, "Super Methane Bros", 0x99999999, 32, 52500, 52750, 0x99, NULL, 0 },
    { UFT_LONGTRACK_EMPTY,      "Empty Long",       0x0000,     0,  105000, 110000, 0x00, NULL, 0 },
    { UFT_LONGTRACK_ZEROES,     "Zeroes",           0x0000,     0,  99000,  100000, 0x00, NULL, 0 },
    { UFT_LONGTRACK_RNC_EMPTY,  "RNC Empty",        0x0000,     0,  99000,  100000, 0x00, NULL, 0 },
};

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get bit from data array
 */
static inline int get_bit(const uint8_t *data, size_t offset)
{
    return (data[offset >> 3] >> (7 - (offset & 7))) & 1;
}

/**
 * @brief Get 16-bit word from bitstream
 */
static inline uint16_t get_word16(const uint8_t *data, size_t bit_offset)
{
    uint16_t word = 0;
    for (int i = 0; i < 16; i++) {
        word = (word << 1) | get_bit(data, bit_offset + i);
    }
    return word;
}

/**
 * @brief Get 32-bit word from bitstream
 */
static inline uint32_t get_word32(const uint8_t *data, size_t bit_offset)
{
    uint32_t word = 0;
    for (int i = 0; i < 32; i++) {
        word = (word << 1) | get_bit(data, bit_offset + i);
    }
    return word;
}

/**
 * @brief Find 16-bit sync in bitstream
 */
static int32_t find_sync16(const uint8_t *data, size_t bits, 
                           size_t start, uint16_t sync)
{
    for (size_t i = start; i + 16 <= bits; i++) {
        if (get_word16(data, i) == sync) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Find 32-bit sync in bitstream
 */
static int32_t find_sync32(const uint8_t *data, size_t bits,
                           size_t start, uint32_t sync)
{
    for (size_t i = start; i + 32 <= bits; i++) {
        if (get_word32(data, i) == sync) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Decode MFM byte
 */
static uint8_t mfm_decode_byte(uint16_t mfm)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        if (mfm & (1 << (14 - i * 2))) {
            byte |= (1 << (7 - i));
        }
    }
    return byte;
}

/**
 * @brief Read MFM bytes from bitstream
 */
static void read_mfm_bytes(const uint8_t *bits, size_t bit_offset,
                           uint8_t *out, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        uint16_t mfm = get_word16(bits, bit_offset + i * 16);
        out[i] = mfm_decode_byte(mfm);
    }
}

/**
 * @brief Check if sequence of MFM-decoded bytes matches pattern
 */
static bool check_mfm_sequence(const uint8_t *data, size_t bits,
                               size_t bit_offset, uint8_t pattern,
                               uint32_t min_count)
{
    uint32_t count = 0;
    size_t pos = bit_offset;
    
    while (pos + 16 <= bits && count < min_count + 100) {
        uint16_t mfm = get_word16(data, pos);
        uint8_t byte = mfm_decode_byte(mfm);
        
        if (byte != pattern) {
            break;
        }
        
        count++;
        pos += 16;
    }
    
    return count >= min_count;
}

uint32_t uft_check_pattern_sequence(const uint8_t *data, size_t bits,
                                     size_t offset, uint8_t byte,
                                     uint32_t count)
{
    uint32_t actual = 0;
    size_t pos = offset;
    
    while (pos + 16 <= bits) {
        uint16_t mfm = get_word16(data, pos);
        if (mfm_decode_byte(mfm) != byte) {
            break;
        }
        actual++;
        pos += 16;
    }
    
    return (actual >= count) ? actual : 0;
}

uint16_t uft_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
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

/*============================================================================
 * Track Length Check
 *============================================================================*/

/**
 * @brief Check if track meets minimum length
 */
static bool check_track_length(size_t track_bits, uint32_t min_bits)
{
    return track_bits >= min_bits;
}

/*============================================================================
 * PROTEC Detection
 *============================================================================*/

bool uft_detect_longtrack_protec(const uint8_t *track_data, size_t track_bits,
                                  uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Search for PROTEC sync 0x4454 */
    int32_t sync_pos = find_sync16(track_data, track_bits, 0, UFT_SYNC_PROTEC);
    if (sync_pos < 0) {
        return false;
    }
    
    result->data_bitoff = (uint32_t)sync_pos;
    result->sync_word = UFT_SYNC_PROTEC;
    
    /* Read pattern byte (first MFM byte after sync) */
    size_t data_start = sync_pos + 16;
    if (data_start + 16 > track_bits) {
        return false;
    }
    
    uint8_t pattern = mfm_decode_byte(get_word16(track_data, data_start));
    
    /* Check for repeated pattern (at least 1000 bytes) */
    if (!check_mfm_sequence(track_data, track_bits, data_start, pattern, 1000)) {
        return false;
    }
    
    /* Check track length */
    if (!check_track_length(track_bits, UFT_MINBITS_PROTEC)) {
        return false;
    }
    
    /* Success! */
    result->detected = true;
    result->type = UFT_LONGTRACK_PROTEC;
    result->pattern_byte = pattern;
    result->pattern_count = uft_check_pattern_sequence(track_data, track_bits,
                                                        data_start, pattern, 1000);
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_PROTEC;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.95f;
    
    return true;
}

/*============================================================================
 * Protoscan Detection (Lotus I/II)
 *============================================================================*/

bool uft_detect_longtrack_protoscan(const uint8_t *track_data, size_t track_bits,
                                     uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Search for Protoscan 32-bit sync 0x41244124 */
    int32_t sync_pos = find_sync32(track_data, track_bits, 0, UFT_SYNC_PROTOSCAN);
    if (sync_pos < 0) {
        return false;
    }
    
    result->data_bitoff = (uint32_t)sync_pos;
    result->sync_word = UFT_SYNC_PROTOSCAN;
    
    /* Check for MFM zeroes after sync */
    size_t data_start = sync_pos + 32;
    if (!check_mfm_sequence(track_data, track_bits, data_start, 0x00, 8)) {
        return false;
    }
    
    /* Check track length (>= 102400 bits) */
    if (!check_track_length(track_bits, UFT_MINBITS_PROTOSCAN)) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_PROTOSCAN;
    result->pattern_byte = 0x00;
    result->pattern_count = uft_check_pattern_sequence(track_data, track_bits,
                                                        data_start, 0x00, 8);
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_PROTOSCAN;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.90f;
    
    return true;
}

/*============================================================================
 * Tiertex Detection (Strider II)
 *============================================================================*/

bool uft_detect_longtrack_tiertex(const uint8_t *track_data, size_t track_bits,
                                   uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Same sync as Protoscan */
    int32_t sync_pos = find_sync32(track_data, track_bits, 0, UFT_SYNC_PROTOSCAN);
    if (sync_pos < 0) {
        return false;
    }
    
    result->data_bitoff = (uint32_t)sync_pos;
    result->sync_word = UFT_SYNC_PROTOSCAN;
    
    /* Check for MFM zeroes after sync */
    size_t data_start = sync_pos + 32;
    if (!check_mfm_sequence(track_data, track_bits, data_start, 0x00, 8)) {
        return false;
    }
    
    /* Tiertex has specific length range: 99328 <= bits <= 103680 */
    if (track_bits < UFT_MINBITS_TIERTEX_MIN || track_bits > UFT_MINBITS_TIERTEX_MAX) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_TIERTEX;
    result->pattern_byte = 0x00;
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_TIERTEX_MIN;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.85f;
    
    return true;
}

/*============================================================================
 * Silmarils Detection
 *============================================================================*/

bool uft_detect_longtrack_silmarils(const uint8_t *track_data, size_t track_bits,
                                     uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Search for Silmarils sync 0xa144 (with 0xaaaa preamble) */
    for (size_t i = 0; i + 32 <= track_bits; i++) {
        uint32_t word = get_word32(track_data, i);
        if (word == 0xaaaaa144) {
            result->data_bitoff = (uint32_t)i + 16;  /* After 0xaaaa */
            result->sync_word = UFT_SYNC_SILMARILS;
            
            /* Check for "ROD0" signature (4 bytes after sync) */
            size_t sig_start = i + 32;  /* After full 0xaaaaa144 */
            if (sig_start + 64 > track_bits) {
                continue;
            }
            
            uint8_t sig[4];
            read_mfm_bytes(track_data, sig_start, sig, 4);
            
            if (memcmp(sig, UFT_SIG_SILMARILS, UFT_SIG_SILMARILS_LEN) != 0) {
                continue;
            }
            
            result->signature_found = true;
            memcpy(result->signature, UFT_SIG_SILMARILS, UFT_SIG_SILMARILS_LEN);
            
            /* Check for MFM zeroes after signature */
            size_t data_start = sig_start + 64;  /* After 4-byte signature */
            if (!check_mfm_sequence(track_data, track_bits, data_start, 0x00, 6500)) {
                continue;
            }
            
            /* Check track length */
            if (!check_track_length(track_bits, UFT_MINBITS_SILMARILS)) {
                continue;
            }
            
            result->detected = true;
            result->type = UFT_LONGTRACK_SILMARILS;
            result->pattern_byte = 0x00;
            result->pattern_count = uft_check_pattern_sequence(track_data, track_bits,
                                                                data_start, 0x00, 6500);
            result->track_bits = (uint32_t)track_bits;
            result->min_required = UFT_MINBITS_SILMARILS;
            result->percent = (uint16_t)((track_bits * 100) / 100000);
            result->confidence = 0.95f;
            
            return true;
        }
    }
    
    return false;
}

/*============================================================================
 * Infogrames Detection
 *============================================================================*/

bool uft_detect_longtrack_infogrames(const uint8_t *track_data, size_t track_bits,
                                      uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Search for sync 0xa144 */
    int32_t sync_pos = find_sync16(track_data, track_bits, 0, UFT_SYNC_SILMARILS);
    if (sync_pos < 0) {
        return false;
    }
    
    result->data_bitoff = (uint32_t)sync_pos;
    result->sync_word = UFT_SYNC_SILMARILS;
    
    /* Skip sync and check for MFM zeroes */
    size_t data_start = sync_pos + 16;
    
    /* Infogrames checks for >13020 0xaa raw bytes = 6510 MFM bytes */
    if (!check_mfm_sequence(track_data, track_bits, data_start, 0x00, 6510)) {
        return false;
    }
    
    /* Check track length */
    if (!check_track_length(track_bits, UFT_MINBITS_INFOGRAMES)) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_INFOGRAMES;
    result->pattern_byte = 0x00;
    result->pattern_count = uft_check_pattern_sequence(track_data, track_bits,
                                                        data_start, 0x00, 6510);
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_INFOGRAMES;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.90f;
    
    return true;
}

/*============================================================================
 * Prolance Detection (B.A.T.)
 *============================================================================*/

bool uft_detect_longtrack_prolance(const uint8_t *track_data, size_t track_bits,
                                    uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Search for sync with 0xaaaa preamble */
    for (size_t i = 0; i + 32 <= track_bits; i++) {
        uint32_t word = get_word32(track_data, i);
        if (word == 0xaaaa8945) {
            result->data_bitoff = (uint32_t)i + 16;
            result->sync_word = UFT_SYNC_PROLANCE;
            
            /* Check for MFM zeroes (>= 3412 longwords = 6824 words) */
            size_t data_start = i + 32;
            if (!check_mfm_sequence(track_data, track_bits, data_start, 0x00, 6826)) {
                continue;
            }
            
            /* Check track length */
            if (!check_track_length(track_bits, UFT_MINBITS_PROLANCE)) {
                continue;
            }
            
            result->detected = true;
            result->type = UFT_LONGTRACK_PROLANCE;
            result->pattern_byte = 0x00;
            result->pattern_count = uft_check_pattern_sequence(track_data, track_bits,
                                                                data_start, 0x00, 6826);
            result->track_bits = (uint32_t)track_bits;
            result->min_required = UFT_MINBITS_PROLANCE;
            result->percent = (uint16_t)((track_bits * 100) / 100000);
            result->confidence = 0.95f;
            
            return true;
        }
    }
    
    return false;
}

/*============================================================================
 * APP Detection (Amiga Power Pack)
 *============================================================================*/

bool uft_detect_longtrack_app(const uint8_t *track_data, size_t track_bits,
                               uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Search for sync 0x924a */
    int32_t sync_pos = find_sync16(track_data, track_bits, 0, UFT_SYNC_APP);
    if (sync_pos < 0) {
        return false;
    }
    
    result->data_bitoff = (uint32_t)sync_pos;
    result->sync_word = UFT_SYNC_APP;
    
    /* Check for 0xdc pattern (6600 times) */
    size_t data_start = sync_pos + 16;
    if (!check_mfm_sequence(track_data, track_bits, data_start, 0xdc, 6600)) {
        return false;
    }
    
    /* Check track length */
    if (!check_track_length(track_bits, UFT_MINBITS_APP)) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_APP;
    result->pattern_byte = 0xdc;
    result->pattern_count = uft_check_pattern_sequence(track_data, track_bits,
                                                        data_start, 0xdc, 6600);
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_APP;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.95f;
    
    return true;
}

/*============================================================================
 * Seven Cities Of Gold Detection
 *============================================================================*/

#define SEVENCITIES_DATA_LEN  122
#define SEVENCITIES_CRC       0x010a

bool uft_detect_longtrack_sevencities(const uint8_t *track_data, size_t track_bits,
                                       uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* First find 0x924a sync */
    int32_t sync1_pos = find_sync16(track_data, track_bits, 0, UFT_SYNC_SEVENCITIES_2);
    if (sync1_pos < 0) {
        return false;
    }
    
    /* Then find 0x9251 sync after that */
    int32_t sync2_pos = find_sync16(track_data, track_bits, sync1_pos + 16, 
                                     UFT_SYNC_SEVENCITIES_1);
    if (sync2_pos < 0) {
        return false;
    }
    
    result->data_bitoff = (uint32_t)sync2_pos;
    result->sync_word = UFT_SYNC_SEVENCITIES_1;
    
    /* Read 122 bytes of protection data */
    size_t data_start = sync2_pos + 16;
    if (data_start + SEVENCITIES_DATA_LEN * 16 > track_bits) {
        return false;
    }
    
    uint8_t prot_data[SEVENCITIES_DATA_LEN];
    
    /* Note: Seven Cities uses raw bytes, not MFM-decoded */
    for (size_t i = 0; i < SEVENCITIES_DATA_LEN; i++) {
        prot_data[i] = 0;
        for (int b = 0; b < 8; b++) {
            prot_data[i] |= get_bit(track_data, data_start + i * 8 + b) << (7 - b);
        }
    }
    
    /* Check CRC */
    uint16_t crc = uft_crc16_ccitt(prot_data, SEVENCITIES_DATA_LEN);
    if (crc != SEVENCITIES_CRC) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_SEVENCITIES;
    result->crc = crc;
    memcpy(result->extra_data, prot_data, SEVENCITIES_DATA_LEN);
    result->extra_data_len = SEVENCITIES_DATA_LEN;
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_SEVENCITIES;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.98f;  /* High confidence due to CRC match */
    
    return true;
}

/*============================================================================
 * Super Methane Bros Detection (GCR)
 *============================================================================*/

bool uft_detect_longtrack_supermethanebros(const uint8_t *track_data, 
                                            size_t track_bits,
                                            const uint32_t *flux_times,
                                            size_t num_flux,
                                            uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 25000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* This is a GCR track (4µs bit time vs 2µs MFM) */
    /* Look for pattern 0x99999999 */
    
    uint32_t match_count = 0;
    size_t i = 0;
    
    while (i + 32 <= track_bits) {
        uint32_t word = get_word32(track_data, i);
        if (word == UFT_PATTERN_SUPERMETHANEBROS) {
            match_count++;
            i += 32;
        } else {
            i++;
        }
    }
    
    /* Need predominantly GCR 99999999 pattern */
    /* Expected: ~52750/32 = ~1648 matches minimum */
    if (match_count < 100000 / (2 * 32)) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_SUPERMETHANEBROS;
    result->sync_word = UFT_PATTERN_SUPERMETHANEBROS;
    result->pattern_byte = 0x99;
    result->pattern_count = match_count;
    result->track_bits = (uint32_t)track_bits;
    result->min_required = 52500;
    result->percent = (uint16_t)((track_bits * 100) / 52750);
    result->confidence = 0.90f;
    
    return true;
}

/*============================================================================
 * Empty/Zeroes Detection
 *============================================================================*/

bool uft_detect_longtrack_empty(const uint8_t *track_data, size_t track_bits,
                                 uft_longtrack_ext_t *result)
{
    if (!track_data || !result || track_bits < 50000) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Check track length first */
    if (!check_track_length(track_bits, UFT_MINBITS_EMPTY)) {
        return false;
    }
    
    /* Count discontinuities and check for zeroes (0xaa or 0x55 raw) */
    uint32_t discontinuities = 0;
    uint32_t max_run = 0;
    uint32_t current_run = 0;
    uint32_t prev_word = 0;
    
    for (size_t i = 0; i + 32 <= track_bits; i += 32) {
        uint32_t word = get_word32(track_data, i);
        
        if (word == 0xaaaaaaaa || word == 0x55555555) {
            if (current_run == 0 || word == prev_word) {
                current_run++;
            } else {
                if (current_run > max_run) max_run = current_run;
                discontinuities++;
                current_run = 1;
            }
        } else {
            if (current_run > max_run) max_run = current_run;
            if (current_run > 0) discontinuities++;
            current_run = 0;
        }
        
        prev_word = word;
    }
    
    if (current_run > max_run) max_run = current_run;
    
    /* Not too many discontinuities and a nice long run of zeroes */
    if (discontinuities > 5 || max_run < (99000 / 32)) {
        return false;
    }
    
    result->detected = true;
    result->type = UFT_LONGTRACK_EMPTY;
    result->pattern_byte = 0x00;
    result->pattern_count = max_run * 32;
    result->track_bits = (uint32_t)track_bits;
    result->min_required = UFT_MINBITS_EMPTY;
    result->percent = (uint16_t)((track_bits * 100) / 100000);
    result->confidence = 0.80f;
    
    return true;
}

/*============================================================================
 * Universal Longtrack Detection
 *============================================================================*/

bool uft_detect_longtrack_ext(const uint8_t *track_data, size_t track_bits,
                               const uint32_t *flux_times, size_t num_flux,
                               uft_longtrack_ext_t *result)
{
    if (!track_data || !result) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Try specific types in priority order */
    
    /* High-confidence types first */
    if (uft_detect_longtrack_sevencities(track_data, track_bits, result)) {
        return true;
    }
    
    if (uft_detect_longtrack_silmarils(track_data, track_bits, result)) {
        return true;
    }
    
    if (uft_detect_longtrack_protec(track_data, track_bits, result)) {
        return true;
    }
    
    if (uft_detect_longtrack_app(track_data, track_bits, result)) {
        return true;
    }
    
    if (uft_detect_longtrack_prolance(track_data, track_bits, result)) {
        return true;
    }
    
    /* Protoscan and Tiertex use same sync - try Tiertex first (narrower range) */
    if (uft_detect_longtrack_tiertex(track_data, track_bits, result)) {
        return true;
    }
    
    if (uft_detect_longtrack_protoscan(track_data, track_bits, result)) {
        return true;
    }
    
    /* Infogrames uses same sync as Silmarils but no signature */
    if (uft_detect_longtrack_infogrames(track_data, track_bits, result)) {
        return true;
    }
    
    /* Special: GCR track */
    if (uft_detect_longtrack_supermethanebros(track_data, track_bits, 
                                               flux_times, num_flux, result)) {
        return true;
    }
    
    /* Fallback: empty long track */
    if (uft_detect_longtrack_empty(track_data, track_bits, result)) {
        return true;
    }
    
    return false;
}

/*============================================================================
 * Generation Functions
 *============================================================================*/

/**
 * @brief Set bit in data array
 */
static inline void set_bit(uint8_t *data, size_t offset, int value)
{
    if (value) {
        data[offset >> 3] |= (1 << (7 - (offset & 7)));
    } else {
        data[offset >> 3] &= ~(1 << (7 - (offset & 7)));
    }
}

/**
 * @brief Write 16-bit word to bitstream
 */
static void write_word16(uint8_t *data, size_t bit_offset, uint16_t word)
{
    for (int i = 0; i < 16; i++) {
        set_bit(data, bit_offset + i, (word >> (15 - i)) & 1);
    }
}

/**
 * @brief Write MFM-encoded byte
 */
static void write_mfm_byte(uint8_t *data, size_t bit_offset, uint8_t byte)
{
    uint16_t mfm = 0;
    int prev_bit = 0;
    
    for (int i = 0; i < 8; i++) {
        int data_bit = (byte >> (7 - i)) & 1;
        int clock_bit = (!prev_bit && !data_bit) ? 1 : 0;
        
        mfm = (mfm << 2) | (clock_bit << 1) | data_bit;
        prev_bit = data_bit;
    }
    
    write_word16(data, bit_offset, mfm);
}

size_t uft_generate_longtrack_protec(uint8_t pattern_byte, uint32_t total_bits,
                                      uint8_t *track_data)
{
    if (!track_data) return 0;
    
    size_t bytes_needed = (total_bits + 7) / 8;
    memset(track_data, 0, bytes_needed);
    
    size_t pos = 0;
    
    /* Write sync 0x4454 (raw) */
    write_word16(track_data, pos, UFT_SYNC_PROTEC);
    pos += 16;
    
    /* Write pattern bytes (MFM encoded) */
    uint32_t pattern_count = (total_bits - 250) / 16;  /* Reserve space */
    for (uint32_t i = 0; i < pattern_count && pos + 16 <= total_bits; i++) {
        write_mfm_byte(track_data, pos, pattern_byte);
        pos += 16;
    }
    
    return bytes_needed;
}

size_t uft_generate_longtrack_protoscan(uint32_t total_bits, uint8_t *track_data)
{
    if (!track_data) return 0;
    
    size_t bytes_needed = (total_bits + 7) / 8;
    memset(track_data, 0, bytes_needed);
    
    size_t pos = 0;
    
    /* Write 32-bit sync 0x41244124 (raw) */
    write_word16(track_data, pos, 0x4124);
    pos += 16;
    write_word16(track_data, pos, 0x4124);
    pos += 16;
    
    /* Fill with MFM zeroes */
    uint32_t zero_count = (total_bits - 500) / 16;
    for (uint32_t i = 0; i < zero_count && pos + 16 <= total_bits; i++) {
        write_mfm_byte(track_data, pos, 0x00);
        pos += 16;
    }
    
    return bytes_needed;
}

size_t uft_generate_longtrack_silmarils(uint32_t total_bits, uint8_t *track_data)
{
    if (!track_data) return 0;
    
    size_t bytes_needed = (total_bits + 7) / 8;
    memset(track_data, 0, bytes_needed);
    
    size_t pos = 0;
    
    /* Write sync 0xa144 (raw) */
    write_word16(track_data, pos, UFT_SYNC_SILMARILS);
    pos += 16;
    
    /* Write "ROD0" signature (MFM) */
    const char *sig = UFT_SIG_SILMARILS;
    for (int i = 0; i < UFT_SIG_SILMARILS_LEN; i++) {
        write_mfm_byte(track_data, pos, (uint8_t)sig[i]);
        pos += 16;
    }
    
    /* Fill with MFM zeroes */
    uint32_t zero_count = (total_bits - 500) / 16;
    for (uint32_t i = 0; i < zero_count && pos + 16 <= total_bits; i++) {
        write_mfm_byte(track_data, pos, 0x00);
        pos += 16;
    }
    
    return bytes_needed;
}

size_t uft_generate_longtrack(uft_longtrack_type_t type,
                               const uft_longtrack_ext_t *params,
                               uint8_t *track_data,
                               uint32_t *track_bits)
{
    if (!track_data || !track_bits) return 0;
    
    uint32_t bits = UFT_DEFBITS_PROTEC;  /* Default */
    uint8_t pattern = 0x33;               /* Default */
    
    if (params) {
        if (params->track_bits > 0) bits = params->track_bits;
        if (params->pattern_byte != 0) pattern = params->pattern_byte;
    }
    
    *track_bits = bits;
    
    switch (type) {
        case UFT_LONGTRACK_PROTEC:
            return uft_generate_longtrack_protec(pattern, bits, track_data);
            
        case UFT_LONGTRACK_PROTOSCAN:
        case UFT_LONGTRACK_TIERTEX:
            *track_bits = (type == UFT_LONGTRACK_TIERTEX) ? 
                          UFT_DEFBITS_TIERTEX : UFT_DEFBITS_PROTOSCAN;
            return uft_generate_longtrack_protoscan(*track_bits, track_data);
            
        case UFT_LONGTRACK_SILMARILS:
            *track_bits = UFT_DEFBITS_SILMARILS;
            return uft_generate_longtrack_silmarils(*track_bits, track_data);
            
        /* Add more generators as needed */
        default:
            return 0;
    }
}

/*============================================================================
 * Utility Functions
 * R21 FIX: Renamed to avoid duplicate symbols - main versions in uft_longtrack.c
 *============================================================================*/

static const char* uft_longtrack_type_name_ext(uft_longtrack_type_t type)
{
    for (int i = 0; i < UFT_LONGTRACK_DEF_COUNT; i++) {
        if (uft_longtrack_defs[i].type == type) {
            return uft_longtrack_defs[i].name;
        }
    }
    return "Unknown";
}

static const uft_longtrack_def_t* uft_longtrack_get_def_ext(uft_longtrack_type_t type)
{
    for (int i = 0; i < UFT_LONGTRACK_DEF_COUNT; i++) {
        if (uft_longtrack_defs[i].type == type) {
            return &uft_longtrack_defs[i];
        }
    }
    return NULL;
}

void uft_longtrack_ext_print(const uft_longtrack_ext_t *result, bool verbose)
{
    if (!result) return;
    
    printf("Longtrack Detection Results:\n");
    printf("  Detected: %s\n", result->detected ? "Yes" : "No");
    
    if (!result->detected) return;
    
    printf("  Type: %s\n", uft_longtrack_type_name_ext(result->type));
    printf("  Confidence: %.0f%%\n", result->confidence * 100);
    printf("  Track Length: %u bits (%u%%)\n", 
           result->track_bits, result->percent);
    printf("  Minimum Required: %u bits\n", result->min_required);
    
    if (result->sync_word != 0) {
        printf("  Sync Word: 0x%04X\n", result->sync_word);
    }
    
    if (result->pattern_byte != 0 || result->pattern_count > 0) {
        printf("  Pattern: 0x%02X x %u\n", 
               result->pattern_byte, result->pattern_count);
    }
    
    if (result->signature_found) {
        printf("  Signature: \"%s\"\n", result->signature);
    }
    
    if (verbose && result->extra_data_len > 0) {
        printf("  Extra Data (%u bytes):", result->extra_data_len);
        for (size_t i = 0; i < result->extra_data_len && i < 16; i++) {
            printf(" %02X", result->extra_data[i]);
        }
        if (result->extra_data_len > 16) {
            printf(" ...");
        }
        printf("\n");
    }
    
    if (result->crc != 0) {
        printf("  CRC: 0x%04X\n", result->crc);
    }
    
    printf("  Data Offset: %u bits\n", result->data_bitoff);
}
