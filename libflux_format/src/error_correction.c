// SPDX-License-Identifier: MIT
/*
 * error_correction.c - MFM Error Correction via Bit-Flipping
 * 
 * Ported from FloppyControl (C# -> C)
 * Original: https://github.com/Skaizo/FloppyControl
 * 
 * This module implements brute-force error correction for MFM data.
 * It systematically tries flipping bits in suspected error regions
 * until a valid CRC is achieved.
 * 
 * WARNING: This is computationally expensive! Use only on small
 * regions where errors are suspected.
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*============================================================================*
 * CONFIGURATION
 *============================================================================*/

/* Maximum bits to brute-force (complexity grows exponentially!) */
#define MAX_BRUTEFORCE_BITS     12

/* Maximum iterations before giving up */
#define MAX_ITERATIONS          (1 << MAX_BRUTEFORCE_BITS)

/*============================================================================*
 * CRC-16 (same as pc_mfm.c)
 *============================================================================*/

extern uint16_t crc16_ccitt(const uint8_t *data, size_t len);
extern uint16_t crc16_with_sync(const uint8_t *data, size_t len);

/* Inline CRC for performance */
static const uint16_t ec_crc_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static inline uint16_t fast_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ ec_crc_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

/*============================================================================*
 * DATA STRUCTURES
 *============================================================================*/

typedef enum {
    EC_SUCCESS = 0,
    EC_NOT_FOUND,
    EC_TIMEOUT,
    EC_INVALID_PARAM,
    EC_NO_MEMORY
} ec_result_t;

typedef struct {
    int bit_positions[MAX_BRUTEFORCE_BITS];  /* Positions of flipped bits */
    int num_flips;                           /* Number of bits flipped */
    uint32_t iterations;                     /* Iterations tried */
} ec_correction_t;

typedef struct {
    int start_bit;          /* Start of error region (bit offset) */
    int end_bit;            /* End of error region */
    int max_flips;          /* Maximum bits to flip (1-12) */
    uint16_t expected_crc;  /* Expected CRC (0 = calculate from corrected) */
    bool verbose;           /* Print progress */
    int (*progress_cb)(int current, int total, void *user_data);
    void *user_data;
} ec_params_t;

/*============================================================================*
 * BIT MANIPULATION
 *============================================================================*/

static inline void flip_bit(uint8_t *data, int bit_pos) {
    int byte_idx = bit_pos / 8;
    int bit_idx = 7 - (bit_pos % 8);
    data[byte_idx] ^= (1 << bit_idx);
}

static inline int get_bit(const uint8_t *data, int bit_pos) {
    int byte_idx = bit_pos / 8;
    int bit_idx = 7 - (bit_pos % 8);
    return (data[byte_idx] >> bit_idx) & 1;
}

/*============================================================================*
 * SINGLE-BIT ERROR CORRECTION
 *============================================================================*/

/**
 * @brief Try to correct single-bit errors
 * 
 * This is fast O(n) where n is the number of bits in the region.
 * 
 * @param data Sector data (will be modified if correction found)
 * @param data_len Data length in bytes
 * @param params Error correction parameters
 * @param correction Output correction info
 * @return EC_SUCCESS if correction found
 */
ec_result_t ec_correct_single_bit(
    uint8_t *data,
    size_t data_len,
    const ec_params_t *params,
    ec_correction_t *correction)
{
    if (!data || !params || !correction) return EC_INVALID_PARAM;
    
    int start = params->start_bit;
    int end = params->end_bit;
    
    if (end <= start || end > (int)(data_len * 8)) {
        return EC_INVALID_PARAM;
    }
    
    correction->num_flips = 0;
    correction->iterations = 0;
    
    /* Calculate expected CRC or use provided */
    uint16_t target_crc = params->expected_crc;
    if (target_crc == 0) {
        /* CRC should be 0 for valid data (CRC is included in data) */
        target_crc = 0;
    }
    
    /* Try flipping each bit in the region */
    for (int bit = start; bit < end; bit++) {
        correction->iterations++;
        
        /* Flip bit */
        flip_bit(data, bit);
        
        /* Check CRC */
        uint16_t crc = fast_crc16(data, data_len);
        
        if (crc == target_crc) {
            /* Found correction! */
            correction->bit_positions[0] = bit;
            correction->num_flips = 1;
            return EC_SUCCESS;
        }
        
        /* Flip back */
        flip_bit(data, bit);
        
        /* Progress callback */
        if (params->progress_cb) {
            if (params->progress_cb(bit - start, end - start, params->user_data) != 0) {
                return EC_TIMEOUT;  /* Cancelled */
            }
        }
    }
    
    return EC_NOT_FOUND;
}

/*============================================================================*
 * MULTI-BIT ERROR CORRECTION (BRUTE FORCE)
 *============================================================================*/

/**
 * @brief Brute-force error correction for multiple bits
 * 
 * WARNING: Complexity is O(n^k) where n is region size and k is max_flips.
 * Use sparingly!
 * 
 * @param data Sector data (will be modified if correction found)
 * @param data_len Data length in bytes
 * @param params Error correction parameters
 * @param correction Output correction info
 * @return EC_SUCCESS if correction found
 */
ec_result_t ec_correct_multi_bit(
    uint8_t *data,
    size_t data_len,
    const ec_params_t *params,
    ec_correction_t *correction)
{
    if (!data || !params || !correction) return EC_INVALID_PARAM;
    
    int start = params->start_bit;
    int end = params->end_bit;
    int max_flips = params->max_flips;
    
    if (max_flips > MAX_BRUTEFORCE_BITS) max_flips = MAX_BRUTEFORCE_BITS;
    if (max_flips < 1) max_flips = 1;
    
    int region_size = end - start;
    if (region_size <= 0 || region_size > 100) {
        return EC_INVALID_PARAM;  /* Region too large */
    }
    
    correction->num_flips = 0;
    correction->iterations = 0;
    
    /* Create working copy */
    uint8_t *work = malloc(data_len);
    if (!work) return EC_NO_MEMORY;
    
    /* Try increasing numbers of bit flips */
    for (int num_flips = 1; num_flips <= max_flips; num_flips++) {
        /* Generate all combinations of num_flips bits in region */
        int positions[MAX_BRUTEFORCE_BITS];
        
        /* Initialize positions */
        for (int i = 0; i < num_flips; i++) {
            positions[i] = start + i;
        }
        
        while (positions[0] <= end - num_flips) {
            correction->iterations++;
            
            /* Apply flips to working copy */
            memcpy(work, data, data_len);
            for (int i = 0; i < num_flips; i++) {
                flip_bit(work, positions[i]);
            }
            
            /* Check CRC */
            uint16_t crc = fast_crc16(work, data_len);
            
            if (crc == 0) {
                /* Found correction! */
                memcpy(data, work, data_len);
                for (int i = 0; i < num_flips; i++) {
                    correction->bit_positions[i] = positions[i];
                }
                correction->num_flips = num_flips;
                free(work);
                return EC_SUCCESS;
            }
            
            /* Progress callback */
            if (params->progress_cb && (correction->iterations % 1000) == 0) {
                if (params->progress_cb(correction->iterations, MAX_ITERATIONS, 
                                       params->user_data) != 0) {
                    free(work);
                    return EC_TIMEOUT;
                }
            }
            
            /* Check iteration limit */
            if (correction->iterations >= MAX_ITERATIONS) {
                free(work);
                return EC_TIMEOUT;
            }
            
            /* Generate next combination */
            int k = num_flips - 1;
            while (k >= 0 && positions[k] >= end - (num_flips - k)) {
                k--;
            }
            
            if (k < 0) break;  /* Done with this number of flips */
            
            positions[k]++;
            for (int i = k + 1; i < num_flips; i++) {
                positions[i] = positions[i-1] + 1;
            }
        }
    }
    
    free(work);
    return EC_NOT_FOUND;
}

/*============================================================================*
 * ERROR REGION DETECTION
 *============================================================================*/

typedef struct {
    int start;
    int end;
    float confidence;
} error_region_t;

/**
 * @brief Detect likely error regions by comparing multiple reads
 * 
 * Compares two captures of the same sector and finds where they differ.
 * Differing regions are likely to contain errors.
 * 
 * @param capture1 First capture
 * @param capture2 Second capture
 * @param len Length (must be same for both)
 * @param regions Output regions
 * @param max_regions Maximum regions to find
 * @return Number of regions found
 */
int ec_detect_error_regions(
    const uint8_t *capture1,
    const uint8_t *capture2,
    size_t len,
    error_region_t *regions,
    int max_regions)
{
    if (!capture1 || !capture2 || !regions) return 0;
    
    int found = 0;
    int in_region = 0;
    int region_start = 0;
    int diff_count = 0;
    
    for (size_t byte_idx = 0; byte_idx < len && found < max_regions; byte_idx++) {
        for (int bit = 7; bit >= 0; bit--) {
            int bit_pos = byte_idx * 8 + (7 - bit);
            int b1 = (capture1[byte_idx] >> bit) & 1;
            int b2 = (capture2[byte_idx] >> bit) & 1;
            
            if (b1 != b2) {
                if (!in_region) {
                    region_start = bit_pos;
                    in_region = 1;
                    diff_count = 0;
                }
                diff_count++;
            } else {
                if (in_region) {
                    /* End of differing region */
                    regions[found].start = region_start;
                    regions[found].end = bit_pos;
                    regions[found].confidence = 1.0f;  /* Simple confidence */
                    found++;
                    in_region = 0;
                }
            }
        }
    }
    
    /* Handle region at end */
    if (in_region && found < max_regions) {
        regions[found].start = region_start;
        regions[found].end = len * 8;
        regions[found].confidence = 1.0f;
        found++;
    }
    
    return found;
}

/*============================================================================*
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Print correction result
 */
void ec_print_correction(const ec_correction_t *correction) {
    if (!correction) return;
    
    printf("Error Correction Result:\n");
    printf("  Iterations: %u\n", correction->iterations);
    printf("  Bits flipped: %d\n", correction->num_flips);
    
    for (int i = 0; i < correction->num_flips; i++) {
        printf("    Position %d: bit %d (byte %d, bit %d)\n",
               i, correction->bit_positions[i],
               correction->bit_positions[i] / 8,
               7 - (correction->bit_positions[i] % 8));
    }
}

/**
 * @brief Get result string
 */
const char *ec_result_string(ec_result_t result) {
    switch (result) {
        case EC_SUCCESS:      return "Success";
        case EC_NOT_FOUND:    return "No correction found";
        case EC_TIMEOUT:      return "Timeout/cancelled";
        case EC_INVALID_PARAM: return "Invalid parameters";
        case EC_NO_MEMORY:    return "Out of memory";
        default:              return "Unknown error";
    }
}
