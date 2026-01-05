/**
 * @file uft_protection.h
 * @brief Copy Protection Detection for UFT
 * 
 * Implements detection algorithms for various floppy disk copy protection
 * schemes, including:
 * - Rob Northen CopyLock (Amiga)
 * - Speedlock variable density (Amiga)
 * - Long tracks
 * - Weak bits / Fuzzy bits
 * - Custom sync marks
 * 
 * Based on analysis of Disk-Utilities by Keir Fraser.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_PROTECTION_H
#define UFT_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Protection Type Enumeration
 *============================================================================*/

typedef enum {
    UFT_PROT_NONE           = 0,
    
    /* Amiga protections */
    UFT_PROT_COPYLOCK       = 0x0100,   /**< Rob Northen CopyLock */
    UFT_PROT_COPYLOCK_OLD   = 0x0101,   /**< Older CopyLock variant */
    UFT_PROT_SPEEDLOCK      = 0x0102,   /**< Speedlock variable density */
    UFT_PROT_LONGTRACK      = 0x0103,   /**< Long track protection */
    UFT_PROT_RNC_PROTECT    = 0x0104,   /**< RNC protection */
    UFT_PROT_SOFTLOCK       = 0x0105,   /**< Softlock */
    
    /* C64 protections */
    UFT_PROT_V_MAX          = 0x0200,   /**< V-MAX! protection */
    UFT_PROT_PIRATESLAYER   = 0x0201,   /**< PirateSlayer */
    UFT_PROT_RAPIDLOK       = 0x0202,   /**< RapidLok */
    UFT_PROT_VORPAL         = 0x0203,   /**< Vorpal protection */
    
    /* Generic protections */
    UFT_PROT_WEAK_BITS      = 0x0300,   /**< Weak/fuzzy bits */
    UFT_PROT_CUSTOM_SYNC    = 0x0301,   /**< Non-standard sync marks */
    UFT_PROT_TIMING_BASED   = 0x0302,   /**< Timing-based protection */
    UFT_PROT_DUPLICATE_SECTOR = 0x0303, /**< Duplicate sector IDs */
    
    UFT_PROT_UNKNOWN        = 0xFFFF    /**< Unknown protection */
} uft_protection_type_t;

/*============================================================================
 * CopyLock Structures
 *============================================================================*/

/** CopyLock signature "Rob Northen Comp" */
#define UFT_COPYLOCK_SIGNATURE "Rob Northen Comp"
#define UFT_COPYLOCK_SIG_LEN   16

/** CopyLock sector containing signature */
#define UFT_COPYLOCK_SIG_SECTOR 6

/** Number of CopyLock sectors */
#define UFT_COPYLOCK_SECTORS   11

/**
 * @brief CopyLock sync markers
 * 
 * Each sector uses a unique sync word:
 * Sector 0: 0x8a91   Sector 6: 0x8914 (slow)
 * Sector 1: 0x8a44   Sector 7: 0x8915
 * Sector 2: 0x8a45   Sector 8: 0x8944
 * Sector 3: 0x8a51   Sector 9: 0x8945
 * Sector 4: 0x8912 (fast)  Sector 10: 0x8951
 * Sector 5: 0x8911
 */
extern const uint16_t uft_copylock_sync_marks[UFT_COPYLOCK_SECTORS];

/**
 * @brief CopyLock detection result
 */
typedef struct {
    uft_protection_type_t type;     /**< COPYLOCK or COPYLOCK_OLD */
    uint32_t lfsr_seed;             /**< 23-bit LFSR seed */
    uint8_t  valid_sectors;         /**< Bitmask of valid sectors */
    uint8_t  sectors_found;         /**< Number of sectors found */
    bool     signature_found;       /**< "Rob Northen Comp" found */
    
    /* Timing analysis */
    int16_t  fast_sector_delta;     /**< Sector 4 timing delta (%) */
    int16_t  slow_sector_delta;     /**< Sector 6 timing delta (%) */
} uft_copylock_info_t;

/*============================================================================
 * Speedlock Structures
 *============================================================================*/

/** Speedlock speed variations */
#define UFT_SPEEDLOCK_NORMAL    100     /**< 100% = normal speed */
#define UFT_SPEEDLOCK_FAST      90      /**< 90% = fast (short bitcells) */
#define UFT_SPEEDLOCK_SLOW      110     /**< 110% = slow (long bitcells) */

/** Speedlock detection threshold */
#define UFT_SPEEDLOCK_THRESHOLD 8       /**< 8% deviation to detect */

/**
 * @brief Speedlock detection result
 */
typedef struct {
    bool     detected;              /**< Speedlock found */
    uint32_t long_region_start;     /**< Bit offset of long bitcells */
    uint32_t short_region_start;    /**< Bit offset of short bitcells */
    uint32_t normal_region_start;   /**< Bit offset of normal bitcells */
    uint16_t sector_length;         /**< Length of each region in bits */
    int16_t  long_delta;            /**< Long region timing delta (%) */
    int16_t  short_delta;           /**< Short region timing delta (%) */
} uft_speedlock_info_t;

/*============================================================================
 * Long Track Structures
 *============================================================================*/

/** Standard track length (MFM DD @ 300 RPM) */
#define UFT_STANDARD_TRACK_BITS     100000

/** Long track threshold (percentage) */
#define UFT_LONGTRACK_THRESHOLD     105     /**< 105% = long track */

/**
 * @brief Long track detection result
 */
typedef struct {
    bool     detected;              /**< Long track found */
    uint32_t track_bits;            /**< Actual track length in bits */
    uint16_t percent;               /**< Percentage of standard length */
    uint32_t extra_bits;            /**< Extra bits beyond standard */
} uft_longtrack_info_t;

/*============================================================================
 * Weak Bits Structures
 *============================================================================*/

/**
 * @brief Weak bit region
 */
typedef struct {
    uint32_t bit_offset;            /**< Start offset in track */
    uint32_t bit_length;            /**< Length in bits */
    uint8_t  variation_percent;     /**< Variation between reads */
} uft_weak_region_t;

/**
 * @brief Weak bits detection result
 */
typedef struct {
    bool     detected;              /**< Weak bits found */
    uint16_t num_regions;           /**< Number of weak regions */
    uft_weak_region_t* regions;     /**< Array of weak regions */
} uft_weakbits_info_t;

/*============================================================================
 * Protection Detection Context
 *============================================================================*/

/**
 * @brief Protection detection context
 */
typedef struct {
    /* Input data */
    const uint8_t* track_data;      /**< Raw track data (MFM/GCR) */
    size_t track_bits;              /**< Track length in bits */
    uint8_t track_number;           /**< Track number */
    uint8_t head;                   /**< Head/side */
    
    /* Multi-revolution data for weak bit detection */
    const uint8_t** revolutions;    /**< Array of revolution data */
    size_t num_revolutions;         /**< Number of revolutions */
    
    /* Timing data (optional) */
    const uint32_t* flux_times;     /**< Flux timing data */
    size_t num_flux;                /**< Number of flux transitions */
    
    /* Detection results */
    uft_protection_type_t primary;  /**< Primary protection detected */
    uint16_t all_protections;       /**< Bitmask of all detected */
    
    uft_copylock_info_t copylock;
    uft_speedlock_info_t speedlock;
    uft_longtrack_info_t longtrack;
    uft_weakbits_info_t weakbits;
} uft_protection_ctx_t;

/*============================================================================
 * LFSR Functions (for CopyLock)
 *============================================================================*/

/**
 * @brief Advance LFSR to next state
 * 
 * 23-bit LFSR with taps at positions 1 and 23.
 * x_new = ((x << 1) & 0x7FFFFF) | ((x >> 22) ^ x) & 1
 * 
 * @param state Current 23-bit state
 * @return Next state
 */
static inline uint32_t uft_lfsr_next(uint32_t state)
{
    return ((state << 1) & 0x7FFFFF) | (((state >> 22) ^ state) & 1);
}

/**
 * @brief Reverse LFSR to previous state
 * @param state Current 23-bit state
 * @return Previous state
 */
static inline uint32_t uft_lfsr_prev(uint32_t state)
{
    return (state >> 1) | ((((state >> 1) ^ state) & 1) << 22);
}

/**
 * @brief Get data byte from LFSR state
 * 
 * The data byte is bits [22:15] of the LFSR state.
 * 
 * @param state 23-bit LFSR state
 * @return Data byte
 */
static inline uint8_t uft_lfsr_byte(uint32_t state)
{
    return (uint8_t)(state >> 15);
}

/**
 * @brief Advance LFSR by N steps
 * @param state Current state
 * @param steps Number of steps (positive = forward)
 * @return New state
 */
uint32_t uft_lfsr_advance(uint32_t state, int steps);

/*============================================================================
 * Protection Detection Functions
 *============================================================================*/

/**
 * @brief Initialize protection detection context
 */
void uft_protection_init(uft_protection_ctx_t* ctx);

/**
 * @brief Free protection detection resources
 */
void uft_protection_free(uft_protection_ctx_t* ctx);

/**
 * @brief Detect CopyLock protection
 * 
 * Scans track for CopyLock characteristics:
 * - Unique sync marks for each sector
 * - LFSR-generated data pattern
 * - "Rob Northen Comp" signature in sector 6
 * - ±5% timing variations
 * 
 * @param ctx Detection context
 * @return true if CopyLock detected
 */
bool uft_detect_copylock(uft_protection_ctx_t* ctx);

/**
 * @brief Detect Speedlock protection
 * 
 * Scans track for variable-density regions:
 * - Long bitcells (+10%)
 * - Short bitcells (-10%)
 * - Normal bitcells (reference)
 * 
 * @param ctx Detection context
 * @return true if Speedlock detected
 */
bool uft_detect_speedlock(uft_protection_ctx_t* ctx);

/**
 * @brief Detect long track protection
 * 
 * Checks if track length exceeds standard by threshold.
 * 
 * @param ctx Detection context
 * @return true if long track detected
 */
bool uft_detect_longtrack(uft_protection_ctx_t* ctx);

/**
 * @brief Detect weak bits
 * 
 * Compares multiple revolutions to find varying bits.
 * Requires ctx->revolutions and ctx->num_revolutions >= 2.
 * 
 * @param ctx Detection context
 * @return true if weak bits detected
 */
bool uft_detect_weakbits(uft_protection_ctx_t* ctx);

/**
 * @brief Detect custom sync marks
 * 
 * Searches for non-standard MFM sync patterns.
 * Standard: 0x4489 (A1 with missing clock)
 * 
 * @param ctx Detection context
 * @return true if custom sync found
 */
bool uft_detect_custom_sync(uft_protection_ctx_t* ctx);

/**
 * @brief Run all protection detection algorithms
 * 
 * @param ctx Detection context
 * @return Primary protection type detected
 */
uft_protection_type_t uft_detect_all_protections(uft_protection_ctx_t* ctx);

/*============================================================================
 * Protection Reconstruction
 *============================================================================*/

/**
 * @brief Reconstruct CopyLock track from LFSR seed
 * 
 * CopyLock tracks can be fully reconstructed from the LFSR seed,
 * allowing recovery of damaged tracks.
 * 
 * @param seed 23-bit LFSR seed
 * @param output Output buffer (must be large enough for full track)
 * @param old_style true for COPYLOCK_OLD variant
 * @return Bytes written
 */
size_t uft_copylock_reconstruct(uint32_t seed, uint8_t* output, bool old_style);

/**
 * @brief Generate Speedlock track
 * 
 * @param normal_data Normal-speed data
 * @param normal_len Length of normal data
 * @param output Output buffer with timing info
 * @return Bytes written
 */
size_t uft_speedlock_generate(const uint8_t* normal_data, size_t normal_len,
                               uint8_t* output);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get protection type name
 * @param type Protection type
 * @return Static string with name
 */
const char* uft_protection_name(uft_protection_type_t type);

/**
 * @brief Print protection detection results
 * @param ctx Detection context
 * @param verbose Include detailed info
 */
void uft_protection_print(const uft_protection_ctx_t* ctx, bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_H */
