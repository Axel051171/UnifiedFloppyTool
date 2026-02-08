/**
 * @file uft_edsk_parser.h
 * @brief Extended DSK Parser for Amstrad CPC/Spectrum
 * @version 3.3.2
 * @date 2025-01-03
 *
 * Supports:
 * - Standard DSK format
 * - Extended DSK (EDSK) format
 * - Variable sector sizes
 * - Weak/random sectors
 * - CRC error preservation
 * - Deleted data marks
 */

#ifndef UFT_EDSK_PARSER_H
#define UFT_EDSK_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define EDSK_MAX_SECTORS 29

/* FDC status flags */
#define FDC_ST1_DE       0x20    /* CRC error */
#define FDC_ST1_ND       0x04    /* No data */
#define FDC_ST2_CM       0x40    /* Deleted mark */
#define FDC_ST2_DD       0x20    /* Data error */

/*============================================================================
 * TYPES
 *============================================================================*/

/* Opaque parser context */
typedef struct edsk_parser_ctx edsk_parser_ctx_t;

/* Parsed sector */
typedef struct {
    /* ID field */
    uint8_t  id_track;
    uint8_t  id_side;
    uint8_t  id_sector;
    uint8_t  id_size;
    uint16_t actual_size;
    
    /* FDC status */
    uint8_t  fdc_st1;
    uint8_t  fdc_st2;
    
    /* Flags */
    bool     crc_error;
    bool     deleted;
    bool     no_data;
    bool     weak;
    
    /* Data (caller must not free) */
    uint8_t* data;
    uint8_t* weak_data;
    int      weak_copies;
} edsk_sector_t;

/* Parsed track */
typedef struct {
    int track_number;
    int side;
    int sector_count;
    
    /* Track parameters */
    uint8_t sector_size_code;
    uint8_t gap3_length;
    uint8_t filler_byte;
    
    /* Sectors */
    edsk_sector_t sectors[EDSK_MAX_SECTORS];
    
    /* Statistics */
    int   good_sectors;
    int   bad_sectors;
    int   weak_sectors;
    int   deleted_sectors;
    float quality_percent;
} edsk_track_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open DSK/EDSK file
 * @param path Path to DSK file
 * @return Parser context or NULL on error
 */
edsk_parser_ctx_t* edsk_parser_open(const char* path);

/**
 * @brief Close EDSK parser
 * @param ctx Pointer to context pointer (set to NULL)
 */
void edsk_parser_close(edsk_parser_ctx_t** ctx);

/**
 * @brief Read and parse track
 * @param ctx Parser context
 * @param track_num Track number
 * @param side Side (0 or 1)
 * @param track_out Output track (free with edsk_parser_free_track)
 * @return 0 on success, -1 on error
 */
int edsk_parser_read_track(
    edsk_parser_ctx_t* ctx,
    int track_num,
    int side,
    edsk_track_t** track_out
);

/**
 * @brief Free track data
 * @param track Pointer to track pointer (set to NULL)
 */
void edsk_parser_free_track(edsk_track_t** track);

/**
 * @brief Read single sector
 * @param ctx Parser context
 * @param track_num Track number
 * @param side Side
 * @param sector_num Sector ID
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes read, or -1 on error
 */
int edsk_parser_read_sector(
    edsk_parser_ctx_t* ctx,
    int track_num,
    int side,
    int sector_num,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Get disk info
 * @param ctx Parser context
 * @param num_tracks Output: number of tracks (may be NULL)
 * @param num_sides Output: number of sides (may be NULL)
 * @param is_extended Output: true if EDSK format (may be NULL)
 * @param creator Output: creator string (may be NULL)
 * @param creator_size Size of creator buffer
 */
void edsk_parser_get_info(
    edsk_parser_ctx_t* ctx,
    int* num_tracks,
    int* num_sides,
    bool* is_extended,
    char* creator,
    size_t creator_size
);

/**
 * @brief Get statistics
 * @param ctx Parser context
 * @param total_sectors Output: total sectors (may be NULL)
 * @param crc_errors Output: CRC errors (may be NULL)
 * @param weak_sectors Output: weak sectors (may be NULL)
 * @param deleted_sectors Output: deleted sectors (may be NULL)
 */
void edsk_parser_get_stats(
    edsk_parser_ctx_t* ctx,
    uint32_t* total_sectors,
    uint32_t* crc_errors,
    uint32_t* weak_sectors,
    uint32_t* deleted_sectors
);

/**
 * @brief Analyze disk format and protection
 * @param ctx Parser context
 * @param report Output buffer
 * @param report_size Buffer size
 * @return Bytes written, or -1 on error
 */
int edsk_parser_analyze_format(
    edsk_parser_ctx_t* ctx,
    char* report,
    size_t report_size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EDSK_PARSER_H */
