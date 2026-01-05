/**
 * @file uft_sector.h
 * @brief Unified Sector Structure (P2-ARCH-004)
 * 
 * Central sector structure for all UFT subsystems.
 * Consolidates: uft_sector_t, ipf_sector, amiga_sector_node, etc.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_SECTOR_H
#define UFT_SECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_SECTOR_MAX_SIZE         8192    /**< Max sector data size */
#define UFT_SECTOR_MAX_ALT_DATA     4       /**< Max alternative data versions */

/*===========================================================================
 * Sector Status Flags
 *===========================================================================*/

/**
 * @brief Sector status flags (bitfield)
 */
typedef enum uft_sector_flags {
    UFT_SF_NONE             = 0,
    
    /* Presence */
    UFT_SF_PRESENT          = (1 << 0),  /**< Sector exists */
    UFT_SF_DATA_PRESENT     = (1 << 1),  /**< Has data (vs header only) */
    
    /* CRC Status */
    UFT_SF_HEADER_CRC_OK    = (1 << 2),  /**< Header CRC valid */
    UFT_SF_DATA_CRC_OK      = (1 << 3),  /**< Data CRC valid */
    UFT_SF_CRC_CORRECTED    = (1 << 4),  /**< CRC error was corrected */
    
    /* Data Marks */
    UFT_SF_DELETED_DATA     = (1 << 5),  /**< Deleted data mark (DAM=0xF8) */
    UFT_SF_CONTROL_DATA     = (1 << 6),  /**< Control data mark */
    
    /* Quality */
    UFT_SF_WEAK_BITS        = (1 << 7),  /**< Contains weak/fuzzy bits */
    UFT_SF_TIMING_VARIANCE  = (1 << 8),  /**< Unusual timing detected */
    UFT_SF_MULTIPLE_COPIES  = (1 << 9),  /**< Multiple revolutions differ */
    
    /* Copy Protection */
    UFT_SF_PROTECTED        = (1 << 10), /**< Part of protection scheme */
    UFT_SF_FAKE_CRC         = (1 << 11), /**< Intentionally bad CRC */
    UFT_SF_NO_DAM           = (1 << 12), /**< Missing data address mark */
    UFT_SF_PHANTOM          = (1 << 13), /**< Phantom/duplicate sector */
    
    /* Format-Specific */
    UFT_SF_INTERLEAVED      = (1 << 14), /**< Interleaved data (Amiga) */
    UFT_SF_DENSITY_MISMATCH = (1 << 15)  /**< Wrong density for track */
} uft_sector_flags_t;

/*===========================================================================
 * Sector Address
 *===========================================================================*/

/**
 * @brief Sector address (IDAM/Header)
 */
typedef struct uft_sector_addr {
    uint8_t  cylinder;          /**< Cylinder/track from header */
    uint8_t  head;              /**< Head/side from header */
    uint8_t  sector;            /**< Sector number from header */
    uint8_t  size_code;         /**< Size code (N): size = 128 << N */
    
    uint16_t header_crc_stored; /**< CRC from disk */
    uint16_t header_crc_calc;   /**< Calculated CRC */
    
    uint32_t bit_position;      /**< Bit position in track */
    uint32_t byte_position;     /**< Byte position in track */
} uft_sector_addr_t;

/*===========================================================================
 * Sector Data
 *===========================================================================*/

/**
 * @brief Single version of sector data
 */
typedef struct uft_sector_data_version {
    uint8_t *data;              /**< Sector data */
    uint16_t size;              /**< Data size in bytes */
    
    uint16_t data_crc_stored;   /**< CRC from disk */
    uint16_t data_crc_calc;     /**< Calculated CRC */
    
    uint8_t  data_mark;         /**< Data address mark (0xFB, 0xF8, etc.) */
    uint8_t  revolution;        /**< Which revolution this came from */
    
    float    confidence;        /**< Read confidence (0.0-1.0) */
    uint8_t *weak_mask;         /**< Weak bit mask (NULL if none) */
} uft_sector_data_version_t;

/*===========================================================================
 * Unified Sector Structure
 *===========================================================================*/

/**
 * @brief Unified sector structure
 */
typedef struct uft_sector_unified {
    /* ═══ Address (Header/IDAM) ═══ */
    uft_sector_addr_t addr;
    
    /* ═══ Status ═══ */
    uint16_t flags;             /**< uft_sector_flags_t */
    
    /* ═══ Primary Data ═══ */
    uint8_t *data;              /**< Primary data buffer */
    uint16_t data_size;         /**< Size in bytes */
    
    uint16_t data_crc_stored;   /**< CRC from disk */
    uint16_t data_crc_calc;     /**< Calculated CRC */
    
    uint8_t  data_mark;         /**< Data address mark */
    
    /* ═══ Quality Metrics ═══ */
    float    confidence;        /**< Overall confidence (0.0-1.0) */
    float    timing_variance;   /**< Timing variance from nominal */
    uint8_t  error_bits;        /**< Estimated bit errors */
    
    /* ═══ Position in Track ═══ */
    uint32_t bit_start;         /**< Start bit position (header) */
    uint32_t bit_end;           /**< End bit position (after data) */
    uint32_t gap_before;        /**< Gap bits before this sector */
    
    /* ═══ Multi-Revolution Data ═══ */
    uint8_t  version_count;     /**< Number of data versions */
    uint8_t  best_version;      /**< Index of best quality version */
    uft_sector_data_version_t *versions[UFT_SECTOR_MAX_ALT_DATA];
    
    /* ═══ Protection Info ═══ */
    uint32_t protection_type;   /**< Detected protection scheme */
    uint8_t *original_timing;   /**< Original timing data (if preserved) */
    size_t   timing_size;
    
    /* ═══ User Data ═══ */
    void    *user_data;         /**< Format-specific extension */
} uft_sector_unified_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/* Lifecycle */
uft_sector_unified_t* uft_sector_create(void);
void uft_sector_free(uft_sector_unified_t *sector);
uft_sector_unified_t* uft_sector_clone(const uft_sector_unified_t *src);
void uft_sector_reset(uft_sector_unified_t *sector);

/* Data Management */
int uft_sector_alloc_data(uft_sector_unified_t *sector, size_t size);
int uft_sector_set_data(uft_sector_unified_t *sector, 
                        const uint8_t *data, size_t size);
int uft_sector_add_version(uft_sector_unified_t *sector,
                           const uft_sector_data_version_t *version);

/* Address */
void uft_sector_set_addr(uft_sector_unified_t *sector,
                         uint8_t cyl, uint8_t head, uint8_t sec, uint8_t size_code);
uint16_t uft_sector_get_size(const uft_sector_unified_t *sector);

/* Status Checks */
static inline bool uft_sector_is_valid(const uft_sector_unified_t *s) {
    return s && (s->flags & UFT_SF_PRESENT);
}
static inline bool uft_sector_crc_ok(const uft_sector_unified_t *s) {
    return s && (s->flags & UFT_SF_DATA_CRC_OK);
}
static inline bool uft_sector_is_deleted(const uft_sector_unified_t *s) {
    return s && (s->flags & UFT_SF_DELETED_DATA);
}
static inline bool uft_sector_has_weak_bits(const uft_sector_unified_t *s) {
    return s && (s->flags & UFT_SF_WEAK_BITS);
}
static inline bool uft_sector_is_protected(const uft_sector_unified_t *s) {
    return s && (s->flags & UFT_SF_PROTECTED);
}

/* Quality */
float uft_sector_calc_confidence(const uft_sector_unified_t *sector);
void uft_sector_select_best_version(uft_sector_unified_t *sector);

/* Comparison */
bool uft_sector_data_equals(const uft_sector_unified_t *a,
                            const uft_sector_unified_t *b);
int uft_sector_compare_addr(const uft_sector_unified_t *a,
                            const uft_sector_unified_t *b);

/* Debug */
int uft_sector_dump(const uft_sector_unified_t *sector, 
                    char *buffer, size_t size);
const char* uft_sector_flags_str(uint16_t flags);

/*===========================================================================
 * Legacy Conversion
 *===========================================================================*/

/* Forward declarations for legacy types */
struct uft_sector;      /* uft_unified_decoder.h */
struct ipf_sector;      /* uft_ipf.h */
struct amiga_sector_node;

/* Conversion functions */
int uft_sector_from_decoder(const struct uft_sector *src,
                            uft_sector_unified_t *dst);
int uft_sector_to_decoder(const uft_sector_unified_t *src,
                          struct uft_sector *dst);

int uft_sector_from_ipf(const struct ipf_sector *src,
                        uft_sector_unified_t *dst);
int uft_sector_to_ipf(const uft_sector_unified_t *src,
                      struct ipf_sector *dst);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_H */
