/**
 * @file uft_msa.h
 * @brief MSA (Magic Shadow Archiver) Format Support
 * @version 3.1.4.002
 *
 * MSA is the Atari ST disk image format created by Magic Shadow Archiver.
 * Features:
 * - Run-Length Encoding (RLE) compression
 * - Track-based storage
 * - Support for single/double sided disks
 * - 9-11 sectors per track
 *
 * Based on msa-coder by Keith Clark (MIT License)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MSA_H
#define UFT_MSA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * MSA Constants
 *============================================================================*/

/** MSA file magic number */
#define UFT_MSA_MAGIC           0x0E0F

/** RLE marker byte */
#define UFT_MSA_RLE_MARKER      0xE5

/** Standard sector size (Atari ST) */
#define UFT_MSA_SECTOR_SIZE     512

/** Maximum sectors per track */
#define UFT_MSA_MAX_SPT         11

/** Maximum tracks */
#define UFT_MSA_MAX_TRACKS      86

/*============================================================================
 * Data Structures
 *============================================================================*/

/** MSA file header */
typedef struct __attribute__((packed)) {
    uint16_t    magic;          /**< Magic number (0x0E0F) */
    uint16_t    sectors_per_track;  /**< Sectors per track (9-11) */
    uint16_t    sides;          /**< Number of sides (0=SS, 1=DS) */
    uint16_t    first_track;    /**< First encoded track */
    uint16_t    last_track;     /**< Last encoded track */
} uft_msa_header_t;

/** MSA disk image handle */
typedef struct {
    uft_msa_header_t header;    /**< File header */
    uint8_t    *data;           /**< Decompressed track data */
    size_t      data_size;      /**< Size of decompressed data */
    bool        modified;       /**< Data has been modified */
} uft_msa_disk_t;

/** MSA format information */
typedef struct {
    uint16_t    sectors_per_track;
    uint16_t    sides;
    uint16_t    first_track;
    uint16_t    last_track;
    uint32_t    total_sectors;
    uint32_t    total_bytes;
} uft_msa_info_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Open and decode MSA file
 * @param path Path to MSA file
 * @return Disk handle or NULL on error
 */
uft_msa_disk_t *uft_msa_open(const char *path);

/**
 * @brief Decode MSA from memory buffer
 * @param data MSA file data
 * @param size Data size
 * @return Disk handle or NULL on error
 */
uft_msa_disk_t *uft_msa_decode(const uint8_t *data, size_t size);

/**
 * @brief Close MSA disk
 * @param disk Disk handle
 */
void uft_msa_close(uft_msa_disk_t *disk);

/**
 * @brief Get MSA format information
 * @param disk Disk handle
 * @param info Output information structure
 * @return 0 on success
 */
int uft_msa_get_info(uft_msa_disk_t *disk, uft_msa_info_t *info);

/**
 * @brief Read sector from MSA image
 * @param disk Disk handle
 * @param track Track number
 * @param side Side (0 or 1)
 * @param sector Sector number (1-based)
 * @param buffer Output buffer (512 bytes)
 * @return 0 on success, -1 on error
 */
int uft_msa_read_sector(uft_msa_disk_t *disk,
                        uint16_t track, uint16_t side, uint16_t sector,
                        uint8_t *buffer);

/**
 * @brief Write sector to MSA image
 * @param disk Disk handle
 * @param track Track number
 * @param side Side
 * @param sector Sector number (1-based)
 * @param buffer Data to write (512 bytes)
 * @return 0 on success, -1 on error
 */
int uft_msa_write_sector(uft_msa_disk_t *disk,
                         uint16_t track, uint16_t side, uint16_t sector,
                         const uint8_t *buffer);

/**
 * @brief Save MSA to file
 * @param disk Disk handle
 * @param path Output path
 * @param compress Enable RLE compression
 * @return 0 on success, -1 on error
 */
int uft_msa_save(uft_msa_disk_t *disk, const char *path, bool compress);

/**
 * @brief Encode data to MSA format
 * @param disk Disk handle
 * @param output Output buffer (caller allocated)
 * @param output_size Output buffer size
 * @param compress Enable compression
 * @param written Actual bytes written
 * @return 0 on success, -1 on error
 */
int uft_msa_encode(uft_msa_disk_t *disk,
                   uint8_t *output, size_t output_size,
                   bool compress, size_t *written);

/**
 * @brief Convert MSA to raw ST image
 * @param disk MSA disk handle
 * @param path Output path
 * @return 0 on success, -1 on error
 */
int uft_msa_to_st(uft_msa_disk_t *disk, const char *path);

/**
 * @brief Convert raw ST to MSA
 * @param st_path Input ST file path
 * @param msa_path Output MSA path
 * @param compress Enable compression
 * @return 0 on success, -1 on error
 */
int uft_st_to_msa(const char *st_path, const char *msa_path, bool compress);

/**
 * @brief Create new MSA disk image
 * @param sectors_per_track Sectors per track (9-11)
 * @param sides Number of sides (1 or 2)
 * @param tracks Number of tracks (typically 80)
 * @return Disk handle or NULL on error
 */
uft_msa_disk_t *uft_msa_create(uint16_t sectors_per_track,
                                uint16_t sides, uint16_t tracks);

/*============================================================================
 * RLE Compression Functions
 *============================================================================*/

/**
 * @brief Decompress RLE-encoded track data
 * @param input Compressed data
 * @param input_size Compressed size
 * @param output Output buffer
 * @param output_size Expected output size
 * @return Actual output bytes, or -1 on error
 */
int uft_msa_rle_decompress(const uint8_t *input, size_t input_size,
                            uint8_t *output, size_t output_size);

/**
 * @brief Compress track data with RLE
 * @param input Raw data
 * @param input_size Input size
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return Compressed size, or -1 if larger than input
 */
int uft_msa_rle_compress(const uint8_t *input, size_t input_size,
                          uint8_t *output, size_t output_size);

/*============================================================================
 * Standard Atari ST Formats
 *============================================================================*/

/** Single-sided 9 sector (360KB) */
#define UFT_MSA_SS_9SPT     { 9, 0, 0, 79 }

/** Double-sided 9 sector (720KB) */
#define UFT_MSA_DS_9SPT     { 9, 1, 0, 79 }

/** Double-sided 10 sector (800KB) */
#define UFT_MSA_DS_10SPT    { 10, 1, 0, 79 }

/** Double-sided 11 sector (880KB) - special format */
#define UFT_MSA_DS_11SPT    { 11, 1, 0, 79 }

#ifdef __cplusplus
}
#endif

#endif /* UFT_MSA_H */
