/**
 * @file uft_copylock.h
 * @brief Rob Northen CopyLock Protection Handler
 * 
 * Implements detection and analysis of Amiga CopyLock protection.
 * Based on algorithm analysis from Keir Fraser's disk-utilities (GPL).
 * Clean-room reimplementation for UFT.
 * 
 * CopyLock uses:
 * - 23-bit LFSR with taps at positions 1 and 23
 * - 11 distinct sync markers
 * - Variable timing for certain sync words
 * - "Rob Northen Comp" signature in sector 6
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_COPYLOCK_H
#define UFT_COPYLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** CopyLock LFSR parameters */
#define UFT_COPYLOCK_LFSR_BITS      23
#define UFT_COPYLOCK_LFSR_MASK      ((1u << UFT_COPYLOCK_LFSR_BITS) - 1)
#define UFT_COPYLOCK_LFSR_TAP1      0   /**< Tap at bit 0 (position 1) */
#define UFT_COPYLOCK_LFSR_TAP2      22  /**< Tap at bit 22 (position 23) */

/** Number of sync markers in CopyLock */
#define UFT_COPYLOCK_SYNC_COUNT     11

/** Timing variations (percentage of standard bitcell) */
#define UFT_COPYLOCK_TIMING_FAST    95   /**< Sync 0x8912: 5% faster */
#define UFT_COPYLOCK_TIMING_SLOW    105  /**< Sync 0x8914: 5% slower */
#define UFT_COPYLOCK_TIMING_NORMAL  100  /**< Standard timing */

/** Signature location and content */
#define UFT_COPYLOCK_SIG_SECTOR     6
#define UFT_COPYLOCK_SIG_LEN        16
#define UFT_COPYLOCK_SIGNATURE      "Rob Northen Comp"

/**
 * @brief Serial Number Derivation Constants
 * 
 * From original Rob Northen disassembly at $298-$2B8:
 * The serial number is computed by subtracting the first 24 bytes 
 * of sector 6 as big-endian longwords from zero.
 * 
 * Algorithm:
 *   checksum = 0
 *   checksum -= "Rob " (0x526F6220)
 *   checksum -= "Nort" (0x4E6F7274)
 *   checksum -= "hen " (0x68656E20)
 *   checksum -= "Comp" (0x436F6D70)
 *   // At this point checksum == 0xB34C4FDC (verification)
 *   checksum -= longword[4]  // LFSR byte 16-19
 *   checksum -= longword[5]  // LFSR byte 20-23
 *   serial = checksum
 */
#define UFT_COPYLOCK_SIG_CHECKSUM      0xB34C4FDCu
#define UFT_COPYLOCK_SERIAL_BYTES      24  /**< First 24 bytes of sector 6 */

/** Extended signatures (APB, Weird Dreams share seed 0x3E2896) */
#define UFT_COPYLOCK_EXT_SIG_SEED      0x3E2896u
#define UFT_COPYLOCK_EXT_SIG_LEN       8

/** Maximum sectors in CopyLock track */
#define UFT_COPYLOCK_MAX_SECTORS    11

/** Tolerance for timing detection (in nanoseconds) */
#define UFT_COPYLOCK_TIMING_TOLERANCE_NS  200

/*===========================================================================
 * Sync Markers
 *===========================================================================*/

/**
 * @brief CopyLock sync marker table (standard version)
 * 
 * These are the 11 sync words used in standard CopyLock protection.
 */
static const uint16_t uft_copylock_sync_standard[UFT_COPYLOCK_SYNC_COUNT] = {
    0x8A91,  /* Sector 0 */
    0x8A44,  /* Sector 1 */
    0x8A45,  /* Sector 2 */
    0x8A51,  /* Sector 3 */
    0x8912,  /* Sector 4 - FAST timing (95%) */
    0x8911,  /* Sector 5 */
    0x8914,  /* Sector 6 - SLOW timing (105%), contains signature */
    0x8915,  /* Sector 7 */
    0x8944,  /* Sector 8 */
    0x8945,  /* Sector 9 */
    0x8951   /* Sector 10 */
};

/**
 * @brief Old CopyLock sync marker table
 * 
 * Early CopyLock versions used 0x65xx sync patterns.
 */
static const uint16_t uft_copylock_sync_old[UFT_COPYLOCK_SYNC_COUNT] = {
    0x6591,
    0x6544,
    0x6545,
    0x6551,
    0x6412,
    0x6411,
    0x6414,
    0x6415,
    0x6444,
    0x6445,
    0x6451
};

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief CopyLock variant type
 */
typedef enum {
    UFT_COPYLOCK_UNKNOWN = 0,      /**< Not detected */
    UFT_COPYLOCK_STANDARD,         /**< Standard 11-sync version (0x8xxx) */
    UFT_COPYLOCK_OLD,              /**< Old version (0x65xx syncs) */
    UFT_COPYLOCK_OLD_VARIANT,      /**< Old version with different LFSR skip */
    UFT_COPYLOCK_ST                /**< Atari ST variant */
} uft_copylock_variant_t;

/**
 * @brief CopyLock detection confidence level
 */
typedef enum {
    UFT_COPYLOCK_CONF_NONE = 0,    /**< Not CopyLock */
    UFT_COPYLOCK_CONF_POSSIBLE,    /**< Some markers found */
    UFT_COPYLOCK_CONF_LIKELY,      /**< Multiple markers + timing */
    UFT_COPYLOCK_CONF_CERTAIN      /**< Full detection + signature */
} uft_copylock_confidence_t;

/**
 * @brief Sector timing measurement
 */
typedef struct {
    uint16_t sync_word;            /**< Detected sync marker */
    uint32_t bit_offset;           /**< Bit position in track */
    float    timing_ratio;         /**< Actual timing as % of nominal */
    bool     timing_valid;         /**< True if timing matches expected */
    uint8_t  expected_timing;      /**< Expected timing (95/100/105) */
} uft_copylock_sector_timing_t;

/**
 * @brief CopyLock LFSR state
 */
typedef struct {
    uint32_t seed;                 /**< Initial LFSR seed (23-bit) */
    uint32_t current;              /**< Current LFSR state */
    uint32_t iterations;           /**< Number of iterations from seed */
} uft_copylock_lfsr_t;

/**
 * @brief CopyLock detection result
 */
typedef struct {
    /* Detection status */
    bool                       detected;
    uft_copylock_variant_t     variant;
    uft_copylock_confidence_t  confidence;
    
    /* LFSR information */
    uint32_t                   lfsr_seed;      /**< Extracted LFSR seed */
    bool                       seed_valid;     /**< True if seed verified */
    
    /* Sync analysis */
    uint8_t                    syncs_found;    /**< Number of syncs detected */
    uint16_t                   sync_list[UFT_COPYLOCK_MAX_SECTORS];
    
    /* Timing analysis */
    uft_copylock_sector_timing_t timings[UFT_COPYLOCK_MAX_SECTORS];
    uint8_t                    timing_matches; /**< Sectors with correct timing */
    
    /* Signature */
    bool                       signature_found;
    uint8_t                    signature[UFT_COPYLOCK_SIG_LEN];
    
    /* Track info */
    uint8_t                    track;
    uint8_t                    head;
    uint32_t                   track_bits;     /**< Total bits in track */
    
    /* Diagnostics */
    char                       info[256];      /**< Human-readable info */
} uft_copylock_result_t;

/**
 * @brief CopyLock reconstruction parameters
 */
typedef struct {
    uint32_t                   lfsr_seed;
    uft_copylock_variant_t     variant;
    uint8_t                    track;
    uint8_t                    head;
    bool                       include_timing; /**< Include timing variations */
} uft_copylock_recon_params_t;

/*===========================================================================
 * LFSR Functions
 *===========================================================================*/

/**
 * @brief Advance LFSR to next state
 * 
 * Implements 23-bit LFSR with taps at positions 1 and 23:
 * new_bit = bit[22] XOR bit[0]
 * 
 * @param state Current LFSR state (23-bit)
 * @return Next LFSR state
 */
static inline uint32_t uft_copylock_lfsr_next(uint32_t state) {
    uint32_t new_bit = ((state >> UFT_COPYLOCK_LFSR_TAP2) ^ 
                        (state >> UFT_COPYLOCK_LFSR_TAP1)) & 1;
    return ((state << 1) & UFT_COPYLOCK_LFSR_MASK) | new_bit;
}

/**
 * @brief Reverse LFSR to previous state
 * 
 * @param state Current LFSR state (23-bit)
 * @return Previous LFSR state
 */
static inline uint32_t uft_copylock_lfsr_prev(uint32_t state) {
    uint32_t old_bit = (((state >> 1) ^ state) & 1);
    return (state >> 1) | (old_bit << UFT_COPYLOCK_LFSR_TAP2);
}

/**
 * @brief Get output byte from LFSR state
 * 
 * CopyLock uses bits 22-15 as output byte.
 * 
 * @param state LFSR state
 * @return Output byte
 */
static inline uint8_t uft_copylock_lfsr_byte(uint32_t state) {
    return (uint8_t)(state >> 15);
}

/**
 * @brief Initialize LFSR context
 * 
 * @param lfsr LFSR context to initialize
 * @param seed Initial seed value
 */
void uft_copylock_lfsr_init(uft_copylock_lfsr_t *lfsr, uint32_t seed);

/**
 * @brief Advance LFSR by N steps
 * 
 * @param lfsr LFSR context
 * @param steps Number of steps to advance
 * @return Output byte after advancement
 */
uint8_t uft_copylock_lfsr_advance(uft_copylock_lfsr_t *lfsr, uint32_t steps);

/**
 * @brief Generate byte sequence from LFSR
 * 
 * @param lfsr LFSR context
 * @param output Output buffer
 * @param length Number of bytes to generate
 */
void uft_copylock_lfsr_generate(uft_copylock_lfsr_t *lfsr,
                                 uint8_t *output, size_t length);

/**
 * @brief Try to recover seed from partial data
 * 
 * Given a sequence of output bytes, attempts to recover the LFSR seed.
 * 
 * @param data Output byte sequence
 * @param length Length of sequence
 * @param seed Output: recovered seed
 * @return true if seed recovered successfully
 */
bool uft_copylock_lfsr_recover_seed(const uint8_t *data, size_t length,
                                     uint32_t *seed);

/*===========================================================================
 * Serial Number Extraction
 *===========================================================================*/

/**
 * @brief Extended signature info for certain titles
 */
typedef struct {
    const char *title;           /**< Game title */
    uint8_t     sig_bytes[8];    /**< Extended signature after "Rob Northen Comp" */
} uft_copylock_ext_sig_info_t;

/**
 * @brief Known extended signatures
 * 
 * APB and Weird Dreams share LFSR seed 0x3E2896 but have unique extended
 * signatures in the 8 bytes following "Rob Northen Comp".
 */
static const uft_copylock_ext_sig_info_t uft_copylock_ext_signatures[] = {
    { "APB",          { 0x54, 0xE1, 0xED, 0x5B, 0x64, 0x85, 0x22, 0x7D } },
    { "Weird Dreams", { 0x78, 0x26, 0x46, 0xF4, 0xD5, 0x24, 0xA0, 0x03 } },
};

#define UFT_COPYLOCK_EXT_SIG_COUNT \
    (sizeof(uft_copylock_ext_signatures) / sizeof(uft_copylock_ext_signatures[0]))

/**
 * @brief Serial number extraction result
 */
typedef struct {
    bool     signature_valid;    /**< "Rob Northen Comp" found and verified */
    bool     serial_valid;       /**< Serial number successfully extracted */
    uint32_t serial_number;      /**< Extracted 32-bit serial number */
    uint32_t sig_checksum;       /**< Intermediate checksum (should be 0xB34C4FDC) */
    int      ext_sig_index;      /**< Extended signature index (-1 if none) */
    const char *ext_sig_title;   /**< Title if extended signature matched */
} uft_copylock_serial_t;

/**
 * @brief Extract serial number from sector 6 data
 * 
 * Implements the original Rob Northen key derivation algorithm:
 * 
 * From disassembly at $298-$2B8:
 * @code
 * moveq.l #$0,d0              ; d0 = 0
 * move.w  #$3,d1              ; loop 4 times
 * movea.l a3,a0               ; buffer = sector 6 data
 * sub.l   (a0)+,d0            ; d0 -= longword
 * dbra    d1,loop1            ; subtract 16 bytes ("Rob Northen Comp")
 * cmp.l   #$b34c4fdc,d0       ; verify signature checksum
 * bne.s   fail
 * move.w  #$1,d1              ; loop 2 more times  
 * sub.l   (a0)+,d0            ; d0 -= longword
 * dbra    d1,loop2            ; subtract 8 more bytes (LFSR data)
 * move.l  d0,d6               ; d6 = serial number
 * @endcode
 * 
 * @param sector6_data First 24+ bytes of sector 6
 * @param data_len Length of data (must be >= 24)
 * @param result Output: extraction result
 * @return true if serial extracted successfully
 */
static inline bool uft_copylock_extract_serial(
    const uint8_t *sector6_data,
    size_t data_len,
    uft_copylock_serial_t *result)
{
    uint32_t checksum = 0;
    size_t i;
    
    if (data_len < UFT_COPYLOCK_SERIAL_BYTES || !result) {
        return false;
    }
    
    /* Initialize result */
    result->signature_valid = false;
    result->serial_valid = false;
    result->serial_number = 0;
    result->sig_checksum = 0;
    result->ext_sig_index = -1;
    result->ext_sig_title = NULL;
    
    /* Verify "Rob Northen Comp" signature */
    if (memcmp(sector6_data, UFT_COPYLOCK_SIGNATURE, UFT_COPYLOCK_SIG_LEN) != 0) {
        return false;
    }
    
    /* Calculate signature checksum: 0 - first 4 longwords (big-endian) */
    for (i = 0; i < 4; i++) {
        uint32_t lw = ((uint32_t)sector6_data[i*4+0] << 24) |
                      ((uint32_t)sector6_data[i*4+1] << 16) |
                      ((uint32_t)sector6_data[i*4+2] << 8)  |
                      ((uint32_t)sector6_data[i*4+3]);
        checksum -= lw;
    }
    
    result->sig_checksum = checksum;
    
    /* Verify signature checksum matches expected value */
    if (checksum != UFT_COPYLOCK_SIG_CHECKSUM) {
        /* Signature text found but checksum wrong - corrupted disk? */
        return false;
    }
    
    result->signature_valid = true;
    
    /* Check for extended signature (APB, Weird Dreams) */
    if (data_len >= UFT_COPYLOCK_SIG_LEN + UFT_COPYLOCK_EXT_SIG_LEN) {
        for (i = 0; i < UFT_COPYLOCK_EXT_SIG_COUNT; i++) {
            if (memcmp(&sector6_data[UFT_COPYLOCK_SIG_LEN],
                       uft_copylock_ext_signatures[i].sig_bytes,
                       UFT_COPYLOCK_EXT_SIG_LEN) == 0) {
                result->ext_sig_index = (int)i;
                result->ext_sig_title = uft_copylock_ext_signatures[i].title;
                break;
            }
        }
    }
    
    /* Continue with LFSR-derived bytes to compute serial */
    for (i = 4; i < 6; i++) {
        uint32_t lw = ((uint32_t)sector6_data[i*4+0] << 24) |
                      ((uint32_t)sector6_data[i*4+1] << 16) |
                      ((uint32_t)sector6_data[i*4+2] << 8)  |
                      ((uint32_t)sector6_data[i*4+3]);
        checksum -= lw;
    }
    
    result->serial_number = checksum;
    result->serial_valid = true;
    
    return true;
}

/**
 * @brief Verify serial number matches expected value
 * 
 * @param sector6_data Sector 6 data
 * @param data_len Length of data
 * @param expected_serial Expected serial number
 * @return true if serial matches
 */
static inline bool uft_copylock_verify_serial(
    const uint8_t *sector6_data,
    size_t data_len,
    uint32_t expected_serial)
{
    uft_copylock_serial_t result;
    if (!uft_copylock_extract_serial(sector6_data, data_len, &result)) {
        return false;
    }
    return (result.serial_number == expected_serial);
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect CopyLock protection on track
 * 
 * Analyzes MFM track data for CopyLock protection markers:
 * - Searches for characteristic sync patterns
 * - Measures timing variations
 * - Looks for "Rob Northen" signature
 * - Extracts LFSR seed if possible
 * 
 * @param track_data Raw MFM track data
 * @param track_bits Number of bits in track
 * @param timing_data Per-bit timing (NULL if unavailable)
 * @param track Track number
 * @param head Head number
 * @param result Output: detection result
 * @return UFT_OK on success
 */
int uft_copylock_detect(const uint8_t *track_data,
                        uint32_t track_bits,
                        const uint16_t *timing_data,
                        uint8_t track,
                        uint8_t head,
                        uft_copylock_result_t *result);

/**
 * @brief Quick check for CopyLock sync markers
 * 
 * Fast check without full analysis. Use for screening.
 * 
 * @param track_data Raw MFM track data
 * @param track_bits Number of bits in track
 * @return Number of CopyLock sync markers found
 */
int uft_copylock_quick_check(const uint8_t *track_data, uint32_t track_bits);

/**
 * @brief Check if sync word is CopyLock marker
 * 
 * @param sync 16-bit sync word
 * @param variant Output: detected variant (if not NULL)
 * @return true if this is a CopyLock sync
 */
bool uft_copylock_is_sync(uint16_t sync, uft_copylock_variant_t *variant);

/**
 * @brief Get expected timing for sync word
 * 
 * @param sync Sync word
 * @return Expected timing as percentage (95, 100, or 105)
 */
uint8_t uft_copylock_expected_timing(uint16_t sync);

/*===========================================================================
 * Extraction Functions
 *===========================================================================*/

/**
 * @brief Extract LFSR seed from CopyLock track
 * 
 * Extracts the seed value that can be used to fully reconstruct
 * the CopyLock track data. This is the key to preservation.
 * 
 * @param track_data Raw MFM track data
 * @param track_bits Number of bits in track
 * @param variant Detected variant
 * @param seed Output: extracted seed
 * @return UFT_OK if seed extracted, error code otherwise
 */
int uft_copylock_extract_seed(const uint8_t *track_data,
                               uint32_t track_bits,
                               uft_copylock_variant_t variant,
                               uint32_t *seed);

/**
 * @brief Verify extracted seed by reconstruction
 * 
 * @param seed Seed to verify
 * @param variant CopyLock variant
 * @param track_data Original track data
 * @param track_bits Track length
 * @return true if reconstruction matches original
 */
bool uft_copylock_verify_seed(uint32_t seed,
                               uft_copylock_variant_t variant,
                               const uint8_t *track_data,
                               uint32_t track_bits);

/*===========================================================================
 * Reconstruction Functions
 *===========================================================================*/

/**
 * @brief Reconstruct CopyLock track from seed
 * 
 * Given the LFSR seed, reconstructs the complete CopyLock track
 * including all sector data and timing variations.
 * 
 * @param params Reconstruction parameters
 * @param output Output buffer (must be large enough)
 * @param output_bits Output: actual bits written
 * @param timing_out Output: timing data (NULL to skip)
 * @return UFT_OK on success
 */
int uft_copylock_reconstruct(const uft_copylock_recon_params_t *params,
                              uint8_t *output,
                              uint32_t *output_bits,
                              uint16_t *timing_out);

/**
 * @brief Get required buffer size for reconstruction
 * 
 * @param variant CopyLock variant
 * @return Required buffer size in bytes
 */
size_t uft_copylock_recon_buffer_size(uft_copylock_variant_t variant);

/*===========================================================================
 * Analysis & Reporting
 *===========================================================================*/

/**
 * @brief Generate detailed CopyLock analysis report
 * 
 * @param result Detection result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written (excluding null terminator)
 */
size_t uft_copylock_report(const uft_copylock_result_t *result,
                            char *buffer, size_t buffer_size);

/**
 * @brief Export CopyLock analysis to JSON
 * 
 * @param result Detection result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_copylock_export_json(const uft_copylock_result_t *result,
                                 char *buffer, size_t buffer_size);

/**
 * @brief Get variant name as string
 * 
 * @param variant Variant type
 * @return String name
 */
const char* uft_copylock_variant_name(uft_copylock_variant_t variant);

/**
 * @brief Get confidence level as string
 * 
 * @param conf Confidence level
 * @return String description
 */
const char* uft_copylock_confidence_name(uft_copylock_confidence_t conf);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Find sync word in MFM bitstream
 * 
 * @param data MFM data
 * @param bits Total bits
 * @param sync Sync word to find
 * @param start_bit Starting bit position
 * @return Bit position of sync, or -1 if not found
 */
int32_t uft_copylock_find_sync(const uint8_t *data, uint32_t bits,
                                uint16_t sync, uint32_t start_bit);

/**
 * @brief Decode MFM sector after sync
 * 
 * @param data MFM data starting after sync
 * @param output Decoded output buffer
 * @param output_len Maximum output length
 * @return Bytes decoded
 */
size_t uft_copylock_decode_sector(const uint8_t *data,
                                   uint8_t *output, size_t output_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_COPYLOCK_H */
