/**
 * @file uft_track_base.h
 * @brief Unified Track Base Structure (P2-ARCH-001)
 * 
 * This header defines a common track structure that all format-specific
 * track types can inherit from or convert to. This enables:
 * - Consistent track handling across all formats
 * - Easy conversion between formats
 * - Unified APIs for track operations
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_TRACK_BASE_H
#define UFT_TRACK_BASE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_TRACK_MAX_SECTORS       32      /**< Max sectors per track */
#define UFT_TRACK_MAX_REVOLUTIONS   8       /**< Max revolutions stored */
#define UFT_TRACK_MAX_WEAK_REGIONS  64      /**< Max weak bit regions */

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Track encoding type (unified across all formats)
 */
typedef enum uft_track_encoding {
    UFT_ENC_UNKNOWN     = 0,
    UFT_ENC_FM          = 1,    /**< Single density FM */
    UFT_ENC_MFM         = 2,    /**< Standard MFM (IBM) */
    UFT_ENC_GCR_C64     = 3,    /**< Commodore 64/1541 GCR */
    UFT_ENC_GCR_APPLE   = 4,    /**< Apple II GCR */
    UFT_ENC_AMIGA_MFM   = 5,    /**< Amiga MFM (odd/even) */
    UFT_ENC_GCR_VICTOR  = 6,    /**< Victor 9000 GCR */
    UFT_ENC_M2FM        = 7,    /**< M2FM (Intel) */
    UFT_ENC_RAW         = 255   /**< Raw flux/bitstream */
} uft_track_encoding_t;

/**
 * @brief Track status flags (bitfield)
 */
typedef enum uft_track_flags {
    UFT_TF_NONE         = 0,
    UFT_TF_PRESENT      = (1 << 0),  /**< Track data exists */
    UFT_TF_INDEXED      = (1 << 1),  /**< Index-aligned */
    UFT_TF_WEAK_BITS    = (1 << 2),  /**< Contains weak/fuzzy bits */
    UFT_TF_PROTECTED    = (1 << 3),  /**< Copy protection detected */
    UFT_TF_LONG         = (1 << 4),  /**< Longer than standard */
    UFT_TF_SHORT        = (1 << 5),  /**< Shorter than standard */
    UFT_TF_MODIFIED     = (1 << 6),  /**< Track was modified */
    UFT_TF_VARIABLE_DENSITY = (1 << 7),  /**< Variable density zones */
    UFT_TF_HALF_TRACK   = (1 << 8),  /**< Half-track position */
    UFT_TF_CRC_ERRORS   = (1 << 9),  /**< Has CRC errors */
    UFT_TF_MULTI_REV    = (1 << 10)  /**< Multi-revolution data */
} uft_track_flags_t;

/**
 * @brief Track quality rating
 */
typedef enum uft_track_quality {
    UFT_TQ_UNKNOWN      = 0,
    UFT_TQ_PERFECT      = 1,    /**< All sectors good, no issues */
    UFT_TQ_GOOD         = 2,    /**< Minor issues, fully readable */
    UFT_TQ_MARGINAL     = 3,    /**< Some bad sectors, recoverable */
    UFT_TQ_POOR         = 4,    /**< Many errors, partial data */
    UFT_TQ_UNREADABLE   = 5     /**< Cannot decode */
} uft_track_quality_t;

/*===========================================================================
 * Core Structures
 *===========================================================================*/

/**
 * @brief Weak bit region descriptor
 */
typedef struct uft_weak_region {
    uint32_t bit_offset;        /**< Start bit offset */
    uint32_t bit_length;        /**< Length in bits */
    uint8_t  variation_count;   /**< Number of observed variations */
    uint8_t  decay_rate;        /**< Signal decay rate (0-255) */
} uft_weak_region_t;

/**
 * @brief Sector descriptor (common subset)
 */
typedef struct uft_sector_base {
    uint8_t  sector_id;         /**< Logical sector number */
    uint8_t  cylinder_id;       /**< Cylinder ID from header */
    uint8_t  head_id;           /**< Head ID from header */
    uint8_t  size_code;         /**< Size code (128<<N) */
    
    uint16_t data_size;         /**< Actual data size in bytes */
    uint16_t flags;             /**< Sector flags */
    
    uint32_t bit_offset;        /**< Bit offset in track */
    uint16_t header_crc;        /**< Header CRC (read) */
    uint16_t data_crc;          /**< Data CRC (read) */
    
    bool     header_ok;         /**< Header CRC valid */
    bool     data_ok;           /**< Data CRC valid */
    bool     deleted;           /**< Deleted data mark */
    bool     weak;              /**< Contains weak bits */
    
    uint8_t *data;              /**< Sector data (NULL if not loaded) */
} uft_sector_base_t;

/**
 * @brief Revolution data (for multi-revolution formats)
 */
typedef struct uft_revolution_base {
    uint32_t index_time_ns;     /**< Index-to-index time in ns */
    uint32_t bit_count;         /**< Total bits in revolution */
    float    quality_score;     /**< Quality score 0.0-1.0 */
    
    uint8_t *flux_data;         /**< Flux transition data (optional) */
    size_t   flux_size;         /**< Flux data size */
    
    uint8_t *bitstream;         /**< Decoded bitstream (optional) */
    size_t   bitstream_bits;    /**< Bitstream length in bits */
} uft_revolution_base_t;

/**
 * @brief Unified Track Base Structure
 */
typedef struct uft_track_base {
    /* ═══ Position ═══ */
    uint8_t  cylinder;          /**< Physical cylinder (0-83) */
    uint8_t  head;              /**< Head/side (0-1) */
    int8_t   cyl_offset_q;      /**< Quarter-track offset (-2 to +2) */
    uint8_t  _pad0;
    
    /* ═══ Status ═══ */
    uint16_t flags;             /**< uft_track_flags_t */
    uint8_t  quality;           /**< uft_track_quality_t */
    uint8_t  encoding;          /**< uft_track_encoding_t */
    
    /* ═══ Sector Info ═══ */
    uint8_t  sectors_expected;  /**< Expected sector count */
    uint8_t  sectors_found;     /**< Actually found */
    uint8_t  sectors_good;      /**< With valid CRC */
    uint8_t  sectors_bad;       /**< With CRC errors */
    
    uft_sector_base_t *sectors; /**< Sector array */
    size_t   sector_count;
    size_t   sector_capacity;
    
    /* ═══ Timing ═══ */
    uint32_t bitcell_ns;        /**< Nominal bitcell time (ns) */
    uint32_t rpm_x100;          /**< RPM × 100 */
    uint32_t track_time_ns;     /**< Total track time (ns) */
    uint32_t write_splice_ns;   /**< Write splice location */
    
    /* ═══ Size ═══ */
    uint32_t bit_length;
    uint32_t byte_length;
    
    /* ═══ Raw Data ═══ */
    uint8_t *bitstream;
    size_t   bitstream_size;
    uint8_t *flux_data;
    size_t   flux_count;
    
    /* ═══ Weak Bits ═══ */
    uint8_t *weak_mask;
    uft_weak_region_t weak_regions[UFT_TRACK_MAX_WEAK_REGIONS];
    size_t   weak_region_count;
    
    /* ═══ Multi-Revolution ═══ */
    uint8_t  revolution_count;
    uint8_t  best_revolution;
    uint8_t  _pad1[2];
    uft_revolution_base_t *revolutions[UFT_TRACK_MAX_REVOLUTIONS];
    
    /* ═══ Detection Info ═══ */
    float    detection_confidence;
    uint32_t protection_type;
    
    void    *user_data;
} uft_track_base_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

uft_track_base_t* uft_track_base_create(uint8_t cylinder, uint8_t head);
void uft_track_base_free(uft_track_base_t *track);
uft_track_base_t* uft_track_base_clone(const uft_track_base_t *src);
void uft_track_base_reset(uft_track_base_t *track);

int uft_track_base_alloc_sectors(uft_track_base_t *track, size_t count);
int uft_track_base_add_sector(uft_track_base_t *track, const uft_sector_base_t *sector);
uft_sector_base_t* uft_track_base_find_sector(uft_track_base_t *track, uint8_t sector_id);
void uft_track_base_sort_sectors(uft_track_base_t *track);

int uft_track_base_alloc_bitstream(uft_track_base_t *track, size_t bits);
int uft_track_base_alloc_flux(uft_track_base_t *track, size_t count);
int uft_track_base_alloc_weak_mask(uft_track_base_t *track);
int uft_track_base_add_revolution(uft_track_base_t *track, uft_revolution_base_t *rev);

uft_track_quality_t uft_track_base_calc_quality(const uft_track_base_t *track);
void uft_track_base_update_stats(uft_track_base_t *track);
bool uft_track_base_has_errors(const uft_track_base_t *track);
int uft_track_base_error_summary(const uft_track_base_t *track, char *buffer, size_t size);

const char* uft_track_encoding_name(uft_track_encoding_t enc);
const char* uft_track_quality_name(uft_track_quality_t qual);
int uft_track_flags_describe(uint16_t flags, char *buffer, size_t size);
uft_track_encoding_t uft_track_encoding_from_legacy(int legacy_enc);
int uft_track_encoding_to_legacy(uft_track_encoding_t enc);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_BASE_H */
