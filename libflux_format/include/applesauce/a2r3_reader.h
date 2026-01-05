// SPDX-License-Identifier: MIT
/*
 * a2r3_reader.h - Applesauce A2R 3.x Format Reader
 * 
 * Reads Applesauce A2R 3.x flux disk images.
 * Preserves flux data losslessly with picosecond timing resolution.
 * 
 * Format Specification:
 *   https://applesaucefdc.com/a2r/
 * 
 * @version 2.8.1
 * @date 2024-12-26
 */

#ifndef A2R3_READER_H
#define A2R3_READER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * A2R3 STRUCTURES
 *============================================================================*/

/**
 * @brief A2R3 capture data
 */
typedef struct {
    uint32_t location;              /* Track location */
    uint32_t capture_type;          /* 1=timing, 2=bits, 3=xtiming */
    uint32_t resolution_ps;         /* Picoseconds per tick */
    uint8_t  index_count;           /* Number of index marks */
    uint32_t* index_ticks;          /* Index mark positions */
    
    /* Packed flux data (lossless) */
    uint8_t*  packed;
    uint32_t packed_len;
    
    /* Decoded flux deltas */
    uint32_t* deltas_ticks;
    uint32_t  deltas_count;
} a2r3_capture_t;

/**
 * @brief A2R3 disk image
 */
typedef struct {
    uint8_t  info_version;
    char     creator[64];
    uint8_t  drive_type;
    uint8_t  write_protected;
    uint8_t  synchronized;
    uint8_t  hard_sector_count;
    
    a2r3_capture_t* captures;
    size_t captures_len;
    
    a2r3_capture_t* solved;
    size_t solved_len;
} a2r3_image_t;

/*============================================================================*
 * A2R3 FUNCTIONS
 *============================================================================*/

/**
 * @brief Read A2R3 file
 * 
 * @param path File path
 * @param image_out Image (output)
 * @return 0 on success
 */
int a2r3_read(const char *path, a2r3_image_t **image_out);

/**
 * @brief Free A2R3 image
 * 
 * @param image Image to free
 */
void a2r3_free(a2r3_image_t *image);

/**
 * @brief Get capture by location
 * 
 * @param image Image
 * @param location Track location
 * @return Capture or NULL
 */
a2r3_capture_t* a2r3_get_capture(a2r3_image_t *image, uint32_t location);

/**
 * @brief Print A2R3 info
 * 
 * @param image Image
 */
void a2r3_print_info(const a2r3_image_t *image);

#ifdef __cplusplus
}
#endif

#endif /* A2R3_READER_H */
