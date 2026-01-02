// SPDX-License-Identifier: MIT
/*
 * woz1_reader.h - Applesauce WOZ 1.0 Format Reader
 * 
 * Reads WOZ 1.0 bitstream disk images.
 * Bitstream normalized to 4µs intervals.
 * 
 * Format Specification:
 *   https://applesaucefdc.com/woz/reference1/
 * 
 * @version 2.8.1
 * @date 2024-12-26
 */

#ifndef WOZ1_READER_H
#define WOZ1_READER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * WOZ1 STRUCTURES
 *============================================================================*/

/**
 * @brief WOZ1 track data
 */
typedef struct {
    uint8_t  bitstream[6646];       /* Track bitstream */
    uint16_t bytes_used;            /* Bytes actually used */
    uint16_t bit_count;             /* Number of bits */
    uint16_t splice_point;          /* Splice point */
    uint8_t  splice_nibble;
    uint8_t  splice_bit_count;
} woz1_track_t;

/**
 * @brief WOZ1 disk image
 */
typedef struct {
    uint8_t info_version;
    uint8_t disk_type;              /* 1=5.25", 2=3.5" */
    uint8_t write_protected;
    uint8_t synchronized;
    uint8_t cleaned;
    char creator[33];
    
    uint8_t tmap[160];              /* Track map */
    uint32_t trks_offset;           /* TRKS chunk offset */
} woz1_image_t;

/*============================================================================*
 * WOZ1 FUNCTIONS
 *============================================================================*/

/**
 * @brief Read WOZ1 file
 * 
 * @param path File path
 * @param image_out Image (output)
 * @return 0 on success
 */
int woz1_read(const char *path, woz1_image_t **image_out);

/**
 * @brief Free WOZ1 image
 * 
 * @param image Image to free
 */
void woz1_free(woz1_image_t *image);

/**
 * @brief Get track data
 * 
 * @param path File path
 * @param image Image
 * @param track_idx Track index
 * @param track_out Track data (output)
 * @return 0 on success
 */
int woz1_get_track(const char *path, woz1_image_t *image, 
                   uint8_t track_idx, woz1_track_t *track_out);

/**
 * @brief Print WOZ1 info
 * 
 * @param image Image
 */
void woz1_print_info(const woz1_image_t *image);

#ifdef __cplusplus
}
#endif

#endif /* WOZ1_READER_H */
