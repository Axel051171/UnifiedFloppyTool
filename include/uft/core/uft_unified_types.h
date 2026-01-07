/**
 * @file uft_unified_types.h
 * @brief Unified data types for all UFT modules
 * 
 * P0-001: Unified Sector-ID Struct
 * P0-002: Unified Track-Struct
 * P0-004: API compatibility layer
 * 
 * This header defines the canonical data structures that ALL modules
 * must use. Legacy structures are deprecated and will be removed.
 * 
 * MIGRATION: Use UFT_COMPAT_LEGACY_TYPES to enable compatibility macros
 */

#ifndef UFT_UNIFIED_TYPES_H
#define UFT_UNIFIED_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version of this type system */
#define UFT_TYPES_VERSION 0x0100
#define UFT_TYPES_VERSION_STR "1.0"

/* ============================================================================
 * Error Codes (Unified across all modules)
 * ============================================================================ */

typedef enum {
    UFT_OK = 0,
    
    /* Read Errors (0x01-0x1F) */
    UFT_ERR_CRC = 0x01,              /**< CRC mismatch */
    UFT_ERR_SYNC_LOST = 0x02,        /**< Lost sync during read */
    UFT_ERR_NO_DATA = 0x03,          /**< No data found */
    UFT_ERR_WEAK_BITS = 0x04,        /**< Weak/unstable bits detected */
    UFT_ERR_TIMING = 0x05,           /**< Timing anomaly */
    UFT_ERR_ID_MISMATCH = 0x06,      /**< Sector ID mismatch */
    UFT_ERR_DELETED_DATA = 0x07,     /**< Deleted data mark */
    UFT_ERR_MISSING_SECTOR = 0x08,   /**< Sector not found */
    UFT_ERR_INCOMPLETE = 0x09,       /**< Incomplete read */
    UFT_ERR_PLL_UNLOCK = 0x0A,       /**< PLL lost lock */
    UFT_ERR_ENCODING = 0x0B,         /**< Encoding error (illegal pattern) */
    
    /* Write Errors (0x20-0x3F) */
    UFT_ERR_WRITE_PROTECT = 0x20,    /**< Write protected */
    UFT_ERR_VERIFY_FAIL = 0x21,      /**< Verify after write failed */
    UFT_ERR_WRITE_FAULT = 0x22,      /**< Hardware write fault */
    UFT_ERR_TRACK_OVERFLOW = 0x23,   /**< Track too long for format */
    
    /* Protection Errors (0x40-0x5F) */
    UFT_ERR_PROTECTION = 0x40,       /**< Generic protection error */
    UFT_ERR_COPY_DENIED = 0x41,      /**< Copy protection active */
    UFT_ERR_LONG_TRACK = 0x42,       /**< Long track protection */
    UFT_ERR_NON_STANDARD = 0x43,     /**< Non-standard format */
    
    /* Format Errors (0x60-0x7F) */
    UFT_ERR_UNKNOWN_FORMAT = 0x60,   /**< Format not recognized */
    UFT_ERR_UNSUPPORTED = 0x61,      /**< Format not supported */
    UFT_ERR_CORRUPT = 0x62,          /**< File/data corrupt */
    UFT_ERR_VERSION = 0x63,          /**< Version mismatch */
    
    /* System Errors (0x80+) */
    UFT_ERR_IO = 0x80,               /**< I/O error */
    UFT_ERR_MEMORY = 0x81,           /**< Memory allocation failed */
    UFT_ERR_INVALID_PARAM = 0x82,    /**< Invalid parameter */
    UFT_ERR_NOT_IMPL = 0x83,         /**< Not implemented */
    UFT_ERR_TIMEOUT = 0x84,          /**< Operation timed out */
    UFT_ERR_CANCELLED = 0x85,        /**< Operation cancelled */
    UFT_ERR_BUSY = 0x86,             /**< Resource busy */
    UFT_ERR_INTERNAL = 0xFF,         /**< Internal error */
    
} uft_error_t;

/**
 * @brief Get error description string
 */
const char* uft_error_str(uft_error_t err);

/**
 * @brief Check if error is recoverable
 */
bool uft_error_recoverable(uft_error_t err);

/* ============================================================================
 * Sector Identification (Unified - P0-001)
 * ============================================================================ */

/**
 * @brief Unified sector identification
 * 
 * This structure replaces:
 * - flux_sector_id_t (flux_decode.h)
 * - xcopy_sector_t (xcopy_protection.h)
 * - c64_sector_id_t (c64_protection.h)
 * - mfm_idam_t (mfm_decode.h)
 */
typedef struct {
    uint16_t track;           /**< Physical track number (0-83+) */
    uint8_t  head;            /**< Head/side (0-1) */
    uint8_t  sector;          /**< Logical sector number */
    uint8_t  size_code;       /**< Size code: 0=128, 1=256, 2=512, 3=1024... */
    uint8_t  status;          /**< Status flags (UFT_SECTOR_*) */
    uint8_t  encoding;        /**< Encoding type (UFT_ENC_*) */
    uint8_t  reserved;        /**< Reserved for alignment */
} uft_sector_id_t;

/* Sector status flags */
#define UFT_SECTOR_OK           0x00
#define UFT_SECTOR_CRC_ERROR    0x01
#define UFT_SECTOR_DELETED      0x02
#define UFT_SECTOR_MISSING      0x04
#define UFT_SECTOR_WEAK         0x08
#define UFT_SECTOR_TIMING_ERR   0x10
#define UFT_SECTOR_RECOVERED    0x20
#define UFT_SECTOR_PROTECTED    0x40
#define UFT_SECTOR_PARTIAL      0x80

/* Encoding types */
#define UFT_ENC_UNKNOWN         0x00
#define UFT_ENC_FM              0x01
#define UFT_ENC_MFM             0x02
#define UFT_ENC_M2FM            0x03
#define UFT_ENC_GCR_C64         0x10
#define UFT_ENC_GCR_APPLE       0x11
#define UFT_ENC_GCR_MAC         0x12
#define UFT_ENC_AMIGA_MFM       0x20

/* Sector ID comparison */
static inline bool uft_sector_id_equal(const uft_sector_id_t *a, 
                                       const uft_sector_id_t *b) {
    if (!a || !b) return false;
    return a->track == b->track && 
           a->head == b->head && 
           a->sector == b->sector;
}

/* Compatibility macros for legacy code */
#ifdef UFT_COMPAT_LEGACY_TYPES
#define UFT_SECTOR_CYLINDER(id)  ((id)->track)
#define UFT_SECTOR_SIDE(id)      ((id)->head)
#define UFT_SECTOR_NUM(id)       ((id)->sector)
#define UFT_SECTOR_SIZE(id)      (128U << (id)->size_code)
#endif

/* ============================================================================
 * Sector Data (Unified)
 * ============================================================================ */

/**
 * @brief Unified sector data with metadata
 */
typedef struct {
    uft_sector_id_t id;       /**< Sector identification */
    
    /* Data */
    uint8_t *data;            /**< Sector data (NULL if missing) */
    size_t   data_len;        /**< Data length in bytes */
    
    /* Quality metrics */
    uint8_t *confidence;      /**< Per-byte confidence 0-255 (optional) */
    uint8_t *weak_mask;       /**< Per-byte weak bit flags (optional) */
    
    /* CRC */
    uint32_t crc_stored;      /**< CRC from disk */
    uint32_t crc_calculated;  /**< Calculated CRC */
    bool     crc_valid;       /**< True if CRCs match */
    
    /* Timing (optional, for flux) */
    double  *timing_ns;       /**< Per-bit timing in nanoseconds */
    size_t   timing_count;    /**< Number of timing entries */
    
    /* Error info */
    uft_error_t error;        /**< Primary error code */
    uint8_t     retry_count;  /**< Number of retries used */
    
    /* Position in track */
    size_t  bit_offset;       /**< Bit position in track */
    size_t  byte_offset;      /**< Byte position (for sector formats) */
    
} uft_sector_t;

/* ============================================================================
 * Track Data (Unified - P0-002)
 * ============================================================================ */

/**
 * @brief Unified track data structure
 * 
 * Replaces various track structs across modules.
 */
typedef struct {
    /* Identification */
    uint16_t track_num;       /**< Track number */
    uint8_t  head;            /**< Head/side */
    uint8_t  encoding;        /**< Primary encoding */
    
    /* Sectors */
    uft_sector_t *sectors;    /**< Array of sectors */
    size_t        sector_count;
    size_t        sector_capacity;
    
    /* Raw data (bitstream) */
    uint8_t *raw_data;        /**< Raw track bits */
    size_t   raw_bits;        /**< Number of bits */
    size_t   raw_capacity;    /**< Allocated bytes */
    
    /* Flux data (optional) */
    double  *flux_times;      /**< Flux transition times (ns) */
    size_t   flux_count;      /**< Number of transitions */
    
    /* Multi-revision data */
    struct {
        uint8_t *data;
        size_t   bits;
        uint8_t  quality;     /**< 0-100 quality score */
    } *revisions;
    size_t revision_count;
    
    /* Quality metrics */
    uint8_t *confidence;      /**< Per-bit confidence */
    bool    *weak_mask;       /**< Per-bit weak flags */
    
    /* Track-level status */
    uft_error_t error;        /**< Primary error */
    uint8_t     quality;      /**< Overall quality 0-100 */
    bool        complete;     /**< All sectors found */
    bool        protected;    /**< Copy protection detected */
    
    /* Timing */
    uint64_t rotation_ns;     /**< Rotation time */
    double   data_rate;       /**< Data rate in bits/sec */
    
    /* Ownership */
    bool owns_data;           /**< True = free on destroy */
    
} uft_track_t;

/* ============================================================================
 * Disk Image (Unified)
 * ============================================================================ */

/**
 * @brief Format identifier
 */
typedef enum {
    UFT_FMT_UNKNOWN = 0,
    
    /* Sector-based */
    UFT_FMT_IMG,
    UFT_FMT_IMA,
    UFT_FMT_DSK,
    UFT_FMT_D64,
    UFT_FMT_D71,
    UFT_FMT_D81,
    UFT_FMT_D82,
    UFT_FMT_ADF,
    UFT_FMT_MSA,
    UFT_FMT_ST,
    UFT_FMT_ATR,
    UFT_FMT_XFD,
    
    /* Extended/Metadata */
    UFT_FMT_G64,
    UFT_FMT_NIB,
    UFT_FMT_DMK,
    UFT_FMT_TD0,
    UFT_FMT_IMD,
    UFT_FMT_EDSK,
    UFT_FMT_HFE,
    UFT_FMT_IPF,
    UFT_FMT_FDI,
    UFT_FMT_CQM,
    
    /* Flux */
    UFT_FMT_SCP,
    UFT_FMT_A2R,
    UFT_FMT_WOZ,
    UFT_FMT_UFT_KF_RAW,
    UFT_FMT_GWRAW,
    UFT_FMT_MOOF,
    
    /* Japanese */
    UFT_FMT_D88,
    UFT_FMT_NFD,
    UFT_FMT_FDD,
    UFT_FMT_HDM,
    
    UFT_FMT_MAX
} uft_format_id_t;

/**
 * @brief Protection type
 */
typedef enum {
    UFT_PROT_NONE = 0,
    
    /* C64 */
    UFT_PROT_RAPIDLOK = 0x0100,
    UFT_PROT_RAPIDLOK2 = 0x0101,
    UFT_PROT_RAPIDLOK6 = 0x0102,
    UFT_PROT_VORPAL = 0x0200,
    UFT_PROT_VMAX = 0x0300,
    UFT_PROT_VMAX3 = 0x0301,
    UFT_PROT_EA = 0x0400,
    UFT_PROT_GEOS = 0x0500,
    
    /* Amiga */
    UFT_PROT_COPYLOCK = 0x1000,
    UFT_PROT_LONG_TRACK = 0x1100,
    UFT_PROT_WEAK_BITS_AMIGA = 0x1200,
    
    /* Apple */
    UFT_PROT_NIBBLE_COUNT = 0x2000,
    UFT_PROT_SPIRAL = 0x2100,
    
    /* PC */
    UFT_PROT_WEAK_BITS_PC = 0x3000,
    UFT_PROT_XDF = 0x3100,
    
} uft_protection_t;

/**
 * @brief Protection information
 */
typedef struct {
    uft_protection_t type;
    uint8_t confidence;       /**< Detection confidence 0-100 */
    uint8_t track_start;      /**< First protected track */
    uint8_t track_end;        /**< Last protected track */
    const char *name;         /**< Protection name */
    const char *description;  /**< Description */
    
    /* Preservation requirements */
    bool requires_flux;
    bool requires_timing;
    bool requires_weak_bits;
    bool requires_long_tracks;
    
} uft_protection_info_t;

/**
 * @brief Unified disk image structure
 */
typedef struct {
    /* Format info */
    uft_format_id_t format;
    char format_name[32];
    
    /* Geometry */
    uint16_t tracks;          /**< Total tracks */
    uint8_t  heads;           /**< Number of heads (1-2) */
    uint8_t  sectors_per_track; /**< 0 = variable */
    uint16_t bytes_per_sector;  /**< 0 = variable */
    
    /* Track data [track * heads + head] */
    uft_track_t **track_data;
    size_t track_count;
    
    /* Protection info */
    uft_protection_info_t protection;
    
    /* Forensic info */
    struct {
        bool has_errors;
        bool has_weak_bits;
        bool has_timing;
        uint32_t bad_sector_count;
        uint32_t recovered_count;
    } forensic;
    
    /* Multi-revision */
    uint8_t revision_count;
    
    /* File info */
    char *source_path;
    uint64_t file_size;
    uint32_t file_crc32;
    
    /* Ownership */
    bool owns_data;
    
} uft_disk_image_t;

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/**
 * @brief Allocate sector structure
 */
uft_sector_t* uft_sector_alloc(size_t data_len);

/**
 * @brief Free sector structure
 */
void uft_sector_free(uft_sector_t *sector);

/**
 * @brief Allocate track structure
 */
uft_track_t* uft_track_alloc(size_t max_sectors, size_t max_raw_bits);

/**
 * @brief Free track structure
 */
void uft_track_free(uft_track_t *track);

/**
 * @brief Allocate disk image structure
 */
uft_disk_image_t* uft_disk_alloc(uint16_t tracks, uint8_t heads);

/**
 * @brief Free disk image structure
 */
void uft_disk_free(uft_disk_image_t *disk);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get format name string
 */
const char* uft_format_name(uft_format_id_t format);

/**
 * @brief Get encoding name string
 */
const char* uft_encoding_name(uint8_t encoding);

/**
 * @brief Calculate sector size from size code
 */
static inline size_t uft_size_from_code(uint8_t code) {
    return (code < 8) ? (128U << code) : 0;
}

/**
 * @brief Calculate size code from sector size
 */
static inline uint8_t uft_code_from_size(size_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 2;  /* Default to 512 */
    }
}

/**
 * @brief Deep copy sector
 */
int uft_sector_copy(uft_sector_t *dest, const uft_sector_t *src);

/**
 * @brief Deep copy track
 */
int uft_track_copy(uft_track_t *dest, const uft_track_t *src);

/**
 * @brief Compare two disks
 */
typedef enum {
    UFT_CMP_IDENTICAL = 0,
    UFT_CMP_DATA_DIFFERS = 1,
    UFT_CMP_GEOMETRY_DIFFERS = 2,
    UFT_CMP_METADATA_DIFFERS = 4,
} uft_compare_result_t;

int uft_disk_compare(const uft_disk_image_t *a, 
                     const uft_disk_image_t *b,
                     uft_compare_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UNIFIED_TYPES_H */
