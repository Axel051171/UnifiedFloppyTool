// SPDX-License-Identifier: MIT
/*
 * mfm_decode.h - MFM Decoder Header
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#ifndef MFM_DECODE_H
#define MFM_DECODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * MFM CONSTANTS
 *============================================================================*/

#define MFM_SYNC_WORD           0x4489
#define AMIGA_SECTOR_SIZE       512
#define AMIGA_SECTORS_PER_TRACK 11

/*============================================================================*
 * MFM STRUCTURES
 *============================================================================*/

/**
 * @brief MFM decoded sector
 */
typedef struct {
    uint32_t sync_position;
    
    uint8_t format_type;
    uint8_t track_number;
    uint8_t sector_number;
    uint8_t sectors_to_gap;
    
    uint32_t header_checksum_calculated;
    uint32_t header_checksum_stored;
    bool header_checksum_valid;
    
    uint8_t data[AMIGA_SECTOR_SIZE];
    
    uint32_t data_checksum_calculated;
    uint32_t data_checksum_stored;
    bool data_checksum_valid;
    
    uint8_t label[16];
    bool has_label;
    
} mfm_sector_t;

/**
 * @brief MFM track analysis
 */
typedef struct {
    uint32_t sync_positions[20];
    int sync_count;
    
    mfm_sector_t sectors[20];
    int sector_count;
    
    uint32_t track_length;
    uint32_t min_gap;
    uint32_t max_gap;
    
    int crc_errors;
    bool has_long_track;
    
} mfm_track_t;

/*============================================================================*
 * SYNC DETECTION
 *============================================================================*/

/**
 * @brief Find MFM sync word
 * 
 * @param data Track data
 * @param len Data length
 * @param pos_out Position (output)
 * @return 0 on success
 */
int mfm_find_sync(const uint8_t *data, size_t len, size_t *pos_out);

/**
 * @brief Find all MFM sync words
 * 
 * @param data Track data
 * @param len Data length
 * @param positions Position array (output)
 * @param max_syncs Maximum syncs
 * @return Number of syncs found
 */
int mfm_find_all_syncs(
    const uint8_t *data,
    size_t len,
    size_t *positions,
    int max_syncs
);

/*============================================================================*
 * SECTOR DECODING
 *============================================================================*/

/**
 * @brief Decode MFM sector
 * 
 * @param track_data Track data
 * @param track_len Track length
 * @param sync_offset Sync position
 * @param sector_out Sector (output)
 * @return 0 on success
 */
int mfm_decode_sector(
    const uint8_t *track_data,
    size_t track_len,
    size_t sync_offset,
    mfm_sector_t *sector_out
);

/*============================================================================*
 * TRACK ANALYSIS
 *============================================================================*/

/**
 * @brief Analyze MFM track
 * 
 * @param track_data Track data
 * @param track_len Track length
 * @param track_out Track analysis (output)
 * @return 0 on success
 */
int mfm_analyze_track(
    const uint8_t *track_data,
    size_t track_len,
    mfm_track_t *track_out
);

/**
 * @brief Get sector by number
 * 
 * @param track Track analysis
 * @param sector_num Sector number
 * @return Sector pointer or NULL
 */
mfm_sector_t* mfm_track_get_sector(mfm_track_t *track, int sector_num);

/**
 * @brief Print track analysis
 * 
 * @param track Track analysis
 */
void mfm_print_track_analysis(const mfm_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* MFM_DECODE_H */
