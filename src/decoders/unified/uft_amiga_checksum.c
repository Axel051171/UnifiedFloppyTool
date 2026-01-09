#include "uft_error.h"
#include "uft_error_compat.h"
#include "uft_track.h"
/**
 * @file uft_amiga_checksum.c
 * @brief Amiga MFM Checksum Verification
 * 
 * AUDIT FIX: TODOs in uft_amiga_mfm_decoder_v2.c:182,187,218
 * 
 * Implements proper checksum verification for Amiga MFM sectors.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// AMIGA CHECKSUM ALGORITHM
// ============================================================================

/**
 * @brief Calculate Amiga sector checksum
 * 
 * The Amiga checksum is calculated by XORing all the odd and even
 * MFM-encoded 32-bit words together.
 * 
 * @param odd_data Array of odd MFM words
 * @param even_data Array of even MFM words
 * @param count Number of 32-bit words
 * @return Calculated checksum
 */
static uint32_t amiga_calc_checksum(const uint32_t* odd_data, 
                                     const uint32_t* even_data,
                                     size_t count) {
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < count; i++) {
        checksum ^= odd_data[i];
        checksum ^= even_data[i];
    }
    
    // Mask to data bits only (remove clock bits)
    checksum &= 0x55555555;
    
    return checksum;
}

/**
 * @brief Decode MFM odd/even pair to single value
 */
static inline uint32_t amiga_decode_long(uint32_t odd, uint32_t even) {
    return ((odd & 0x55555555) << 1) | (even & 0x55555555);
}

/**
 * @brief Verify Amiga sector header checksum
 * 
 * Header consists of:
 * - info_odd (1 long)
 * - info_even (1 long)
 * - label_odd (4 longs)
 * - label_even (4 longs)
 * Total: 10 longs for checksum calculation
 * 
 * @param info_odd MFM-encoded info (odd bits)
 * @param info_even MFM-encoded info (even bits)
 * @param label_odd 4 MFM-encoded label longs (odd bits)
 * @param label_even 4 MFM-encoded label longs (even bits)
 * @param stored_checksum_odd Stored checksum (odd bits)
 * @param stored_checksum_even Stored checksum (even bits)
 * @return true if checksum matches
 */
bool amiga_verify_header_checksum(uint32_t info_odd, uint32_t info_even,
                                   const uint32_t label_odd[4],
                                   const uint32_t label_even[4],
                                   uint32_t stored_checksum_odd,
                                   uint32_t stored_checksum_even) {
    // Calculate checksum over header data
    uint32_t checksum = 0;
    
    // Info
    checksum ^= info_odd;
    checksum ^= info_even;
    
    // Label (4 longs)
    for (int i = 0; i < 4; i++) {
        checksum ^= label_odd[i];
        checksum ^= label_even[i];
    }
    
    // Mask to data bits
    checksum &= 0x55555555;
    
    // Decode stored checksum
    uint32_t stored = amiga_decode_long(stored_checksum_odd, stored_checksum_even);
    
    return (checksum == stored);
}

/**
 * @brief Verify Amiga sector data checksum
 * 
 * Data consists of 128 longs (512 bytes when decoded).
 * 
 * @param odd_data 128 MFM-encoded data longs (odd bits)
 * @param even_data 128 MFM-encoded data longs (even bits)
 * @param stored_checksum_odd Stored checksum (odd bits)
 * @param stored_checksum_even Stored checksum (even bits)
 * @return true if checksum matches
 */
bool amiga_verify_data_checksum(const uint32_t odd_data[128],
                                 const uint32_t even_data[128],
                                 uint32_t stored_checksum_odd,
                                 uint32_t stored_checksum_even) {
    // Calculate checksum over all data
    uint32_t checksum = amiga_calc_checksum(odd_data, even_data, 128);
    
    // Decode stored checksum
    uint32_t stored = amiga_decode_long(stored_checksum_odd, stored_checksum_even);
    
    return (checksum == stored);
}

/**
 * @brief Calculate checksum for encoding
 * 
 * Used when writing Amiga sectors - calculates the checksum
 * that should be stored.
 * 
 * @param odd_data Array of odd MFM words
 * @param even_data Array of even MFM words
 * @param count Number of 32-bit words
 * @param checksum_odd Output: checksum odd bits
 * @param checksum_even Output: checksum even bits
 */
void amiga_calc_checksum_for_write(const uint32_t* odd_data,
                                    const uint32_t* even_data,
                                    size_t count,
                                    uint32_t* checksum_odd,
                                    uint32_t* checksum_even) {
    uint32_t checksum = amiga_calc_checksum(odd_data, even_data, count);
    
    // Encode checksum back to MFM odd/even format
    // odd bits are the upper bits, even bits are the lower bits
    *checksum_odd = (checksum >> 1) & 0x55555555;
    *checksum_even = checksum & 0x55555555;
}
