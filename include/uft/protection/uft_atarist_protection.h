/**
 * @file uft_atarist_protection.h
 * @brief Atari ST Copy Protection Detection
 * 
 * Detects and analyzes Atari ST copy protection schemes:
 * - Copylock ST (Rob Northen)
 * - Macrodos
 * - Fuzzy Sectors
 * - Long Tracks
 * - Flaschel (FDC Bug Exploit)
 * 
 */

#ifndef UFT_ATARIST_PROTECTION_H
#define UFT_ATARIST_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Atari ST disk parameters */
#define UFT_ATARIST_TRACKS          80
#define UFT_ATARIST_SIDES           2
#define UFT_ATARIST_SECTORS_DD      9       /**< DD: 720KB */
#define UFT_ATARIST_SECTORS_HD      18      /**< HD: 1.44MB */
#define UFT_ATARIST_SECTOR_SIZE     512

/** Protection thresholds */
#define UFT_ATARIST_LONG_TRACK_MIN  6500    /**< Minimum bytes for long track */
#define UFT_ATARIST_FUZZY_THRESHOLD 3       /**< Revolutions for fuzzy check */
#define UFT_ATARIST_FLASCHEL_GAP    0x4E    /**< Flaschel gap byte */

/*===========================================================================
 * Protection Types
 *===========================================================================*/

/**
 * @brief Atari ST protection types
 */
typedef enum {
    UFT_ATARIST_PROT_NONE = 0,
    UFT_ATARIST_PROT_COPYLOCK,          /**< Rob Northen Copylock */
    UFT_ATARIST_PROT_MACRODOS,          /**< Macrodos protection */
    UFT_ATARIST_PROT_FUZZY_SECTOR,      /**< Fuzzy/weak sectors */
    UFT_ATARIST_PROT_LONG_TRACK,        /**< Extended track length */
    UFT_ATARIST_PROT_FLASCHEL,          /**< FDC bug exploit */
    UFT_ATARIST_PROT_NO_FLUX,           /**< No-flux area */
    UFT_ATARIST_PROT_SECTOR_GAP,        /**< Modified sector gaps */
    UFT_ATARIST_PROT_HIDDEN_DATA,       /**< Hidden inter-sector data */
    UFT_ATARIST_PROT_MULTIPLE           /**< Multiple protections */
} uft_atarist_prot_type_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Copylock protection info
 */
typedef struct {
    uint8_t  track;                 /**< Protection track */
    uint8_t  side;                  /**< Disk side */
    
    /* LFSR signature */
    uint32_t lfsr_seed;             /**< LFSR seed value */
    uint32_t lfsr_poly;             /**< LFSR polynomial */
    uint32_t signature[4];          /**< Signature values */
    
    /* Timing info */
    uint16_t key_track;             /**< Key track number */
    uint32_t timing_value;          /**< Critical timing value */
    
    bool     detected;              /**< Copylock detected */
    double   confidence;            /**< Detection confidence */
} uft_copylock_st_t;

/**
 * @brief Fuzzy sector protection info
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint8_t  side;                  /**< Disk side */
    uint8_t  sector;                /**< Sector number */
    
    /* Multi-revolution data */
    uint8_t  revolutions;           /**< Revolutions analyzed */
    uint8_t  variations;            /**< Number of variations */
    uint32_t *variation_offsets;    /**< Offset of each variation */
    
    /* Weak bit info */
    uint32_t weak_bit_start;        /**< First weak bit position */
    uint32_t weak_bit_count;        /**< Number of weak bits */
    
    bool     detected;              /**< Fuzzy sector detected */
    double   confidence;            /**< Detection confidence */
} uft_fuzzy_sector_t;

/**
 * @brief Flaschel protection info
 * 
 * Flaschel exploits a bug in the WD1772 FDC where specific
 * gap patterns cause the controller to misread sector IDs.
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint8_t  side;                  /**< Disk side */
    
    /* FDC bug trigger */
    uint32_t gap_position;          /**< Position of exploit gap */
    uint16_t gap_length;            /**< Gap length in bytes */
    uint8_t  gap_pattern[16];       /**< Gap pattern bytes */
    
    /* Sector confusion */
    uint8_t  visible_sector;        /**< Sector ID visible to FDC */
    uint8_t  actual_sector;         /**< Actual sector number */
    int8_t   sector_offset;         /**< Sector number offset */
    
    /* Verification */
    bool     fdc_bug_triggered;     /**< FDC bug would trigger */
    uint16_t timing_margin_ns;      /**< Timing margin */
    
    bool     detected;              /**< Flaschel detected */
    double   confidence;            /**< Detection confidence */
} uft_flaschel_t;

/**
 * @brief Long track protection info
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint8_t  side;                  /**< Disk side */
    
    uint32_t standard_length;       /**< Standard track length */
    uint32_t actual_length;         /**< Actual track length */
    uint32_t extra_bytes;           /**< Extra bytes beyond standard */
    
    /* Extra data location */
    uint32_t extra_data_start;      /**< Start of extra data */
    uint8_t  extra_data_preview[32];/**< First 32 bytes of extra data */
    
    bool     detected;              /**< Long track detected */
    double   confidence;            /**< Detection confidence */
} uft_long_track_st_t;

/**
 * @brief Combined Atari ST protection result
 */
typedef struct {
    uft_atarist_prot_type_t primary_type; /**< Primary protection type */
    uint32_t type_flags;                   /**< All detected types (bitmask) */
    
    /* Protection details */
    uft_copylock_st_t copylock;            /**< Copylock info */
    
    uft_fuzzy_sector_t *fuzzy_sectors;     /**< Fuzzy sector array */
    uint8_t fuzzy_sector_count;            /**< Fuzzy sector count */
    
    uft_flaschel_t *flaschels;             /**< Flaschel array */
    uint8_t flaschel_count;                /**< Flaschel count */
    
    uft_long_track_st_t *long_tracks;      /**< Long track array */
    uint8_t long_track_count;              /**< Long track count */
    
    /* Summary */
    double overall_confidence;             /**< Overall confidence */
    char description[256];                 /**< Human-readable description */
} uft_atarist_prot_result_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

typedef struct {
    bool detect_copylock;           /**< Detect Copylock */
    bool detect_fuzzy;              /**< Detect fuzzy sectors */
    bool detect_flaschel;           /**< Detect Flaschel */
    bool detect_long_track;         /**< Detect long tracks */
    
    uint8_t fuzzy_revolutions;      /**< Revolutions for fuzzy check */
    uint16_t long_track_threshold;  /**< Long track threshold */
} uft_atarist_detect_config_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Initialize detection config
 */
void uft_atarist_config_init(uft_atarist_detect_config_t *config);

/**
 * @brief Allocate protection result
 */
uft_atarist_prot_result_t *uft_atarist_result_alloc(void);

/**
 * @brief Free protection result
 */
void uft_atarist_result_free(uft_atarist_prot_result_t *result);

/**
 * @brief Detect Flaschel protection
 * 
 * @param track_data Raw MFM track data
 * @param track_len Track data length
 * @param track_num Track number
 * @param side Disk side
 * @param result Output Flaschel result
 * @return 0 on success
 */
int uft_atarist_detect_flaschel(const uint8_t *track_data, size_t track_len,
                                uint8_t track_num, uint8_t side,
                                uft_flaschel_t *result);

/**
 * @brief Detect fuzzy sector protection
 * 
 * @param revolutions Array of track revolutions
 * @param rev_lens Array of revolution lengths
 * @param rev_count Number of revolutions
 * @param track_num Track number
 * @param side Disk side
 * @param results Output fuzzy sector array
 * @param max_results Maximum results
 * @return Number of fuzzy sectors found
 */
size_t uft_atarist_detect_fuzzy(const uint8_t **revolutions,
                                const size_t *rev_lens, uint8_t rev_count,
                                uint8_t track_num, uint8_t side,
                                uft_fuzzy_sector_t *results, size_t max_results);

/**
 * @brief Detect long track protection
 * 
 * @param track_data Raw track data
 * @param track_len Track data length
 * @param track_num Track number
 * @param side Disk side
 * @param result Output long track result
 * @return 0 on success
 */
int uft_atarist_detect_long_track(const uint8_t *track_data, size_t track_len,
                                  uint8_t track_num, uint8_t side,
                                  uft_long_track_st_t *result);

/**
 * @brief Full protection detection
 */
int uft_atarist_detect_all(const uint8_t **tracks, const size_t *track_lens,
                           const uint8_t **revolutions, const size_t *rev_lens,
                           uint8_t rev_count, uint8_t track_count,
                           const uft_atarist_detect_config_t *config,
                           uft_atarist_prot_result_t *result);

/**
 * @brief Get protection type name
 */
const char *uft_atarist_prot_name(uft_atarist_prot_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARIST_PROTECTION_H */
