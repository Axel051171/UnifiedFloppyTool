// SPDX-License-Identifier: MIT
/*
 * msa.h - MSA (Magic Shadow Archiver) Format Support
 * 
 * MSA Format Specification:
 * - Atari ST compressed disk image format
 * - Created by David Lawrence (Magic Shadow Software)
 * - RLE compression algorithm
 * - Supports 9 and 10 sector formats
 * - Single/Double sided support
 * 
 * @version 2.8.6
 * @date 2024-12-26
 */

#ifndef MSA_H
#define MSA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * MSA CONSTANTS
 *============================================================================*/

#define MSA_MAGIC           0x0E0F      /* Magic number (big endian) */
#define MSA_HEADER_SIZE     10

/* Sectors per track */
#define MSA_SECTORS_9       9
#define MSA_SECTORS_10      10
#define MSA_SECTORS_11      11
#define MSA_SECTORS_18      18

/* Bytes per sector */
#define MSA_SECTOR_SIZE     512

/* Typical track counts */
#define MSA_TRACKS_SS       80          /* Single sided */
#define MSA_TRACKS_DS       80          /* Double sided (160 total) */

/* Maximum values */
#define MSA_MAX_TRACKS      256
#define MSA_MAX_SIDES       2

/*============================================================================*
 * MSA STRUCTURES
 *============================================================================*/

/**
 * @brief MSA file header (10 bytes, big endian)
 */
typedef struct {
    uint16_t magic;             /* 0x0E0F */
    uint16_t sectors_per_track; /* Sectors per track (9, 10, 11, or 18) */
    uint16_t sides;             /* 0 = single sided, 1 = double sided */
    uint16_t starting_track;    /* Usually 0 */
    uint16_t ending_track;      /* Usually 79 or 159 */
} __attribute__((packed)) msa_header_t;

/**
 * @brief MSA track header (4 bytes per track, big endian)
 */
typedef struct {
    uint16_t data_length;       /* Length of track data (compressed) */
    uint16_t track_data[];      /* Track data (variable length) */
} __attribute__((packed)) msa_track_header_t;

/**
 * @brief MSA image container (in-memory representation)
 */
typedef struct {
    /* Header */
    msa_header_t header;
    
    /* Track data */
    uint8_t **tracks;           /* Array of decompressed tracks */
    size_t *track_sizes;        /* Size of each decompressed track */
    uint16_t num_tracks;
    uint16_t num_sides;
    
    /* File info */
    char *filename;
    bool modified;
} msa_image_t;

/*============================================================================*
 * MSA API
 *============================================================================*/

/**
 * @brief Read MSA image from file
 * 
 * @param filename Path to MSA file
 * @param image Output image structure
 * @return true on success
 */
bool msa_read(const char *filename, msa_image_t *image);

/**
 * @brief Write MSA image to file
 * 
 * @param filename Path to output file
 * @param image Image to write
 * @return true on success
 */
bool msa_write(const char *filename, const msa_image_t *image);

/**
 * @brief Initialize empty MSA image
 * 
 * @param image Image to initialize
 * @param sectors_per_track Sectors per track (9, 10, 11, or 18)
 * @param sides Number of sides (1 or 2)
 * @param tracks Number of tracks (typically 80)
 * @return true on success
 */
bool msa_init(msa_image_t *image, uint16_t sectors_per_track,
              uint16_t sides, uint16_t tracks);

/**
 * @brief Free MSA image resources
 * 
 * @param image Image to free
 */
void msa_free(msa_image_t *image);

/**
 * @brief Get track data from MSA image
 * 
 * @param image Image to read from
 * @param track Track number
 * @param side Side number (0 or 1)
 * @param data Output buffer for track data
 * @param size Output size of track data
 * @return true on success
 */
bool msa_get_track(const msa_image_t *image, uint16_t track, uint16_t side,
                   uint8_t **data, size_t *size);

/**
 * @brief Decompress MSA track data (RLE)
 * 
 * @param compressed Compressed data
 * @param comp_size Size of compressed data
 * @param decompressed Output buffer
 * @param decomp_size Expected decompressed size
 * @return true on success
 */
bool msa_decompress_track(const uint8_t *compressed, size_t comp_size,
                          uint8_t *decompressed, size_t decomp_size);

/**
 * @brief Compress MSA track data (RLE)
 * 
 * @param data Track data to compress
 * @param size Size of track data
 * @param compressed Output buffer
 * @param comp_size Output compressed size
 * @return true on success
 */
bool msa_compress_track(const uint8_t *data, size_t size,
                        uint8_t *compressed, size_t *comp_size);

/**
 * @brief Convert MSA to ST (raw Atari ST format)
 * 
 * @param msa_filename Input MSA file
 * @param st_filename Output ST file
 * @return true on success
 */
bool msa_to_st(const char *msa_filename, const char *st_filename);

/**
 * @brief Convert ST to MSA
 * 
 * @param st_filename Input ST file
 * @param msa_filename Output MSA file
 * @return true on success
 */
bool st_to_msa(const char *st_filename, const char *msa_filename);

/**
 * @brief Validate MSA image
 * 
 * @param image Image to validate
 * @param errors Output buffer for error messages (optional)
 * @param errors_size Size of error buffer
 * @return true if valid
 */
bool msa_validate(const msa_image_t *image, char *errors, size_t errors_size);

/**
 * @brief Calculate track size in bytes
 * 
 * @param sectors_per_track Number of sectors per track
 * @return Track size in bytes
 */
size_t msa_track_size(uint16_t sectors_per_track);

#ifdef __cplusplus
}
#endif

#endif /* MSA_H */
