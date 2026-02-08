/**
 * @file uft_bitstream_recovery.c
 * @brief UFT Bitstream-Level Recovery Module
 * 
 * Bitstream recovery for damaged or degraded data:
 * - CRC-based error detection and correction
 * - Pattern matching for sync recovery
 * - Bit slip detection and correction
 * - Missing bit interpolation
 * 
 * @version 5.28.0 GOD MODE
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Types
 * ============================================================================ */

typedef enum {
    UFT_BS_ENC_MFM,
    UFT_BS_ENC_FM,
    UFT_BS_ENC_GCR_C64,
    UFT_BS_ENC_GCR_APPLE,
    UFT_BS_ENC_AMIGA
} uft_bs_encoding_t;

typedef struct {
    size_t position;
    size_t length;
    uint8_t type;       /* 0=missing, 1=extra, 2=corrupted */
    uint8_t confidence;
} uft_bs_error_t;

typedef struct {
    uint8_t *corrected_bits;
    size_t   bit_count;
    uft_bs_error_t *errors;
    size_t   error_count;
    uint32_t corrections;
    uint8_t  confidence;
} uft_bs_recovery_result_t;

typedef struct {
    uft_bs_encoding_t encoding;
    bool     attempt_crc_correction;
    bool     recover_sync;
    uint8_t  max_bit_slip;
    uint8_t  recovery_level;
} uft_bs_recovery_config_t;

/* ============================================================================
 * Sync Pattern Tables
 * ============================================================================ */

/* MFM sync: A1 A1 A1 with missing clock */
static const uint8_t MFM_SYNC[] = {0x44, 0x89, 0x44, 0x89, 0x44, 0x89};
static const size_t MFM_SYNC_LEN = 6;

/* C64 GCR sync: 10 one bits */
static const uint8_t GCR_C64_SYNC[] = {0xFF, 0xFF, 0xC0};
static const size_t GCR_C64_SYNC_LEN = 3;

/* Apple II sync: D5 AA */
static const uint8_t APPLE_SYNC[] = {0xD5, 0xAA};
static const size_t APPLE_SYNC_LEN = 2;

/* ============================================================================
 * CRC Functions
 * ============================================================================ */

/**
 * @brief Calculate CRC-16 CCITT
 */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
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

/**
 * @brief Attempt single-bit CRC correction
 * 
 * Tries flipping each bit to see if CRC becomes valid
 */
static int try_single_bit_correction(uint8_t *data, size_t len, 
                                     uint16_t expected_crc,
                                     size_t *corrected_byte,
                                     uint8_t *corrected_bit)
{
    for (size_t i = 0; i < len; i++) {
        for (int b = 0; b < 8; b++) {
            /* Flip bit */
            data[i] ^= (1 << b);
            
            /* Check CRC */
            if (crc16_ccitt(data, len) == expected_crc) {
                *corrected_byte = i;
                *corrected_bit = b;
                return 1;  /* Success */
            }
            
            /* Restore bit */
            data[i] ^= (1 << b);
        }
    }
    
    return 0;  /* No single-bit correction found */
}

/**
 * @brief Attempt double-bit CRC correction
 */
static int try_double_bit_correction(uint8_t *data, size_t len,
                                     uint16_t expected_crc)
{
    size_t total_bits = len * 8;
    
    for (size_t i = 0; i + 1 < total_bits; i++) {
        for (size_t j = i + 1; j < total_bits; j++) {
            /* Flip both bits */
            data[i / 8] ^= (1 << (i % 8));
            data[j / 8] ^= (1 << (j % 8));
            
            /* Check CRC */
            if (crc16_ccitt(data, len) == expected_crc) {
                return 1;  /* Success - leave bits flipped */
            }
            
            /* Restore bits */
            data[i / 8] ^= (1 << (i % 8));
            data[j / 8] ^= (1 << (j % 8));
        }
    }
    
    return 0;
}

/* ============================================================================
 * Sync Recovery
 * ============================================================================ */

/**
 * @brief Find sync pattern with fuzzy matching
 */
static int find_sync_fuzzy(const uint8_t *bitstream, size_t bit_count,
                           const uint8_t *pattern, size_t pattern_len,
                           size_t start, int max_errors,
                           size_t *sync_pos, int *error_count)
{
    size_t byte_count = (bit_count + 7) / 8;
    
    for (size_t i = start; i + pattern_len < byte_count; i++) {
        int errors = 0;
        
        for (size_t j = 0; j < pattern_len && errors <= max_errors; j++) {
            uint8_t diff = bitstream[i + j] ^ pattern[j];
            /* Count differing bits */
            while (diff) {
                errors += diff & 1;
                diff >>= 1;
            }
        }
        
        if (errors <= max_errors) {
            *sync_pos = i;
            *error_count = errors;
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief Repair corrupted sync pattern
 */
static void repair_sync(uint8_t *bitstream, size_t sync_pos,
                        const uint8_t *pattern, size_t pattern_len)
{
    memcpy(bitstream + sync_pos, pattern, pattern_len);
}

/* ============================================================================
 * Bit Slip Detection
 * ============================================================================ */

/**
 * @brief Detect bit slip (missing or extra bits)
 */
static int detect_bit_slip(const uint8_t *bitstream, size_t bit_count,
                           size_t expected_sector_bits,
                           int *slip_amount, size_t *slip_position)
{
    /* Look for sync patterns and check spacing */
    size_t byte_count = (bit_count + 7) / 8;
    size_t last_sync = 0;
    size_t sync_count = 0;
    size_t total_gap = 0;
    
    (void)total_gap; /* Reserved for future gap analysis */
    for (size_t i = 0; i + MFM_SYNC_LEN < byte_count; i++) {
        bool match = true;
        for (size_t j = 0; j < MFM_SYNC_LEN && match; j++) {
            if (bitstream[i + j] != MFM_SYNC[j]) {
                match = false;
            }
        }
        
        if (match) {
            if (sync_count > 0) {
                size_t gap = (i - last_sync) * 8;
                total_gap += gap;
                
                /* Check if gap differs from expected */
                int diff = (int)gap - (int)expected_sector_bits;
                if (abs(diff) > 16 && abs(diff) < 64) {
                    *slip_amount = diff;
                    *slip_position = last_sync * 8 + expected_sector_bits / 2;
                    return 1;
                }
            }
            last_sync = i;
            sync_count++;
        }
    }
    
    return 0;
}

/**
 * @brief Correct bit slip by inserting/removing bits
 */
static int correct_bit_slip(uint8_t *bitstream, size_t *bit_count,
                            int slip_amount, size_t slip_position)
{
    size_t byte_count = (*bit_count + 7) / 8;
    
    if (slip_amount > 0) {
        /* Extra bits - remove them */
        size_t remove_bytes = (slip_amount + 7) / 8;
        size_t byte_pos = slip_position / 8;
        
        memmove(bitstream + byte_pos, 
                bitstream + byte_pos + remove_bytes,
                byte_count - byte_pos - remove_bytes);
        
        *bit_count -= slip_amount;
    } else if (slip_amount < 0) {
        /* Missing bits - insert zeros */
        size_t insert_bytes = (-slip_amount + 7) / 8;
        size_t byte_pos = slip_position / 8;
        
        memmove(bitstream + byte_pos + insert_bytes,
                bitstream + byte_pos,
                byte_count - byte_pos);
        
        memset(bitstream + byte_pos, 0, insert_bytes);
        *bit_count += -slip_amount;
    }
    
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize bitstream recovery configuration
 */
void uft_bs_recovery_config_init(uft_bs_recovery_config_t *config)
{
    config->encoding = UFT_BS_ENC_MFM;
    config->attempt_crc_correction = true;
    config->recover_sync = true;
    config->max_bit_slip = 8;
    config->recovery_level = 1;
}

/**
 * @brief Recover bitstream with CRC-based correction
 */
int uft_bs_recover_crc(uint8_t *data, size_t len,
                       uint16_t expected_crc, uint32_t *corrections)
{
    if (!data || len == 0) return -1;
    
    *corrections = 0;
    
    /* Check if already valid */
    if (crc16_ccitt(data, len) == expected_crc) {
        return 0;
    }
    
    /* Try single-bit correction */
    size_t corr_byte;
    uint8_t corr_bit;
    if (try_single_bit_correction(data, len, expected_crc, &corr_byte, &corr_bit)) {
        *corrections = 1;
        return 0;
    }
    
    /* Try double-bit correction */
    if (try_double_bit_correction(data, len, expected_crc)) {
        *corrections = 2;
        return 0;
    }
    
    return -1;  /* Could not correct */
}

/**
 * @brief Recover sync patterns in bitstream
 */
int uft_bs_recover_sync(uint8_t *bitstream, size_t bit_count,
                        uft_bs_encoding_t encoding, uint32_t *repairs)
{
    if (!bitstream || bit_count == 0) return -1;
    
    const uint8_t *sync_pattern;
    size_t sync_len;
    
    switch (encoding) {
        case UFT_BS_ENC_MFM:
            sync_pattern = MFM_SYNC;
            sync_len = MFM_SYNC_LEN;
            break;
        case UFT_BS_ENC_GCR_C64:
            sync_pattern = GCR_C64_SYNC;
            sync_len = GCR_C64_SYNC_LEN;
            break;
        case UFT_BS_ENC_GCR_APPLE:
            sync_pattern = APPLE_SYNC;
            sync_len = APPLE_SYNC_LEN;
            break;
        default:
            sync_pattern = MFM_SYNC;
            sync_len = MFM_SYNC_LEN;
            break;
    }
    
    *repairs = 0;
    size_t pos = 0;
    size_t sync_pos;
    int error_count;
    
    while (find_sync_fuzzy(bitstream, bit_count, sync_pattern, sync_len,
                           pos, 2, &sync_pos, &error_count)) {
        if (error_count > 0) {
            repair_sync(bitstream, sync_pos, sync_pattern, sync_len);
            (*repairs)++;
        }
        pos = sync_pos + sync_len;
    }
    
    return 0;
}

/**
 * @brief Detect and correct bit slip
 */
int uft_bs_recover_slip(uint8_t *bitstream, size_t *bit_count,
                        size_t expected_sector_bits, uint32_t *corrections)
{
    if (!bitstream || !bit_count || *bit_count == 0) return -1;
    
    *corrections = 0;
    
    int slip_amount;
    size_t slip_position;
    
    while (detect_bit_slip(bitstream, *bit_count, expected_sector_bits,
                           &slip_amount, &slip_position)) {
        correct_bit_slip(bitstream, bit_count, slip_amount, slip_position);
        (*corrections)++;
        
        if (*corrections > 10) break;  /* Prevent infinite loop */
    }
    
    return 0;
}

/**
 * @brief Full bitstream recovery
 */
int uft_bs_recover_full(uint8_t *bitstream, size_t *bit_count,
                        const uft_bs_recovery_config_t *config,
                        uft_bs_recovery_result_t *result)
{
    if (!bitstream || !bit_count || !config || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Step 1: Recover sync patterns */
    uint32_t sync_repairs = 0;
    if (config->recover_sync) {
        uft_bs_recover_sync(bitstream, *bit_count, config->encoding, &sync_repairs);
        result->corrections += sync_repairs;
    }
    
    /* Step 2: Detect and correct bit slip */
    uint32_t slip_corrections = 0;
    if (config->max_bit_slip > 0) {
        uft_bs_recover_slip(bitstream, bit_count, 
                            512 * 16,  /* Typical MFM sector */
                            &slip_corrections);
        result->corrections += slip_corrections;
    }
    
    /* Allocate result buffer */
    result->bit_count = *bit_count;
    result->corrected_bits = malloc((*bit_count + 7) / 8);
    if (!result->corrected_bits) {
        return -1;  /* Memory allocation failed */
    }
    memcpy(result->corrected_bits, bitstream, (*bit_count + 7) / 8);
    
    /* Calculate confidence (prevent underflow for uint8_t) */
    int temp_conf = 100 - (result->corrections * 5);
    result->confidence = (temp_conf > 0) ? (uint8_t)temp_conf : 0;
    
    return 0;
}

/**
 * @brief Free recovery result
 */
void uft_bs_recovery_result_free(uft_bs_recovery_result_t *result)
{
    if (result) {
        free(result->corrected_bits);
        free(result->errors);
        memset(result, 0, sizeof(*result));
    }
}
