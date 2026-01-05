// SPDX-License-Identifier: MIT
/*
 * moof_reader.h - Applesauce MOOF 1.0 Format Reader
 * 
 * Reads MOOF 1.0 hybrid disk images (bitstream OR flux).
 * Supports both quantized bitstreams and raw flux captures.
 * 
 * Format Specification:
 *   https://applesaucefdc.com/moof-reference/
 * 
 * @version 2.8.1
 * @date 2024-12-26
 */

#ifndef MOOF_READER_H
#define MOOF_READER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * MOOF STRUCTURES
 *============================================================================*/

/**
 * @brief MOOF track data
 */
typedef struct {
    uint16_t start_block;           /* Block offset */
    uint16_t block_count;
    uint32_t bit_count;
    
    /* Bitstream data */
    uint8_t* bitstream;
    uint32_t bitstream_len;
    
    /* Optional flux data */
    uint8_t* flux_packed;
    uint32_t flux_packed_len;
    uint32_t* flux_deltas;
    uint32_t flux_delta_count;
} moof_track_t;

/**
 * @brief MOOF disk image
 */
typedef struct {
    uint8_t info_version;
    uint8_t disk_type;              /* 1=SSDD 400K, 2=DSDD 800K, 3=DSHD 1.44M */
    uint8_t write_protected;
    uint8_t synchronized;
    uint8_t optimal_bit_timing_125ns;
    char creator[33];
    
    uint16_t largest_track_blocks;
    uint16_t flux_block;            /* 0 if no flux */
    
    uint8_t tmap[160];              /* Track map */
    moof_track_t* tracks;
    size_t track_count;
} moof_image_t;

/*============================================================================*
 * MOOF FUNCTIONS
 *============================================================================*/

/**
 * @brief Read MOOF file
 * 
 * @param path File path
 * @param image_out Image (output)
 * @return 0 on success
 */
int moof_read(const char *path, moof_image_t **image_out);

/**
 * @brief Free MOOF image
 * 
 * @param image Image to free
 */
void moof_free(moof_image_t *image);

/**
 * @brief Get track data
 * 
 * @param path File path
 * @param image Image
 * @param track_idx Track index
 * @param track_out Track data (output)
 * @return 0 on success
 */
int moof_get_track(const char *path, moof_image_t *image,
                   uint8_t track_idx, moof_track_t *track_out);

/**
 * @brief Check if image has flux data
 * 
 * @param image Image
 * @return true if has flux
 */
bool moof_has_flux(const moof_image_t *image);

/**
 * @brief Print MOOF info
 * 
 * @param image Image
 */
void moof_print_info(const moof_image_t *image);

#ifdef __cplusplus
}
#endif

#endif /* MOOF_READER_H */
