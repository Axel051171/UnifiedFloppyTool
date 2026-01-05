// SPDX-License-Identifier: MIT
/*
 * mfm_decode.c - Complete MFM Decoder for Amiga Disks
 * 
 * Implements full MFM (Modified Frequency Modulation) decoding
 * for Amiga floppy disks. Handles sync detection, sector decoding,
 * and checksum verification.
 * 
 * MFM Encoding:
 *   - Clock bits between data bits prevent long runs of 0s
 *   - Sync word: $4489 (clock suppressed pattern)
 *   - Amiga uses 11 sectors per track
 *   - Each sector: 544 bytes (16 header + 528 data)
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================*
 * MFM CONSTANTS
 *============================================================================*/

#define MFM_SYNC_WORD           0x4489  /* MFM sync pattern */
#define MFM_SYNC_MASK           0xFFFF
#define AMIGA_SECTOR_SIZE       512     /* Data bytes per sector */
#define AMIGA_SECTOR_HEADER     16      /* Header bytes */
#define AMIGA_SECTOR_TOTAL      544     /* Header + data encoded */
#define AMIGA_SECTORS_PER_TRACK 11      /* Standard Amiga */
#define AMIGA_TRACK_GAP         700     /* Minimum gap between sectors */

/*============================================================================*
 * MFM STRUCTURES
 *============================================================================*/

/**
 * @brief MFM decoded sector
 */
typedef struct {
    /* Position */
    uint32_t sync_position;     /* Position of sync mark in track */
    
    /* Header */
    uint8_t format_type;        /* Always 0xFF for Amiga */
    uint8_t track_number;       /* Physical track number */
    uint8_t sector_number;      /* Sector number (0-10) */
    uint8_t sectors_to_gap;     /* Sectors until gap (11 - sector) */
    
    /* Header checksum */
    uint32_t header_checksum_calculated;
    uint32_t header_checksum_stored;
    bool header_checksum_valid;
    
    /* Data */
    uint8_t data[AMIGA_SECTOR_SIZE];  /* Decoded sector data */
    
    /* Data checksum */
    uint32_t data_checksum_calculated;
    uint32_t data_checksum_stored;
    bool data_checksum_valid;
    
    /* Label (optional) */
    uint8_t label[16];
    bool has_label;
    
} mfm_sector_t;

/**
 * @brief MFM track analysis
 */
typedef struct {
    uint32_t sync_positions[20];  /* Positions of sync marks */
    int sync_count;               /* Number of syncs found */
    
    mfm_sector_t sectors[20];     /* Decoded sectors */
    int sector_count;             /* Number of valid sectors */
    
    uint32_t track_length;        /* Total track length */
    uint32_t min_gap;             /* Minimum gap between sectors */
    uint32_t max_gap;             /* Maximum gap between sectors */
    
    int crc_errors;               /* Number of CRC errors */
    bool has_long_track;          /* Long track detected */
    
} mfm_track_t;

/*============================================================================*
 * MFM BIT MANIPULATION
 *============================================================================*/

/**
 * @brief Decode MFM word (2 bytes) to byte
 * 
 * MFM encoding uses odd/even bits:
 * - Odd bits (1,3,5,7,9,11,13,15) = data bits
 * - Even bits (0,2,4,6,8,10,12,14) = clock bits
 * 
 * We extract odd bits to get original data.
 */
static inline uint8_t mfm_decode_byte(uint16_t odd, uint16_t even)
{
    uint8_t result = 0;
    
    /* Extract odd bits from odd word */
    for (int i = 0; i < 8; i++) {
        if (odd & (1 << (i * 2 + 1))) {
            result |= (1 << (7 - i));
        }
    }
    
    return result;
}

/**
 * @brief Decode MFM long word (4 bytes) to 2 bytes
 */
static inline uint16_t mfm_decode_word(uint32_t mfm_long)
{
    uint16_t odd = (mfm_long >> 16) & 0xFFFF;
    uint16_t even = mfm_long & 0xFFFF;
    
    uint16_t result = 0;
    result |= (mfm_decode_byte(odd >> 8, even >> 8) << 8);
    result |= mfm_decode_byte(odd & 0xFF, even & 0xFF);
    
    return result;
}

/**
 * @brief Decode MFM buffer
 * 
 * Decodes MFM encoded data buffer to plain data.
 * Input is big-endian MFM encoded longs.
 */
static int mfm_decode_buffer(
    const uint8_t *mfm_data,
    size_t mfm_len,
    uint8_t *decoded_out,
    size_t decoded_max
) {
    if (!mfm_data || !decoded_out || mfm_len < 4) {
        return -1;
    }
    
    size_t decoded_len = 0;
    
    for (size_t i = 0; i < mfm_len && decoded_len < decoded_max; i += 4) {
        if (i + 3 >= mfm_len) break;
        
        /* Read big-endian long */
        uint32_t mfm_long = ((uint32_t)mfm_data[i] << 24) |
                           ((uint32_t)mfm_data[i+1] << 16) |
                           ((uint32_t)mfm_data[i+2] << 8) |
                           ((uint32_t)mfm_data[i+3]);
        
        /* Decode to word */
        uint16_t word = mfm_decode_word(mfm_long);
        
        /* Store as bytes */
        if (decoded_len < decoded_max) {
            decoded_out[decoded_len++] = (word >> 8) & 0xFF;
        }
        if (decoded_len < decoded_max) {
            decoded_out[decoded_len++] = word & 0xFF;
        }
    }
    
    return (int)decoded_len;
}

/*============================================================================*
 * CHECKSUM CALCULATION
 *============================================================================*/

/**
 * @brief Calculate Amiga MFM checksum
 * 
 * Amiga checksum is XOR of all long words.
 */
static uint32_t mfm_calculate_checksum(const uint8_t *data, size_t len)
{
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < len; i += 4) {
        if (i + 3 >= len) break;
        
        uint32_t long_word = ((uint32_t)data[i] << 24) |
                            ((uint32_t)data[i+1] << 16) |
                            ((uint32_t)data[i+2] << 8) |
                            ((uint32_t)data[i+3]);
        
        checksum ^= long_word;
    }
    
    return checksum;
}

/**
 * @brief Verify sector header checksum
 */
static bool mfm_verify_header_checksum(
    const uint8_t *header_mfm,
    size_t header_len
) {
    if (header_len < 8) return false;
    
    /* First 4 bytes are checksum, rest is header data */
    uint32_t stored_checksum = mfm_calculate_checksum(header_mfm, 4);
    uint32_t data_checksum = mfm_calculate_checksum(header_mfm + 4, header_len - 4);
    
    /* Checksum of checksum XOR data checksum should be 0 */
    return (stored_checksum ^ data_checksum) == 0;
}

/*============================================================================*
 * SYNC DETECTION
 *============================================================================*/

/**
 * @brief Find MFM sync word in data
 * 
 * Searches for $4489 sync pattern in MFM data.
 * Returns position of first sync found.
 */
int mfm_find_sync(const uint8_t *data, size_t len, size_t *pos_out)
{
    if (!data || !pos_out || len < 2) {
        return -1;
    }
    
    for (size_t i = 0; i < len - 1; i++) {
        uint16_t word = ((uint16_t)data[i] << 8) | data[i + 1];
        
        if (word == MFM_SYNC_WORD) {
            *pos_out = i;
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

/**
 * @brief Find all MFM sync words in track
 * 
 * Searches for all sync patterns and returns their positions.
 */
int mfm_find_all_syncs(
    const uint8_t *data,
    size_t len,
    size_t *positions,
    int max_syncs
) {
    if (!data || !positions || max_syncs <= 0) {
        return -1;
    }
    
    int count = 0;
    size_t pos = 0;
    
    while (pos < len - 1 && count < max_syncs) {
        uint16_t word = ((uint16_t)data[pos] << 8) | data[pos + 1];
        
        if (word == MFM_SYNC_WORD) {
            positions[count++] = pos;
            pos += 2;  /* Skip this sync */
        } else {
            pos++;
        }
    }
    
    return count;
}

/*============================================================================*
 * SECTOR DECODING
 *============================================================================*/

/**
 * @brief Decode Amiga MFM sector
 * 
 * Decodes a complete sector from MFM data starting at given offset.
 * 
 * Sector structure (MFM encoded):
 *   $4489 - Sync word
 *   $4489 - Sync word (repeated)
 *   Long  - Header checksum
 *   Long  - Data checksum
 *   Byte  - Format (0xFF)
 *   Byte  - Track number
 *   Byte  - Sector number
 *   Byte  - Sectors to gap
 *   16*Long - Optional label
 *   512 bytes - Sector data
 */
int mfm_decode_sector(
    const uint8_t *track_data,
    size_t track_len,
    size_t sync_offset,
    mfm_sector_t *sector_out
) {
    if (!track_data || !sector_out || sync_offset >= track_len) {
        return -1;
    }
    
    memset(sector_out, 0, sizeof(*sector_out));
    sector_out->sync_position = sync_offset;
    
    const uint8_t *ptr = track_data + sync_offset;
    size_t remaining = track_len - sync_offset;
    
    /* Skip sync words (2 * 2 bytes) */
    if (remaining < 4) return -1;
    ptr += 4;
    remaining -= 4;
    
    /* Read header (8 bytes MFM = 4 bytes decoded) */
    if (remaining < 8) return -1;
    
    uint8_t header_decoded[4];
    if (mfm_decode_buffer(ptr, 8, header_decoded, 4) < 4) {
        return -1;
    }
    
    sector_out->format_type = header_decoded[0];
    sector_out->track_number = header_decoded[1];
    sector_out->sector_number = header_decoded[2];
    sector_out->sectors_to_gap = header_decoded[3];
    
    /* Verify header checksum */
    sector_out->header_checksum_valid = mfm_verify_header_checksum(ptr, 8);
    
    ptr += 8;
    remaining -= 8;
    
    /* Skip label area (32 bytes MFM) */
    if (remaining < 32) return -1;
    ptr += 32;
    remaining -= 32;
    
    /* Read data checksum (4 bytes MFM) */
    if (remaining < 4) return -1;
    sector_out->data_checksum_stored = mfm_calculate_checksum(ptr, 4);
    ptr += 4;
    remaining -= 4;
    
    /* Read sector data (1024 bytes MFM = 512 bytes decoded) */
    if (remaining < 1024) return -1;
    
    int decoded = mfm_decode_buffer(ptr, 1024, sector_out->data, AMIGA_SECTOR_SIZE);
    if (decoded != AMIGA_SECTOR_SIZE) {
        return -1;
    }
    
    /* Calculate data checksum */
    sector_out->data_checksum_calculated = mfm_calculate_checksum(
        sector_out->data, AMIGA_SECTOR_SIZE
    );
    
    sector_out->data_checksum_valid = 
        (sector_out->data_checksum_calculated == sector_out->data_checksum_stored);
    
    return 0;
}

/*============================================================================*
 * TRACK ANALYSIS
 *============================================================================*/

/**
 * @brief Analyze complete MFM track
 * 
 * Finds all sectors, verifies checksums, detects anomalies.
 */
int mfm_analyze_track(
    const uint8_t *track_data,
    size_t track_len,
    mfm_track_t *track_out
) {
    if (!track_data || !track_out) {
        return -1;
    }
    
    memset(track_out, 0, sizeof(*track_out));
    track_out->track_length = track_len;
    track_out->min_gap = 0xFFFFFFFF;
    track_out->max_gap = 0;
    
    /* Find all sync marks */
    track_out->sync_count = mfm_find_all_syncs(
        track_data, track_len,
        track_out->sync_positions, 20
    );
    
    if (track_out->sync_count < 0) {
        return -1;
    }
    
    /* Decode each sector */
    track_out->sector_count = 0;
    
    for (int i = 0; i < track_out->sync_count && 
         track_out->sector_count < 20; i++) {
        
        mfm_sector_t *sector = &track_out->sectors[track_out->sector_count];
        
        if (mfm_decode_sector(track_data, track_len,
                             track_out->sync_positions[i],
                             sector) == 0) {
            
            track_out->sector_count++;
            
            /* Count CRC errors */
            if (!sector->header_checksum_valid || 
                !sector->data_checksum_valid) {
                track_out->crc_errors++;
            }
            
            /* Calculate gap to next sector */
            if (i < track_out->sync_count - 1) {
                uint32_t gap = track_out->sync_positions[i + 1] - 
                              track_out->sync_positions[i];
                
                if (gap < track_out->min_gap) {
                    track_out->min_gap = gap;
                }
                if (gap > track_out->max_gap) {
                    track_out->max_gap = gap;
                }
            }
        }
    }
    
    /* Detect long track */
    uint32_t expected_length = AMIGA_SECTORS_PER_TRACK * AMIGA_SECTOR_TOTAL + 
                              AMIGA_TRACK_GAP * AMIGA_SECTORS_PER_TRACK;
    
    if (track_len > expected_length * 1.15) {
        track_out->has_long_track = true;
    }
    
    return 0;
}

/*============================================================================*
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get sector by sector number
 */
mfm_sector_t* mfm_track_get_sector(mfm_track_t *track, int sector_num)
{
    if (!track || sector_num < 0 || sector_num >= 11) {
        return NULL;
    }
    
    for (int i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].sector_number == sector_num) {
            return &track->sectors[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Print track analysis
 */
void mfm_print_track_analysis(const mfm_track_t *track)
{
    if (!track) return;
    
    printf("MFM Track Analysis:\n");
    printf("  Track length: %u bytes\n", track->track_length);
    printf("  Sync marks:   %d\n", track->sync_count);
    printf("  Sectors:      %d\n", track->sector_count);
    printf("  CRC errors:   %d\n", track->crc_errors);
    printf("  Gap range:    %u - %u bytes\n", track->min_gap, track->max_gap);
    printf("  Long track:   %s\n", track->has_long_track ? "YES" : "NO");
    
    if (track->sector_count > 0) {
        printf("\n  Sector Details:\n");
        for (int i = 0; i < track->sector_count; i++) {
            const mfm_sector_t *s = &track->sectors[i];
            printf("    Sector %d: Track %d, Pos %u, Header %s, Data %s\n",
                   s->sector_number,
                   s->track_number,
                   s->sync_position,
                   s->header_checksum_valid ? "OK" : "ERR",
                   s->data_checksum_valid ? "OK" : "ERR");
        }
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
