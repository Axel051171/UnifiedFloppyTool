/**
 * @file uft_protection_detect.h
 * @brief Copy Protection Detection System
 * 
 * Comprehensive copy protection detection for multiple platforms:
 * - C64: V-MAX, PirateSlayer, RapidLok
 * - Amiga: CopyLock (196 variants), Speedlock, Psygnosis, Factor5
 * - PC: Various custom schemes
 * 
 * Sources:
 * - UFT GUI parameter_registry.json (Amiga protections)
 * 
 * @version 1.0.0
 */

#ifndef UFT_PROTECTION_DETECT_H
#define UFT_PROTECTION_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*============================================================================
 * Protection Types
 *============================================================================*/

/**
 * @brief Protection scheme family
 */
typedef enum {
    /* No protection */
    UFT_PROT_NONE = 0,
    
    /* C64 Protections */
    UFT_PROT_VMAX = 0x0100,
    UFT_PROT_VMAX_CW,           /**< Cinemaware V-MAX variant */
    UFT_PROT_PIRATESLAYER,
    UFT_PROT_PIRATESLAYER_V2,
    UFT_PROT_RAPIDLOK,
    UFT_PROT_RAPIDLOK_V2,
    UFT_PROT_FAT_TRACK,
    UFT_PROT_CUSTOM_GCR,
    
    /* Amiga Protections */
    UFT_PROT_COPYLOCK = 0x0200,
    UFT_PROT_COPYLOCK_OLD,
    UFT_PROT_RNC_PDOS,
    UFT_PROT_RNC_PDOS_OLD,
    UFT_PROT_RNC_GAP,
    UFT_PROT_RNC_HIDDEN,
    UFT_PROT_SPEEDLOCK,
    UFT_PROT_PSYGNOSIS_A,
    UFT_PROT_PSYGNOSIS_B,
    UFT_PROT_PSYGNOSIS_C,
    UFT_PROT_SHADOW_BEAST,
    UFT_PROT_LEMMINGS,
    UFT_PROT_FACTOR5,
    UFT_PROT_TURRICAN,
    UFT_PROT_RAINBOW_ARTS,
    UFT_PROT_BLUE_BYTE,
    UFT_PROT_CORE_DESIGN,
    UFT_PROT_SENSIBLE,
    UFT_PROT_LONG_TRACK,
    
    /* PC/Atari Protections */
    UFT_PROT_WEAK_BITS = 0x0300,
    UFT_PROT_FUZZY_BITS,
    UFT_PROT_EXTRA_SECTORS,
    UFT_PROT_MISSING_SECTORS,
    UFT_PROT_BAD_CRC,
    
    /* Unknown/Custom */
    UFT_PROT_UNKNOWN = 0xFFFF
} uft_protection_type_t;

/**
 * @brief Protection detection result
 */
typedef struct {
    uft_protection_type_t type;     /**< Detected protection type */
    const char *name;               /**< Human-readable name */
    const char *family;             /**< Protection family name */
    int confidence;                 /**< Detection confidence (0-100) */
    
    /* Track information */
    int track;                      /**< Track where detected */
    int side;                       /**< Side where detected */
    size_t offset;                  /**< Offset in track data */
    
    /* Protection-specific data */
    uint8_t signature[32];          /**< Detected signature bytes */
    size_t signature_len;           /**< Signature length */
    
    /* Alignment info */
    const uint8_t *align_point;     /**< Suggested track alignment */
    
    /* Additional info */
    char notes[256];                /**< Detection notes */
} uft_protection_result_t;

/**
 * @brief Protection detection context
 */
typedef struct {
    /* Results */
    uft_protection_result_t *results;
    size_t result_count;
    size_t result_capacity;
    
    /* Statistics */
    int tracks_scanned;
    int protections_found;
    
    /* Configuration */
    bool detect_c64;
    bool detect_amiga;
    bool detect_pc;
    bool verbose;
} uft_protection_ctx_t;

/*============================================================================
 * C64 Protection Detection
 *============================================================================*/

/**
 * @brief V-MAX duplicator marker bytes
 */
extern const uint8_t UFT_VMAX_MARKERS[5];

/**
 * @brief Cinemaware V-MAX marker
 */
extern const uint8_t UFT_VMAX_CW_MARKER[4];

/**
 * @brief PirateSlayer signature v1
 */
extern const uint8_t UFT_PIRATESLAYER_SIG_V1[5];

/**
 * @brief PirateSlayer signature v2
 */
extern const uint8_t UFT_PIRATESLAYER_SIG_V2[4];

/**
 * @brief Detect V-MAX protection
 * 
 * @param track_data    Track data buffer
 * @param track_len     Track length
 * @param result        Output result (if found)
 * @return Pointer to marker position, or NULL
 */
const uint8_t *uft_prot_detect_vmax(const uint8_t *track_data,
                                    size_t track_len,
                                    uft_protection_result_t *result);

/**
 * @brief Detect V-MAX Cinemaware variant
 */
const uint8_t *uft_prot_detect_vmax_cw(const uint8_t *track_data,
                                       size_t track_len,
                                       uft_protection_result_t *result);

/**
 * @brief Detect PirateSlayer protection
 * 
 * Searches with all 8 bit alignments
 */
const uint8_t *uft_prot_detect_pirateslayer(const uint8_t *track_data,
                                            size_t track_len,
                                            uft_protection_result_t *result);

/**
 * @brief Detect RapidLok protection
 * 
 * Looks for RL Track Header (RL-TH):
 * - 21+ sync bytes
 * - 0x55 ID byte
 * - 164+ 0x7B bytes
 */
const uint8_t *uft_prot_detect_rapidlok(const uint8_t *track_data,
                                        size_t track_len,
                                        uft_protection_result_t *result);

/**
 * @brief Detect fat tracks (span two physical tracks)
 * 
 * @param track_a       First track data
 * @param len_a         First track length
 * @param track_b       Second track data
 * @param len_b         Second track length
 * @param match_bytes   Output: number of matching bytes
 * @return true if likely fat track
 */
bool uft_prot_detect_fat_track(const uint8_t *track_a, size_t len_a,
                               const uint8_t *track_b, size_t len_b,
                               size_t *match_bytes);

/*============================================================================
 * Amiga Protection Detection
 *============================================================================*/

/**
 * @brief Amiga DOS sync words
 */
extern const uint32_t UFT_AMIGA_DOS_SYNCS[4];

/**
 * @brief CopyLock sync words (non-standard)
 */
extern const uint16_t UFT_COPYLOCK_SYNCS[11];

/**
 * @brief Long track lengths indicating protection
 */
extern const uint32_t UFT_AMIGA_LONG_TRACKS[7];

/**
 * @brief Speedlock protection parameters
 */
typedef struct {
    uint32_t offset_bytes;          /**< 9756 */
    uint32_t offset_bits;           /**< 78048 */
    uint16_t long_bytes;            /**< 120 */
    uint16_t short_bytes;           /**< 120 */
    float timing_variation_pct;     /**< 10.0 */
    float ewma_tick_us;             /**< 0.2 */
    uint8_t threshold_ticks;        /**< 8 */
} uft_speedlock_params_t;

extern const uft_speedlock_params_t UFT_SPEEDLOCK_DEFAULT;

/**
 * @brief Detect CopyLock protection
 * 
 * Looks for non-standard sync words indicating CopyLock
 * 
 * @param track_data    MFM track data
 * @param track_len     Track length
 * @param result        Output result
 * @return true if CopyLock detected
 */
bool uft_prot_detect_copylock(const uint8_t *track_data,
                              size_t track_len,
                              uft_protection_result_t *result);

/**
 * @brief Detect Speedlock protection
 * 
 * Analyzes timing variations at specific offsets
 */
bool uft_prot_detect_speedlock(const uint8_t *track_data,
                               size_t track_len,
                               const uint32_t *timing_ns,
                               size_t timing_count,
                               uft_protection_result_t *result);

/**
 * @brief Detect long track protection
 * 
 * @param track_len     Track length in bits
 * @param result        Output result
 * @return true if track length matches known protection
 */
bool uft_prot_detect_long_track(size_t track_len,
                                uft_protection_result_t *result);

/**
 * @brief Detect RNC hidden sectors
 * 
 * Looks for non-standard sync words hiding extra sectors
 */
bool uft_prot_detect_rnc_hidden(const uint8_t *track_data,
                                size_t track_len,
                                uft_protection_result_t *result);

/*============================================================================
 * Generic Protection Detection
 *============================================================================*/

/**
 * @brief Detect weak/fuzzy bits
 * 
 * Compares multiple reads to find inconsistent bits
 * 
 * @param reads         Array of track reads
 * @param read_count    Number of reads
 * @param track_len     Track length
 * @param weak_map      Output: bitmap of weak bit positions
 * @param weak_count    Output: number of weak bits found
 * @return true if weak bits detected
 */
bool uft_prot_detect_weak_bits(const uint8_t **reads,
                               size_t read_count,
                               size_t track_len,
                               uint8_t *weak_map,
                               size_t *weak_count);

/**
 * @brief Detect extra sectors (more than standard)
 */
bool uft_prot_detect_extra_sectors(int expected_sectors,
                                   int found_sectors,
                                   uft_protection_result_t *result);

/**
 * @brief Detect missing sectors
 */
bool uft_prot_detect_missing_sectors(int expected_sectors,
                                     const bool *sector_found,
                                     uft_protection_result_t *result);

/**
 * @brief Detect intentional bad CRC
 */
bool uft_prot_detect_bad_crc(const uint8_t *sector_data,
                             size_t sector_len,
                             uint16_t stored_crc,
                             uint16_t computed_crc,
                             uft_protection_result_t *result);

/*============================================================================
 * Context Management
 *============================================================================*/

/**
 * @brief Initialize protection detection context
 */
int uft_protection_ctx_init(uft_protection_ctx_t *ctx);

/**
 * @brief Free protection detection context
 */
void uft_protection_ctx_free(uft_protection_ctx_t *ctx);

/**
 * @brief Add detection result to context
 */
int uft_protection_ctx_add_result(uft_protection_ctx_t *ctx,
                                  const uft_protection_result_t *result);

/**
 * @brief Scan entire disk for protections
 * 
 * @param ctx           Detection context
 * @param tracks        Array of track data
 * @param track_lens    Array of track lengths
 * @param track_count   Number of tracks
 * @param side_count    Number of sides
 * @return Number of protections detected
 */
int uft_protection_scan_disk(uft_protection_ctx_t *ctx,
                             const uint8_t **tracks,
                             const size_t *track_lens,
                             int track_count,
                             int side_count);

/**
 * @brief Get human-readable protection name
 */
const char *uft_protection_type_name(uft_protection_type_t type);

/**
 * @brief Get protection family name
 */
const char *uft_protection_family_name(uft_protection_type_t type);

/**
 * @brief Generate protection analysis report
 * 
 * @param ctx       Detection context
 * @param buffer    Output buffer
 * @param buf_size  Buffer size
 * @return Length written, or -1 on error
 */
int uft_protection_generate_report(const uft_protection_ctx_t *ctx,
                                   char *buffer,
                                   size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_DETECT_H */
