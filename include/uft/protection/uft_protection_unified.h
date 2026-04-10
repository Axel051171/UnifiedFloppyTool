/**
 * @file uft_protection_unified.h
 * @brief Unified Cross-Platform Copy Protection Detection API
 * @version 1.0.0
 *
 * Single entry point for detecting copy protection across ALL platforms:
 *   C64, Amiga, Atari ST, Apple II, Atari 8-bit, IBM PC,
 *   ZX Spectrum, Amstrad CPC, BBC Micro, MSX
 *
 * Dispatches to platform-specific detectors and normalises results
 * into a common structure with confidence scoring.
 */

#ifndef UFT_PROTECTION_UNIFIED_H
#define UFT_PROTECTION_UNIFIED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_PROT_UNI_MAX_HITS       64
#define UFT_PROT_UNI_MAX_NAME       64
#define UFT_PROT_UNI_MAX_DETAIL     256
#define UFT_PROT_UNI_MAX_PLATFORM   32

/*===========================================================================
 * Platform Enumeration
 *===========================================================================*/

typedef enum {
    UFT_UNI_PLATFORM_UNKNOWN = 0,
    UFT_UNI_PLATFORM_C64,
    UFT_UNI_PLATFORM_AMIGA,
    UFT_UNI_PLATFORM_ATARI_ST,
    UFT_UNI_PLATFORM_APPLE2,
    UFT_UNI_PLATFORM_ATARI_8BIT,
    UFT_UNI_PLATFORM_PC,
    UFT_UNI_PLATFORM_SPECTRUM,
    UFT_UNI_PLATFORM_CPC,
    UFT_UNI_PLATFORM_BBC,
    UFT_UNI_PLATFORM_MSX,
    UFT_UNI_PLATFORM_COUNT
} uft_uni_platform_t;

/*===========================================================================
 * Protection Technique Flags
 *===========================================================================*/

#define UFT_UNI_TECH_WEAK_BITS       0x0001
#define UFT_UNI_TECH_TIMING          0x0002
#define UFT_UNI_TECH_LONG_TRACK      0x0004
#define UFT_UNI_TECH_HALF_TRACK      0x0008
#define UFT_UNI_TECH_CUSTOM_SYNC     0x0010
#define UFT_UNI_TECH_DENSITY         0x0020
#define UFT_UNI_TECH_EXTRA_SECTORS   0x0040
#define UFT_UNI_TECH_BAD_SECTORS     0x0080
#define UFT_UNI_TECH_LFSR            0x0100
#define UFT_UNI_TECH_ENCRYPTED       0x0200
#define UFT_UNI_TECH_CUSTOM_FORMAT   0x0400
#define UFT_UNI_TECH_FAT_TRACK       0x0800

/*===========================================================================
 * Unified Protection Hit
 *===========================================================================*/

/**
 * @brief Single protection detection result -- unified across all platforms
 */
typedef struct {
    char     platform[UFT_PROT_UNI_MAX_PLATFORM]; /**< "C64", "Amiga", etc. */
    char     scheme_name[UFT_PROT_UNI_MAX_NAME];  /**< "V-MAX!", "CopyLock" */
    char     variant[UFT_PROT_UNI_MAX_NAME];      /**< "v2b", "Old Style" */
    char     details[UFT_PROT_UNI_MAX_DETAIL];    /**< Human-readable detail */
    float    confidence;                            /**< 0.0 - 1.0 */
    uint8_t  track;                                 /**< Track where found */
    uint8_t  head;                                  /**< Head/side */
    uint16_t technique_flags;                       /**< UFT_UNI_TECH_* bitmask */
    bool     preservable_in_flux;                   /**< SCP/HFE can preserve */
    bool     preservable_in_native;                 /**< D64/ADF/ST can preserve */
} uft_protection_hit_unified_t;

/*===========================================================================
 * Unified Detection Result
 *===========================================================================*/

typedef struct {
    uft_uni_platform_t    detected_platform;
    int                   hit_count;
    uft_protection_hit_unified_t hits[UFT_PROT_UNI_MAX_HITS];

    /* Summary */
    bool                  is_protected;
    float                 overall_confidence;        /**< Highest confidence */
    char                  primary_scheme[UFT_PROT_UNI_MAX_NAME];
    char                  summary[UFT_PROT_UNI_MAX_DETAIL];
} uft_protection_unified_result_t;

/*===========================================================================
 * Track Data Accessor Callback
 *===========================================================================*/

/**
 * @brief Callback to provide track data for analysis
 *
 * @param track   Track number
 * @param head    Head/side
 * @param size    [out] Size of returned data in bytes
 * @param user    User context pointer
 * @return Pointer to track data (owned by caller), or NULL
 */
typedef const uint8_t *(*uft_uni_track_accessor_t)(
    uint8_t track, uint8_t head, size_t *size, void *user);

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Detect all protections on a disk image file
 *
 * Opens the image, determines the platform, and runs all applicable
 * platform-specific detectors.
 *
 * @param image_path  Path to disk image
 * @param hits        Output array for detection hits
 * @param max_hits    Capacity of hits array
 * @param hit_count   [out] Number of hits written
 * @return 0 on success, negative on error
 */
int uft_protection_detect_all(const char *image_path,
                               uft_protection_hit_unified_t *hits,
                               int max_hits,
                               int *hit_count);

/**
 * @brief Detect protections from raw track data via callback
 *
 * @param accessor    Callback to provide track data
 * @param user        User context for callback
 * @param platform    Platform hint (UFT_UNI_PLATFORM_UNKNOWN = auto)
 * @param track_count Number of tracks to scan
 * @param result      Output result structure
 * @return 0 on success
 */
int uft_protection_detect_tracks(uft_uni_track_accessor_t accessor,
                                  void *user,
                                  uft_uni_platform_t platform,
                                  int track_count,
                                  uft_protection_unified_result_t *result);

/**
 * @brief Get platform name string
 */
const char *uft_uni_platform_name(uft_uni_platform_t platform);

/**
 * @brief Generate human-readable report from unified result
 *
 * @param result      Analysis result
 * @param buffer      Output buffer
 * @param buffer_size Buffer capacity
 * @return Bytes written (excluding NUL)
 */
size_t uft_protection_unified_report(
    const uft_protection_unified_result_t *result,
    char *buffer, size_t buffer_size);

/**
 * @brief Export unified result to JSON
 */
size_t uft_protection_unified_to_json(
    const uft_protection_unified_result_t *result,
    char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_UNIFIED_H */
