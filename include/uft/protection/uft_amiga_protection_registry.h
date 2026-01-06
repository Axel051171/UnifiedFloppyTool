/**
 * @file uft_amiga_protection_registry.h
 * @brief Amiga Copy Protection Detection Registry
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Source: disk-utilities (Keir Fraser)
 * License: Public Domain
 *
 * This module provides detection signatures for 170+ Amiga copy protections.
 * Based on Keir Fraser's disk-utilities libdisk format handlers.
 *
 * Integration: R50 - disk-utilities extraction
 */

#ifndef UFT_AMIGA_PROTECTION_REGISTRY_H
#define UFT_AMIGA_PROTECTION_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Protection Type Enumeration
 *============================================================================*/

typedef enum {
    UFT_AMIGA_PROT_NONE             = 0,
    
    /* Major Protection Systems */
    UFT_AMIGA_PROT_COPYLOCK         = 1,    /**< Rob Northen CopyLock */
    UFT_AMIGA_PROT_COPYLOCK_OLD     = 2,    /**< Old-style CopyLock */
    UFT_AMIGA_PROT_SPEEDLOCK        = 3,    /**< SpeedLock */
    UFT_AMIGA_PROT_GREMLIN          = 4,    /**< Gremlin Longtrack */
    
    /* RNC Protections */
    UFT_AMIGA_PROT_RNC_DUALFORMAT   = 10,   /**< RNC Dualformat */
    UFT_AMIGA_PROT_RNC_TRIFORMAT    = 11,   /**< RNC Triformat */
    UFT_AMIGA_PROT_RNC_GAP          = 12,   /**< RNC Gap Protection */
    UFT_AMIGA_PROT_RNC_PROTECT      = 13,   /**< RNC Protect Process */
    
    /* Publisher-Specific */
    UFT_AMIGA_PROT_PSYGNOSIS_A      = 20,   /**< Psygnosis Type A */
    UFT_AMIGA_PROT_PSYGNOSIS_B      = 21,   /**< Psygnosis Type B */
    UFT_AMIGA_PROT_PSYGNOSIS_C      = 22,   /**< Psygnosis Type C */
    UFT_AMIGA_PROT_THALION          = 23,   /**< Thalion */
    UFT_AMIGA_PROT_FACTOR5          = 24,   /**< Factor 5 */
    UFT_AMIGA_PROT_UBI              = 25,   /**< Ubi Soft Protection */
    UFT_AMIGA_PROT_RAINBOW_ARTS     = 26,   /**< Rainbow Arts */
    UFT_AMIGA_PROT_MILLENNIUM       = 27,   /**< Millennium */
    UFT_AMIGA_PROT_FIREBIRD         = 28,   /**< Firebird */
    UFT_AMIGA_PROT_MICROPROSE       = 29,   /**< MicroProse */
    
    /* Format-Based */
    UFT_AMIGA_PROT_LONGTRACK        = 40,   /**< Long Track (>6300 bytes) */
    UFT_AMIGA_PROT_SHORTTRACK       = 41,   /**< Short Track (<6200 bytes) */
    UFT_AMIGA_PROT_VARIABLE_TIMING  = 42,   /**< Variable Bit Timing */
    UFT_AMIGA_PROT_EXTRA_SECTORS    = 43,   /**< 12+ Sectors/Track */
    UFT_AMIGA_PROT_WEAK_BITS        = 44,   /**< Weak/Flakey Bits */
    UFT_AMIGA_PROT_DUPLICATE_SYNC   = 45,   /**< Duplicate Sync Marks */
    
    /* Game-Specific (Selection) */
    UFT_AMIGA_PROT_DUNGEON_MASTER   = 100,
    UFT_AMIGA_PROT_ELITE            = 101,
    UFT_AMIGA_PROT_SHADOW_BEAST     = 102,
    UFT_AMIGA_PROT_XENON2           = 103,
    UFT_AMIGA_PROT_SUPAPLEX         = 104,
    UFT_AMIGA_PROT_PINBALL_DREAMS   = 105,
    UFT_AMIGA_PROT_STARDUST         = 106,
    UFT_AMIGA_PROT_ALIEN_BREED      = 107,
    UFT_AMIGA_PROT_SENSIBLE         = 108,
    UFT_AMIGA_PROT_DISPOSABLE_HERO  = 109,
    
    UFT_AMIGA_PROT_COUNT            = 200   /**< Total known protections */
} uft_amiga_protection_t;

/*============================================================================
 * CopyLock LFSR Definitions
 *============================================================================*/

/**
 * @brief CopyLock LFSR state structure
 */
typedef struct {
    uint32_t seed;              /**< 23-bit LFSR seed */
    uint8_t  sec6_skips_sig;    /**< LFSR skips signature in sector 6 */
    uint8_t  ext_sig_id;        /**< Extended signature ID */
} uft_copylock_lfsr_t;

/**
 * @brief CopyLock sync word list (11 sectors)
 */
static const uint16_t uft_copylock_sync_list[11] = {
    0x8a91, 0x8a44, 0x8a45, 0x8a51, 0x8912, 0x8911,
    0x8914, 0x8915, 0x8944, 0x8945, 0x8951
};

/**
 * @brief "Rob Northen Comp" signature (sector 6)
 */
static const uint8_t uft_copylock_signature[16] = {
    0x52, 0x6f, 0x62, 0x20, 0x4e, 0x6f, 0x72, 0x74,
    0x68, 0x65, 0x6e, 0x20, 0x43, 0x6f, 0x6d, 0x70
};

/*============================================================================
 * Protection Detection Result
 *============================================================================*/

/**
 * @brief Protection detection result
 */
typedef struct {
    uft_amiga_protection_t type;    /**< Detected protection type */
    uint8_t  confidence;            /**< Confidence 0-100% */
    uint8_t  track;                 /**< Key track (if applicable) */
    uint8_t  flags;                 /**< Detection flags */
    uint32_t signature;             /**< Protection-specific signature */
    char     name[64];              /**< Human-readable name */
    char     publisher[32];         /**< Publisher if known */
} uft_amiga_protection_result_t;

/** Detection flags */
#define UFT_AMIGA_PROT_FLAG_LONGTRACK       (1 << 0)
#define UFT_AMIGA_PROT_FLAG_TIMING          (1 << 1)
#define UFT_AMIGA_PROT_FLAG_WEAK_BITS       (1 << 2)
#define UFT_AMIGA_PROT_FLAG_MULTI_TRACK     (1 << 3)

/*============================================================================
 * Track Signature
 *============================================================================*/

/**
 * @brief Track signature for protection detection
 */
typedef struct {
    uint8_t  track_num;             /**< Track number */
    uint8_t  side;                  /**< Side (0 or 1) */
    uint16_t sync_count;            /**< Number of sync marks */
    uint32_t sync_words[16];        /**< Detected sync words */
    uint32_t track_length;          /**< Track length in bits */
    uint32_t min_gap;               /**< Minimum gap length */
    uint32_t max_gap;               /**< Maximum gap length */
    uint8_t  sector_count;          /**< Detected sectors */
    bool     has_timing_variation;  /**< Variable bit timing detected */
    bool     has_weak_bits;         /**< Weak bits detected */
} uft_amiga_track_sig_t;

/*============================================================================
 * Protection Registry Entry
 *============================================================================*/

/**
 * @brief Registry entry for a known protection
 */
typedef struct {
    uft_amiga_protection_t type;
    const char* name;
    const char* publisher;
    uint8_t  key_track;             /**< Primary protection track */
    uint8_t  key_side;              /**< Protection track side */
    uint32_t sync_pattern;          /**< Expected sync pattern */
    uint32_t track_len_min;         /**< Minimum track length (bits) */
    uint32_t track_len_max;         /**< Maximum track length (bits) */
    uint8_t  sector_count;          /**< Expected sector count */
    uint8_t  flags;                 /**< Required flags */
} uft_amiga_protection_entry_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Detect protection from disk image
 * @param tracks Array of track signatures (160 tracks max)
 * @param num_tracks Number of tracks
 * @param results Output array for detected protections
 * @param max_results Maximum results to return
 * @return Number of protections detected
 */
int uft_amiga_detect_protection(const uft_amiga_track_sig_t* tracks,
                                 size_t num_tracks,
                                 uft_amiga_protection_result_t* results,
                                 size_t max_results);

/**
 * @brief Check for CopyLock protection
 * @param tracks Track signatures
 * @param num_tracks Number of tracks
 * @param lfsr Output LFSR info if detected
 * @return true if CopyLock detected
 */
bool uft_amiga_check_copylock(const uft_amiga_track_sig_t* tracks,
                               size_t num_tracks,
                               uft_copylock_lfsr_t* lfsr);

/**
 * @brief Check for long track protection
 * @param track Track signature
 * @return true if long track (>6300 bytes)
 */
bool uft_amiga_is_longtrack(const uft_amiga_track_sig_t* track);

/**
 * @brief Get protection name
 * @param type Protection type
 * @return Human-readable name
 */
const char* uft_amiga_protection_name(uft_amiga_protection_t type);

/**
 * @brief Get protection registry
 * @param count Output: number of entries
 * @return Pointer to registry array
 */
const uft_amiga_protection_entry_t* uft_amiga_get_registry(size_t* count);

/*============================================================================
 * CopyLock LFSR Functions
 *============================================================================*/

/**
 * @brief LFSR next state
 * @param x Current state (23 bits)
 * @return Next state
 */
static inline uint32_t uft_copylock_lfsr_next(uint32_t x)
{
    return ((x << 1) & 0x7FFFFF) | (((x >> 22) ^ x) & 1);
}

/**
 * @brief LFSR previous state
 * @param x Current state (23 bits)
 * @return Previous state
 */
static inline uint32_t uft_copylock_lfsr_prev(uint32_t x)
{
    return (x >> 1) | ((((x >> 1) ^ x) & 1) << 22);
}

/**
 * @brief Extract byte from LFSR state
 * @param x LFSR state
 * @return Extracted byte (bits 22:15)
 */
static inline uint8_t uft_copylock_lfsr_byte(uint32_t x)
{
    return (uint8_t)(x >> 15);
}

/**
 * @brief Advance LFSR by delta steps
 * @param x Current state
 * @param delta Steps to advance
 * @return New state
 */
uint32_t uft_copylock_lfsr_forward(uint32_t x, unsigned int delta);

/**
 * @brief Rewind LFSR by delta steps
 * @param x Current state
 * @param delta Steps to rewind
 * @return New state
 */
uint32_t uft_copylock_lfsr_backward(uint32_t x, unsigned int delta);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_PROTECTION_REGISTRY_H */
