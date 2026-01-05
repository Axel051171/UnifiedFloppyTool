/**
 * @file uft_stx_parser.h
 * @brief Atari ST STX (Pasti) Parser - Full Implementation
 * @version 3.3.2
 * @date 2025-01-03
 *
 * STX is the preservation format for Atari ST disk images,
 * created by the Pasti project. It preserves:
 * - Exact sector data and CRC status
 * - Fuzzy bits (for copy protection)
 * - Timing information
 * - Full track images
 */

#ifndef UFT_STX_PARSER_H
#define UFT_STX_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define STX_MAX_SECTORS 32

/* Sector flags */
#define STX_SF_FUZZY        0x80
#define STX_SF_CRC_ERROR    0x08
#define STX_SF_DELETED      0x04
#define STX_SF_ID_CRC_ERROR 0x02

/*============================================================================
 * TYPES
 *============================================================================*/

/* Opaque parser context */
typedef struct stx_parser_ctx stx_parser_ctx_t;

/* Parsed sector */
typedef struct {
    /* ID field */
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint16_t size_bytes;
    
    /* Position and timing */
    uint32_t bit_position;
    uint32_t read_time_us;
    
    /* Status */
    bool     crc_error;
    bool     id_crc_error;
    bool     deleted;
    bool     has_fuzzy;
    uint8_t  fdc_status;
    
    /* Data (caller must not free) */
    uint8_t* data;
    uint8_t* fuzzy_mask;
} stx_sector_t;

/* Parsed track */
typedef struct {
    int track_number;
    int side;
    int sector_count;
    
    /* Track image */
    uint8_t*  track_data;
    size_t    track_data_size;
    uint32_t  track_length_bits;
    
    /* Timing data */
    uint16_t* timing_data;
    size_t    timing_count;
    
    /* Fuzzy bits */
    uint8_t*  fuzzy_data;
    size_t    fuzzy_size;
    uint32_t  fuzzy_bit_count;
    
    /* Sectors */
    stx_sector_t sectors[STX_MAX_SECTORS];
    
    /* Flags */
    bool has_track_image;
    bool has_timing;
    bool has_fuzzy;
    
    /* Statistics */
    int   good_sectors;
    int   bad_sectors;
    float quality_percent;
} stx_track_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open STX file
 * @param path Path to STX file
 * @return Parser context or NULL on error
 */
stx_parser_ctx_t* stx_parser_open(const char* path);

/**
 * @brief Close STX parser
 * @param ctx Pointer to context pointer (set to NULL)
 */
void stx_parser_close(stx_parser_ctx_t** ctx);

/**
 * @brief Read and parse track
 * @param ctx Parser context
 * @param track_num Track number (0-83)
 * @param side Side/head (0 or 1)
 * @param track_out Output track (free with stx_parser_free_track)
 * @return 0 on success, -1 on error
 */
int stx_parser_read_track(
    stx_parser_ctx_t* ctx,
    int track_num,
    int side,
    stx_track_t** track_out
);

/**
 * @brief Free track data
 * @param track Pointer to track pointer (set to NULL)
 */
void stx_parser_free_track(stx_track_t** track);

/**
 * @brief Read single sector
 * @param ctx Parser context
 * @param track_num Track number
 * @param side Side/head
 * @param sector_num Sector number (1-based typically)
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes read, or -1 on error
 */
int stx_parser_read_sector(
    stx_parser_ctx_t* ctx,
    int track_num,
    int side,
    int sector_num,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Get disk info
 * @param ctx Parser context
 * @param track_count Output: number of tracks (may be NULL)
 * @param version Output: format version (may be NULL)
 * @param tool_version Output: tool version (may be NULL)
 */
void stx_parser_get_info(
    stx_parser_ctx_t* ctx,
    int* track_count,
    uint16_t* version,
    uint16_t* tool_version
);

/**
 * @brief Get statistics
 * @param ctx Parser context
 * @param total_sectors Output: total sectors read (may be NULL)
 * @param fuzzy_sectors Output: sectors with fuzzy bits (may be NULL)
 * @param crc_errors Output: sectors with CRC errors (may be NULL)
 */
void stx_parser_get_stats(
    stx_parser_ctx_t* ctx,
    uint32_t* total_sectors,
    uint32_t* fuzzy_sectors,
    uint32_t* crc_errors
);

/**
 * @brief Analyze copy protection
 * @param ctx Parser context
 * @param report Output buffer for report text
 * @param report_size Report buffer size
 * @return Bytes written, or -1 on error
 */
int stx_parser_analyze_protection(
    stx_parser_ctx_t* ctx,
    char* report,
    size_t report_size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_STX_PARSER_H */
