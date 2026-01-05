/**
 * @file uft_a2r_parser.h
 * @brief A2R Apple II Flux Format Parser
 * 
 * Parser for A2R format (Applesauce project) - Apple II flux preservation.
 * Supports A2R v2 and v3 formats with:
 * - Flux timing data in 125ns resolution
 * - Quarter-track support (0.25 track stepping)
 * - Multi-capture per track for weak bit analysis
 * - Metadata extraction
 * - Solved (decoded) nibble data (v3)
 * 
 * @author UFT Team
 * @version 3.4.0
 * @date 2026-01-03
 * 
 * References:
 * - https://applesaucefdc.com/a2r/
 * - A2R v2 Specification
 * - A2R v3 Specification
 */

#ifndef UFT_A2R_PARSER_H
#define UFT_A2R_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** A2R file magic bytes */
#define A2R_MAGIC_V2        "A2R2"
#define A2R_MAGIC_V3        "A2R3"
#define A2R_MAGIC_LEN       4
#define A2R_HEADER_SUFFIX   "\xFF\n\r\n"
#define A2R_HEADER_SIZE     8

/** Chunk identifiers */
#define A2R_CHUNK_INFO      "INFO"
#define A2R_CHUNK_STRM      "STRM"  /* v2: flux stream */
#define A2R_CHUNK_META      "META"
#define A2R_CHUNK_RWCP      "RWCP"  /* v3: raw capture */
#define A2R_CHUNK_SLVD      "SLVD"  /* v3: solved data */

/** Disk types */
#define A2R_DISK_525_SS     1   /* 5.25" SS (Disk II) */
#define A2R_DISK_525_DS     2   /* 5.25" DS */
#define A2R_DISK_35_SS      3   /* 3.5" SS (400K) */
#define A2R_DISK_35_DS      4   /* 3.5" DS (800K) */

/** Timing resolution: 125ns per tick (8 MHz sample rate) */
#define A2R_TICK_NS         125
#define A2R_SAMPLE_RATE_HZ  8000000

/** Maximum values */
#define A2R_MAX_TRACKS      160     /* Quarter tracks: 40 * 4 */
#define A2R_MAX_CAPTURES    32      /* Max captures per track */
#define A2R_MAX_META_SIZE   65536   /* Max metadata size */

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    A2R_OK = 0,
    A2R_ERR_NULL_PARAM,
    A2R_ERR_FILE_OPEN,
    A2R_ERR_FILE_READ,
    A2R_ERR_BAD_MAGIC,
    A2R_ERR_UNSUPPORTED_VERSION,
    A2R_ERR_BAD_CHUNK,
    A2R_ERR_NO_INFO,
    A2R_ERR_NO_FLUX,
    A2R_ERR_TRACK_RANGE,
    A2R_ERR_CAPTURE_RANGE,
    A2R_ERR_ALLOC,
    A2R_ERR_CORRUPT,
    A2R_ERR_COUNT
} a2r_error_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief INFO chunk data
 */
typedef struct {
    uint8_t     version;            /**< Format version (2 or 3) */
    char        creator[33];        /**< Creator string (32 chars + null) */
    uint8_t     disk_type;          /**< Disk type (A2R_DISK_*) */
    bool        write_protected;    /**< Disk write protected */
    bool        synchronized;       /**< Tracks are synchronized */
    bool        cleaned;            /**< Flux cleaned (v3) */
    bool        optimal_timing;     /**< Optimal bit timing (v3) */
    
    /* v3 extended fields */
    uint8_t     disk_sides;         /**< Number of sides */
    uint8_t     boot_sector_format; /**< Boot sector format */
    uint8_t     data_format;        /**< Data format */
    uint32_t    optimal_bit_timing; /**< Optimal bit timing in ns (v3) */
    uint16_t    compatible_hw;      /**< Compatible hardware flags (v3) */
    uint16_t    required_ram;       /**< Required RAM in KB (v3) */
    uint16_t    largest_track;      /**< Largest track size (v3) */
} a2r_info_t;

/**
 * @brief Single flux capture data
 */
typedef struct {
    uint8_t     capture_type;       /**< Capture type (1=timing, 2=bits, 3=xtiming) */
    uint32_t    data_length;        /**< Length of flux data in bytes */
    uint32_t    tick_count;         /**< Number of timing ticks */
    uint8_t    *data;               /**< Flux timing data (caller-owns after read) */
    
    /* Derived values */
    double      duration_us;        /**< Track duration in microseconds */
    double      rpm;                /**< Estimated RPM */
} a2r_capture_t;

/**
 * @brief Track data with multiple captures
 */
typedef struct {
    uint8_t         track_number;       /**< Quarter track (0-159 for 5.25") */
    uint8_t         side;               /**< Side (0 or 1) */
    uint8_t         capture_count;      /**< Number of captures */
    a2r_capture_t   captures[A2R_MAX_CAPTURES];
    
    /* Solved data (v3 SLVD chunk) */
    bool            has_solved;         /**< Has solved nibble data */
    uint8_t        *nibbles;            /**< Decoded nibbles */
    uint32_t        nibble_count;       /**< Number of nibbles */
} a2r_track_t;

/**
 * @brief Metadata entry
 */
typedef struct {
    char    key[64];
    char    value[256];
} a2r_meta_entry_t;

/**
 * @brief A2R file context
 */
typedef struct {
    /* File info */
    char            path[1024];
    uint8_t         version;            /**< A2R version (2 or 3) */
    
    /* Chunks */
    a2r_info_t      info;
    
    /* Track data */
    uint8_t         track_count;
    a2r_track_t    *tracks;             /**< Array of tracks */
    
    /* Metadata */
    uint16_t        meta_count;
    a2r_meta_entry_t *metadata;
    
    /* Statistics */
    uint32_t        total_captures;
    uint64_t        total_flux_bytes;
    double          min_rpm;
    double          max_rpm;
    
    /* Internal */
    void           *file_data;          /**< Mapped file data */
    size_t          file_size;
} a2r_context_t;

/**
 * @brief Decoded flux sample
 */
typedef struct {
    uint32_t    tick;           /**< Tick value (125ns units) */
    double      time_ns;        /**< Absolute time in nanoseconds */
    bool        is_extended;    /**< Extended timing value */
} a2r_flux_sample_t;

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Open A2R file
 * @param path Path to A2R file
 * @return Context pointer or NULL on error
 */
a2r_context_t *a2r_open(const char *path);

/**
 * @brief Close A2R file and free resources
 * @param ctx Context from a2r_open()
 */
void a2r_close(a2r_context_t *ctx);

/**
 * @brief Read track data
 * @param ctx Context
 * @param quarter_track Quarter track number (0-159)
 * @param side Side number (0 or 1)
 * @param track Output track data
 * @return A2R_OK on success
 */
a2r_error_t a2r_read_track(a2r_context_t *ctx, 
                           uint8_t quarter_track, 
                           uint8_t side,
                           a2r_track_t *track);

/**
 * @brief Free track data
 * @param track Track from a2r_read_track()
 */
void a2r_free_track(a2r_track_t *track);

/**
 * @brief Get flux samples from capture
 * @param capture Capture data
 * @param samples Output sample array (caller allocates)
 * @param max_samples Maximum samples to decode
 * @param out_count Actual number of samples decoded
 * @return A2R_OK on success
 */
a2r_error_t a2r_decode_flux(const a2r_capture_t *capture,
                            a2r_flux_sample_t *samples,
                            uint32_t max_samples,
                            uint32_t *out_count);

/**
 * @brief Convert flux to bit cells (Apple II GCR)
 * @param capture Capture data
 * @param bit_time_ns Nominal bit time in nanoseconds (default: 4000)
 * @param nibbles Output nibble buffer
 * @param max_nibbles Maximum nibbles
 * @param out_count Actual nibble count
 * @return A2R_OK on success
 */
a2r_error_t a2r_flux_to_nibbles(const a2r_capture_t *capture,
                                double bit_time_ns,
                                uint8_t *nibbles,
                                uint32_t max_nibbles,
                                uint32_t *out_count);

/**
 * @brief Get file info
 * @param ctx Context
 * @param info Output info structure
 * @return A2R_OK on success
 */
a2r_error_t a2r_get_info(const a2r_context_t *ctx, a2r_info_t *info);

/**
 * @brief Get metadata value by key
 * @param ctx Context
 * @param key Metadata key
 * @param value Output buffer
 * @param value_size Size of output buffer
 * @return A2R_OK if found
 */
a2r_error_t a2r_get_metadata(const a2r_context_t *ctx,
                             const char *key,
                             char *value,
                             size_t value_size);

/**
 * @brief Check if file is valid A2R
 * @param path Path to file
 * @return true if valid A2R signature
 */
bool a2r_is_valid_file(const char *path);

/**
 * @brief Get A2R file version
 * @param path Path to file
 * @return Version (2 or 3) or 0 on error
 */
uint8_t a2r_get_file_version(const char *path);

/**
 * @brief Get error string
 * @param err Error code
 * @return Human-readable error string
 */
const char *a2r_error_string(a2r_error_t err);

/**
 * @brief Get disk type name
 * @param disk_type Disk type code
 * @return Human-readable disk type
 */
const char *a2r_disk_type_string(uint8_t disk_type);

/**
 * @brief Convert quarter track to whole track + fraction
 * @param quarter_track Quarter track number
 * @param track Output whole track
 * @param quarter Output quarter (0-3)
 */
static inline void a2r_quarter_to_track(uint8_t quarter_track,
                                        uint8_t *track,
                                        uint8_t *quarter) {
    if (track) *track = quarter_track / 4;
    if (quarter) *quarter = quarter_track % 4;
}

/**
 * @brief Convert whole track + fraction to quarter track
 * @param track Whole track
 * @param quarter Quarter (0-3)
 * @return Quarter track number
 */
static inline uint8_t a2r_track_to_quarter(uint8_t track, uint8_t quarter) {
    return (track * 4) + (quarter & 0x03);
}

/**
 * @brief Calculate RPM from track duration
 * @param duration_us Track duration in microseconds
 * @return Estimated RPM
 */
static inline double a2r_duration_to_rpm(double duration_us) {
    if (duration_us <= 0.0) return 0.0;
    return 60000000.0 / duration_us;
}

/*============================================================================
 * Advanced API
 *============================================================================*/

/**
 * @brief Get raw flux timing data for analysis
 * @param capture Capture data
 * @param timings Output timing array (in nanoseconds)
 * @param max_timings Maximum timings
 * @param out_count Actual count
 * @return A2R_OK on success
 */
a2r_error_t a2r_get_raw_timings(const a2r_capture_t *capture,
                                double *timings,
                                uint32_t max_timings,
                                uint32_t *out_count);

/**
 * @brief Fuse multiple captures for weak bit detection
 * @param captures Array of captures
 * @param count Number of captures
 * @param fused Output fused capture
 * @param weak_mask Output weak bit mask (caller allocates)
 * @param mask_size Size of weak mask buffer
 * @return A2R_OK on success
 */
a2r_error_t a2r_fuse_captures(const a2r_capture_t *captures,
                              uint8_t count,
                              a2r_capture_t *fused,
                              uint8_t *weak_mask,
                              size_t mask_size);

/**
 * @brief Analyze track for copy protection
 * @param track Track data
 * @param has_weak Output: has weak bits
 * @param has_timing Output: has timing protection
 * @param has_halftrack Output: uses half/quarter tracks
 * @return A2R_OK on success
 */
a2r_error_t a2r_analyze_protection(const a2r_track_t *track,
                                   bool *has_weak,
                                   bool *has_timing,
                                   bool *has_halftrack);

#ifdef __cplusplus
}
#endif

#endif /* UFT_A2R_PARSER_H */
