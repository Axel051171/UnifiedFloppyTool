/**
 * @file uft_ipf_parser.h
 * @brief IPF (Interchangeable Preservation Format) Parser
 * 
 * Parser for IPF format (Software Preservation Society).
 * Supports:
 * - IPF v1 and v2 formats
 * - Full track data with timing information
 * - Weak/fuzzy bit detection
 * - Copy protection analysis
 * - CTRaw (raw cell timing) data
 * 
 * @author UFT Team
 * @version 3.4.0
 * @date 2026-01-03
 * 
 * References:
 * - Software Preservation Society IPF Specification
 * - CAPS Library Documentation
 */

#ifndef UFT_IPF_PARSER_H
#define UFT_IPF_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** IPF file signature "CAPS" */
#define IPF_SIGNATURE       "CAPS"
#define IPF_SIGNATURE_LEN   4

/** Record types */
#define IPF_RECORD_CAPS     0x4341  /* 'CA' - CAPS header */
#define IPF_RECORD_INFO     0x494E  /* 'IN' - INFO record */
#define IPF_RECORD_IMGE     0x494D  /* 'IM' - Image record */
#define IPF_RECORD_DATA     0x4441  /* 'DA' - Data record */
#define IPF_RECORD_TRCK     0x5452  /* 'TR' - Track record (v2) */

/** Encoder types */
#define IPF_ENC_UNKNOWN     0
#define IPF_ENC_CAPS        1       /* CAPS encoded */
#define IPF_ENC_SPS         2       /* SPS encoded */
#define IPF_ENC_CTRAW       3       /* CTRaw (cell timing) */

/** Platform types */
#define IPF_PLATFORM_AMIGA      1
#define IPF_PLATFORM_ATARI_ST   2
#define IPF_PLATFORM_PC         3
#define IPF_PLATFORM_AMSTRAD    4
#define IPF_PLATFORM_SPECTRUM   5
#define IPF_PLATFORM_SAM_COUPE  6
#define IPF_PLATFORM_ARCHIMEDES 7
#define IPF_PLATFORM_C64        8
#define IPF_PLATFORM_ATARI_8BIT 9

/** Density types */
#define IPF_DENSITY_AUTO        0
#define IPF_DENSITY_NOISE       1
#define IPF_DENSITY_DD          2   /* Double density */
#define IPF_DENSITY_HD          3   /* High density */
#define IPF_DENSITY_ED          4   /* Extra density */

/** Data flags */
#define IPF_FLAG_FUZZY          0x0001  /* Contains fuzzy bits */
#define IPF_FLAG_WEAK           0x0002  /* Contains weak bits */
#define IPF_FLAG_SYNC           0x0004  /* Contains sync marks */
#define IPF_FLAG_EXTRA          0x0008  /* Has extra data */

/** Maximum values */
#define IPF_MAX_TRACKS          168     /* 84 tracks * 2 sides */
#define IPF_MAX_SECTORS         32      /* Per track */
#define IPF_MAX_GAPS            8       /* Per sector */
#define IPF_MAX_DATA_AREAS      64      /* Per track */

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    IPF_OK = 0,
    IPF_ERR_NULL_PARAM,
    IPF_ERR_FILE_OPEN,
    IPF_ERR_FILE_READ,
    IPF_ERR_BAD_SIGNATURE,
    IPF_ERR_UNSUPPORTED_VERSION,
    IPF_ERR_BAD_RECORD,
    IPF_ERR_NO_INFO,
    IPF_ERR_NO_DATA,
    IPF_ERR_TRACK_RANGE,
    IPF_ERR_SECTOR_RANGE,
    IPF_ERR_CRC,
    IPF_ERR_ALLOC,
    IPF_ERR_DECODE,
    IPF_ERR_CORRUPT,
    IPF_ERR_COUNT
} ipf_error_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Gap element
 */
typedef struct {
    uint32_t    type;           /**< Gap type */
    uint32_t    size;           /**< Gap size in bits */
    uint8_t    *pattern;        /**< Gap fill pattern */
    uint32_t    pattern_len;    /**< Pattern length */
} ipf_gap_t;

/**
 * @brief Data area within a track
 */
typedef struct {
    uint32_t    type;           /**< Data type (header/data/gap) */
    uint32_t    offset;         /**< Bit offset in track */
    uint32_t    size;           /**< Size in bits */
    uint32_t    flags;          /**< Data flags */
    uint8_t    *data;           /**< Raw data bytes */
    uint32_t    data_len;       /**< Data length in bytes */
    
    /* Fuzzy/weak bit info */
    uint8_t    *fuzzy_mask;     /**< Fuzzy bit mask (same size as data) */
    bool        has_fuzzy;      /**< Contains fuzzy bits */
} ipf_data_area_t;

/**
 * @brief Sector information
 */
typedef struct {
    uint8_t     cylinder;       /**< Cylinder number */
    uint8_t     head;           /**< Head (side) */
    uint8_t     sector;         /**< Sector number */
    uint8_t     size_code;      /**< Size code (0=128, 1=256, 2=512, 3=1024) */
    
    uint32_t    data_offset;    /**< Offset to data in track */
    uint32_t    data_size;      /**< Data size in bytes */
    uint32_t    flags;          /**< Sector flags */
    
    uint16_t    header_crc;     /**< Header CRC */
    uint16_t    data_crc;       /**< Data CRC */
    bool        crc_ok;         /**< CRC verification result */
    
    /* Gaps */
    ipf_gap_t   gaps[IPF_MAX_GAPS];
    uint8_t     gap_count;
} ipf_sector_t;

/**
 * @brief Track information
 */
typedef struct {
    uint8_t     cylinder;       /**< Cylinder number */
    uint8_t     head;           /**< Head (side) */
    uint8_t     sector_count;   /**< Number of sectors */
    
    /* Track timing */
    uint32_t    track_bits;     /**< Total track length in bits */
    uint32_t    start_bit;      /**< Start bit position */
    double      rpm;            /**< Estimated RPM */
    double      duration_us;    /**< Track duration in microseconds */
    
    /* Data areas */
    ipf_data_area_t data_areas[IPF_MAX_DATA_AREAS];
    uint8_t     data_area_count;
    
    /* Sectors */
    ipf_sector_t sectors[IPF_MAX_SECTORS];
    
    /* Raw encoded data */
    uint8_t    *raw_data;       /**< Raw MFM/GCR encoded data */
    uint32_t    raw_data_len;   /**< Length in bytes */
    
    /* CTRaw timing data */
    uint32_t   *cell_timings;   /**< Cell timing values */
    uint32_t    timing_count;   /**< Number of timing values */
    
    /* Protection flags */
    uint32_t    flags;          /**< Track flags */
    bool        has_fuzzy;      /**< Contains fuzzy bits */
    bool        has_weak;       /**< Contains weak bits */
    bool        has_timing;     /**< Has timing variations */
} ipf_track_t;

/**
 * @brief Image information (from INFO record)
 */
typedef struct {
    uint32_t    media_type;     /**< Media type */
    uint32_t    encoder_type;   /**< Encoder type */
    uint32_t    encoder_rev;    /**< Encoder revision */
    uint32_t    file_key;       /**< File key (unique ID) */
    uint32_t    file_rev;       /**< File revision */
    
    uint32_t    origin;         /**< Origin (SPS member ID) */
    uint32_t    min_cylinder;   /**< Minimum cylinder */
    uint32_t    max_cylinder;   /**< Maximum cylinder */
    uint32_t    min_head;       /**< Minimum head */
    uint32_t    max_head;       /**< Maximum head */
    
    char        date[16];       /**< Creation date */
    uint32_t    platform;       /**< Target platform */
    uint32_t    density;        /**< Disk density */
    
    bool        has_copy_protection; /**< Has copy protection */
} ipf_info_t;

/**
 * @brief IPF file context
 */
typedef struct {
    /* File info */
    char            path[1024];
    uint32_t        version;        /**< IPF version */
    
    /* Records */
    ipf_info_t      info;
    
    /* Tracks */
    uint8_t         track_count;
    ipf_track_t    *tracks;
    
    /* Statistics */
    uint32_t        total_sectors;
    uint32_t        bad_sectors;
    uint32_t        fuzzy_tracks;
    uint64_t        total_data_bytes;
    
    /* Internal */
    void           *file_data;
    size_t          file_size;
} ipf_context_t;

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Open IPF file
 * @param path Path to IPF file
 * @return Context pointer or NULL on error
 */
ipf_context_t *ipf_open(const char *path);

/**
 * @brief Close IPF file and free resources
 * @param ctx Context from ipf_open()
 */
void ipf_close(ipf_context_t *ctx);

/**
 * @brief Read track data
 * @param ctx Context
 * @param cylinder Cylinder number
 * @param head Head number (0 or 1)
 * @param track Output track data
 * @return IPF_OK on success
 */
ipf_error_t ipf_read_track(ipf_context_t *ctx,
                           uint8_t cylinder,
                           uint8_t head,
                           ipf_track_t *track);

/**
 * @brief Free track data
 * @param track Track from ipf_read_track()
 */
void ipf_free_track(ipf_track_t *track);

/**
 * @brief Read sector data
 * @param ctx Context
 * @param cylinder Cylinder
 * @param head Head
 * @param sector Sector number
 * @param data Output buffer
 * @param data_size Size of output buffer
 * @param out_size Actual data size
 * @return IPF_OK on success
 */
ipf_error_t ipf_read_sector(ipf_context_t *ctx,
                            uint8_t cylinder,
                            uint8_t head,
                            uint8_t sector,
                            uint8_t *data,
                            uint32_t data_size,
                            uint32_t *out_size);

/**
 * @brief Get image info
 * @param ctx Context
 * @param info Output info structure
 * @return IPF_OK on success
 */
ipf_error_t ipf_get_info(const ipf_context_t *ctx, ipf_info_t *info);

/**
 * @brief Check if file is valid IPF
 * @param path Path to file
 * @return true if valid IPF signature
 */
bool ipf_is_valid_file(const char *path);

/**
 * @brief Get error string
 * @param err Error code
 * @return Human-readable error string
 */
const char *ipf_error_string(ipf_error_t err);

/**
 * @brief Get platform name
 * @param platform Platform code
 * @return Human-readable platform name
 */
const char *ipf_platform_string(uint32_t platform);

/**
 * @brief Get density name
 * @param density Density code
 * @return Human-readable density name
 */
const char *ipf_density_string(uint32_t density);

/*============================================================================
 * CTRaw Support
 *============================================================================*/

/**
 * @brief Decode CTRaw timing data
 * @param track Track with CTRaw data
 * @param timings Output timing array (in nanoseconds)
 * @param max_timings Maximum timings
 * @param out_count Actual count
 * @return IPF_OK on success
 */
ipf_error_t ipf_decode_ctraw(const ipf_track_t *track,
                             double *timings,
                             uint32_t max_timings,
                             uint32_t *out_count);

/**
 * @brief Convert CTRaw to MFM bits
 * @param track Track with CTRaw data
 * @param bit_time_ns Nominal bit time in nanoseconds
 * @param mfm Output MFM data
 * @param max_bytes Maximum output bytes
 * @param out_bytes Actual byte count
 * @return IPF_OK on success
 */
ipf_error_t ipf_ctraw_to_mfm(const ipf_track_t *track,
                             double bit_time_ns,
                             uint8_t *mfm,
                             uint32_t max_bytes,
                             uint32_t *out_bytes);

/*============================================================================
 * Protection Analysis
 *============================================================================*/

/**
 * @brief Analyze track for copy protection
 * @param track Track data
 * @param protection_type Output: protection type detected
 * @param confidence Output: confidence level (0-100)
 * @return IPF_OK on success
 */
ipf_error_t ipf_analyze_protection(const ipf_track_t *track,
                                   uint32_t *protection_type,
                                   uint8_t *confidence);

/**
 * @brief Get fuzzy bits from track
 * @param track Track data
 * @param fuzzy_mask Output fuzzy bit mask
 * @param mask_size Size of mask buffer
 * @param fuzzy_count Output: number of fuzzy bits
 * @return IPF_OK on success
 */
ipf_error_t ipf_get_fuzzy_bits(const ipf_track_t *track,
                               uint8_t *fuzzy_mask,
                               size_t mask_size,
                               uint32_t *fuzzy_count);

/*============================================================================
 * Sector Utilities
 *============================================================================*/

/**
 * @brief Get sector size from size code
 * @param size_code Size code (0-3)
 * @return Sector size in bytes
 */
static inline uint32_t ipf_sector_size(uint8_t size_code) {
    return 128U << (size_code & 0x03);
}

/**
 * @brief Calculate CRC-16 CCITT
 * @param data Data buffer
 * @param len Data length
 * @return CRC value
 */
uint16_t ipf_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_PARSER_H */
