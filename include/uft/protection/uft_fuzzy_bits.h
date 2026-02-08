/**
 * @file uft_fuzzy_bits.h
 * @brief Fuzzy bit copy protection detection and analysis
 * 
 * Based on Dungeon Master / Chaos Strikes Back copy protection
 * 
 * Fuzzy bits are created by placing flux reversals at PLL inspection
 * window boundaries, causing random bit values on each read.
 */

#ifndef UFT_FUZZY_BITS_H
#define UFT_FUZZY_BITS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Standard MFM bit cell duration in microseconds */
#define UFT_MFM_BIT_CELL_US         2.0

/** Standard flux spacing values in microseconds */
#define UFT_MFM_FLUX_4US            4.0
#define UFT_MFM_FLUX_6US            6.0
#define UFT_MFM_FLUX_8US            8.0

/** Fuzzy bit timing thresholds */
#define UFT_FUZZY_TIMING_MIN_US     4.3
#define UFT_FUZZY_TIMING_MAX_US     5.7
#define UFT_FUZZY_TIMING_CENTER_US  5.0

/** Dungeon Master specific constants */
#define UFT_DM_FUZZY_TRACK          0
#define UFT_DM_FUZZY_SECTOR         7
#define UFT_DM_SECTOR247_TRACK      0
#define UFT_DM_SECTOR247_NUMBER     247

/** First Byte protection markers */
#define UFT_FB_MARKER_START         "PACE/FB"
#define UFT_FB_MARKER_END           "FB"
#define UFT_FB_SERIAL_PREFIX        "Seri"

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Flux timing measurement
 */
typedef struct uft_flux_timing {
    double timing_us;           /**< Time since last flux in microseconds */
    double position_us;         /**< Absolute position in track */
    bool is_ambiguous;          /**< True if timing is in fuzzy zone */
} uft_flux_timing_t;

/**
 * @brief Fuzzy bit analysis result for a single byte
 */
typedef struct uft_fuzzy_byte {
    uint8_t value_min;          /**< Minimum observed value */
    uint8_t value_max;          /**< Maximum observed value */
    uint8_t read_count;         /**< Number of reads performed */
    uint8_t variation_count;    /**< Number of different values seen */
    bool is_fuzzy;              /**< True if byte shows variation */
} uft_fuzzy_byte_t;

/**
 * @brief Fuzzy sector analysis result
 */
typedef struct uft_fuzzy_sector {
    uint8_t track;              /**< Track number */
    uint8_t sector;             /**< Sector number */
    uint8_t data[512];          /**< Last read data */
    uft_fuzzy_byte_t bytes[512]; /**< Per-byte analysis */
    uint16_t fuzzy_count;       /**< Total fuzzy bytes found */
    bool is_protected;          /**< True if copy protection detected */
    bool crc_valid;             /**< CRC status of sector */
} uft_fuzzy_sector_t;

/**
 * @brief Dungeon Master serial number
 */
typedef struct uft_dm_serial {
    uint8_t bytes[4];           /**< 4-byte serial number */
    uint8_t crc;                /**< CRC-8 checksum */
    bool crc_valid;             /**< True if CRC matches */
} uft_dm_serial_t;

/**
 * @brief Complete copy protection analysis result
 */
typedef struct uft_copy_protection {
    bool has_fuzzy_sector;      /**< Fuzzy bits in sector 7 */
    bool has_sector_247;        /**< Invalid sector number 247 */
    bool has_fb_markers;        /**< First Byte markers present */
    uft_dm_serial_t serial;     /**< Extracted serial number */
    uft_fuzzy_sector_t fuzzy;   /**< Fuzzy sector analysis */
    char protection_type[32];   /**< "First Byte", "FTL", etc. */
} uft_copy_protection_t;

/*============================================================================
 * Flux Timing Analysis Functions
 *============================================================================*/

/**
 * @brief Check if flux timing is in ambiguous zone
 * @param timing_us Timing in microseconds
 * @return true if timing could cause fuzzy bit
 */
static inline bool uft_is_fuzzy_timing(double timing_us) {
    return (timing_us > UFT_FUZZY_TIMING_MIN_US && 
            timing_us < UFT_FUZZY_TIMING_MAX_US);
}

/**
 * @brief Check if flux timing is valid MFM
 * @param timing_us Timing in microseconds
 * @param tolerance_pct Tolerance percentage (default 10%)
 * @return true if timing is valid MFM
 */
bool uft_is_valid_mfm_timing(double timing_us, double tolerance_pct);

/**
 * @brief Detect Dungeon Master fuzzy timing pattern
 * 
 * DM uses gradual timing shift: 4µs→5.5µs then 6µs→4.5µs
 * with compensating pairs always summing to ~10µs
 * 
 * @param timings Array of flux timings
 * @param count Number of timings
 * @return true if DM pattern detected
 */
bool uft_detect_dm_fuzzy_pattern(const uft_flux_timing_t *timings, size_t count);

/*============================================================================
 * Fuzzy Bit Detection Functions
 *============================================================================*/

/**
 * @brief Analyze sector for fuzzy bits
 * 
 * Reads sector multiple times and compares results.
 * Fuzzy bits will show different values on each read.
 * 
 * @param ctx UFT context
 * @param track Track number
 * @param sector Sector number
 * @param read_count Number of reads to perform (minimum 2)
 * @param result Output analysis result
 * @return UFT_ERR_OK on success
 */
int uft_analyze_fuzzy_sector(void *ctx, uint8_t track, uint8_t sector,
                             int read_count, uft_fuzzy_sector_t *result);

/**
 * @brief Quick check for fuzzy bits
 * 
 * Performs two reads and checks for any difference.
 * 
 * @param ctx UFT context
 * @param track Track number
 * @param sector Sector number
 * @return true if fuzzy bits detected
 */
bool uft_has_fuzzy_bits(void *ctx, uint8_t track, uint8_t sector);

/*============================================================================
 * Copy Protection Detection Functions
 *============================================================================*/

/**
 * @brief Detect Dungeon Master / CSB copy protection
 * 
 * Checks for:
 * - Fuzzy bits in track 0, sector 7
 * - Invalid sector number 247 in track 0
 * - First Byte markers
 * - Valid serial number with CRC
 * 
 * @param ctx UFT context
 * @param result Output protection analysis
 * @return UFT_ERR_OK on success
 */
int uft_detect_dm_protection(void *ctx, uft_copy_protection_t *result);

/**
 * @brief Check for invalid sector numbers
 * 
 * Sector numbers $F5-$F7 cannot be written by WD1772 FDC.
 * 
 * @param ctx UFT context
 * @param track Track to scan
 * @param found_sector Output: the invalid sector number found (or 0)
 * @return true if invalid sector number found
 */
bool uft_has_invalid_sector_number(void *ctx, uint8_t track, uint8_t *found_sector);

/**
 * @brief Extract serial number from protection sector
 * 
 * @param sector_data 512-byte sector data
 * @param serial Output serial number structure
 * @return true if serial found and CRC valid
 */
bool uft_extract_dm_serial(const uint8_t *sector_data, uft_dm_serial_t *serial);

/**
 * @brief Calculate serial number CRC
 * 
 * CRC-8 with polynomial 0x01, init 0x2D
 * 
 * @param serial 4-byte serial number
 * @return Calculated CRC value
 */
uint8_t uft_calc_dm_serial_crc(const uint8_t serial[4]);

/*============================================================================
 * Preservation Functions
 *============================================================================*/

/**
 * @brief Create flux-level image of fuzzy sector
 * 
 * Captures raw flux timings for accurate preservation.
 * 
 * @param ctx UFT context
 * @param track Track number
 * @param sector Sector number
 * @param timings Output flux timing array
 * @param max_timings Maximum timings to capture
 * @param timing_count Output: actual timing count
 * @return UFT_ERR_OK on success
 */
int uft_capture_fuzzy_flux(void *ctx, uint8_t track, uint8_t sector,
                           uft_flux_timing_t *timings, size_t max_timings,
                           size_t *timing_count);

/**
 * @brief Write fuzzy sector with ambiguous timing
 * 
 * Creates flux reversals at window boundaries to reproduce
 * fuzzy bit behavior. Requires special hardware support.
 * 
 * @param ctx UFT context (must support flux-level writing)
 * @param track Track number
 * @param sector Sector number
 * @param timings Flux timing pattern to write
 * @param timing_count Number of timings
 * @return UFT_ERR_OK on success, UFT_ERR_NOT_SUPPORTED if hardware cannot write
 */
int uft_write_fuzzy_flux(void *ctx, uint8_t track, uint8_t sector,
                         const uft_flux_timing_t *timings, size_t timing_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FUZZY_BITS_H */
