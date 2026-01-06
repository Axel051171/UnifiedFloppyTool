/**
 * @file uft_ff_formats.h
 * @brief FlashFloppy-derived format detection algorithms
 *
 * Extracted from FlashFloppy by Keir Fraser (Public Domain/Unlicense)
 * Adapted for UFT by the UFT Team
 *
 * Supported formats:
 * - TI-99/4A (VIB detection)
 * - PC-98 FDI/HDM
 * - MSX (BPB detection)
 * - MGT (SAM Coupé/+D)
 * - UKNC (Soviet PDP-11 clone)
 *
 * SPDX-License-Identifier: Unlicense
 */
#ifndef UFT_FF_FORMATS_H
#define UFT_FF_FORMATS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum uft_ff_error {
    UFT_FF_OK = 0,
    UFT_FF_ERR_INVALID = -1,
    UFT_FF_ERR_NOT_DETECTED = -2,
    UFT_FF_ERR_IO = -3,
    UFT_FF_ERR_SIZE = -4
} uft_ff_error_t;

/* ============================================================================
 * Common Structures
 * ============================================================================ */

/**
 * @brief Disk geometry information
 */
typedef struct uft_ff_geometry {
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    uint16_t rpm;
    uint8_t  gap3;
    uint8_t  interleave;
    uint8_t  skew;
    bool     is_fm;          /**< FM encoding (vs MFM) */
    bool     has_iam;        /**< Has Index Address Mark */
    uint32_t data_offset;    /**< Offset to first data byte */
} uft_ff_geometry_t;

/**
 * @brief Format detection result
 */
typedef struct uft_ff_detect_result {
    const char *format_name;
    const char *format_desc;
    uft_ff_geometry_t geometry;
    uint32_t confidence;     /**< 0-100 */
    uint32_t flags;
} uft_ff_detect_result_t;

/* Detection flags */
#define UFT_FF_FLAG_SEQUENTIAL     (1u << 0)
#define UFT_FF_FLAG_SIDES_SWAPPED  (1u << 1)
#define UFT_FF_FLAG_REVERSE_SIDE0  (1u << 2)
#define UFT_FF_FLAG_REVERSE_SIDE1  (1u << 3)

/* ============================================================================
 * TI-99/4A Format
 * ============================================================================ */

/**
 * @brief TI-99/4A Volume Information Block (VIB)
 */
#pragma pack(push, 1)
typedef struct uft_ti99_vib {
    uint8_t  name[10];           /**< Volume name (space-padded) */
    uint16_t total_sectors;      /**< Total sectors (BE) */
    uint8_t  sectors_per_track;  /**< Sectors per track */
    char     id[3];              /**< "DSK" signature */
    uint8_t  protection;         /**< Write protection flag */
    uint8_t  tracks_per_side;    /**< Tracks per side */
    uint8_t  sides;              /**< Number of sides (1 or 2) */
    uint8_t  density;            /**< Density code */
} uft_ti99_vib_t;
#pragma pack(pop)

/**
 * @brief Detect TI-99/4A disk format
 * 
 * @param data First sector data (256 bytes minimum)
 * @param file_size Total file size in bytes
 * @param result Output detection result
 * @return UFT_FF_OK on success
 */
int uft_ff_detect_ti99(const uint8_t *data, size_t file_size,
                       uft_ff_detect_result_t *result);

/* ============================================================================
 * PC-98 Formats
 * ============================================================================ */

/**
 * @brief PC-98 FDI header
 */
#pragma pack(push, 1)
typedef struct uft_pc98_fdi_header {
    uint32_t zero;               /**< Must be 0 */
    uint32_t density;            /**< 0x30=2DD, other=2HD */
    uint32_t header_size;        /**< Usually 4096 */
    uint32_t image_body_size;    /**< Data size */
    uint32_t sector_size;        /**< Bytes per sector */
    uint32_t sectors_per_track;  /**< Sectors per track */
    uint32_t heads;              /**< Number of heads */
    uint32_t cylinders;          /**< Number of cylinders */
} uft_pc98_fdi_header_t;
#pragma pack(pop)

/**
 * @brief Detect PC-98 FDI format
 */
int uft_ff_detect_pc98_fdi(const uint8_t *header, size_t file_size,
                           uft_ff_detect_result_t *result);

/**
 * @brief Detect PC-98 HDM format (raw 1.25MB HD)
 */
int uft_ff_detect_pc98_hdm(size_t file_size, uft_ff_detect_result_t *result);

/* ============================================================================
 * MSX Format
 * ============================================================================ */

/**
 * @brief DOS BPB structure for MSX detection
 */
#pragma pack(push, 1)
typedef struct uft_msx_bpb {
    uint16_t bytes_per_sector;   /**< Offset 0x0B */
    uint16_t sectors_per_track;  /**< Offset 0x18 */
    uint16_t heads;              /**< Offset 0x1A */
    uint16_t total_sectors;      /**< Offset 0x13 */
    uint16_t root_entries;       /**< Offset 0x11 */
    uint16_t fat_sectors;        /**< Offset 0x16 */
    uint16_t boot_signature;     /**< Offset 0x1FE (0xAA55) */
} uft_msx_bpb_t;
#pragma pack(pop)

/**
 * @brief Detect MSX disk format from BPB
 */
int uft_ff_detect_msx(const uint8_t *boot_sector, size_t file_size,
                      uft_ff_detect_result_t *result);

/* ============================================================================
 * MGT Format (SAM Coupé / +D)
 * ============================================================================ */

/**
 * @brief Detect MGT format (SAM Coupé, Spectrum +D)
 * 
 * MGT format: 80 tracks, 2 sides, 10 sectors/track, 512 bytes/sector = 819200 bytes
 */
int uft_ff_detect_mgt(size_t file_size, uft_ff_detect_result_t *result);

/* ============================================================================
 * UKNC Format (Soviet PDP-11 Clone)
 * ============================================================================ */

/**
 * @brief Detect UKNC format
 * 
 * UKNC: 80 tracks, 2 sides, 10 sectors/track, 512 bytes/sector
 * Special: post-CRC sync marks, custom GAP2/GAP4A
 */
int uft_ff_detect_uknc(size_t file_size, uft_ff_detect_result_t *result);

/* ============================================================================
 * Auto-Detection
 * ============================================================================ */

/**
 * @brief Try all FlashFloppy-derived format detectors
 * 
 * @param data File data (at least first 4096 bytes)
 * @param data_size Size of data buffer
 * @param file_size Total file size
 * @param result Output result (best match)
 * @return UFT_FF_OK if format detected, UFT_FF_ERR_NOT_DETECTED otherwise
 */
int uft_ff_detect_auto(const uint8_t *data, size_t data_size, size_t file_size,
                       uft_ff_detect_result_t *result);

/**
 * @brief Get format name from detection result
 */
const char *uft_ff_format_name(const uft_ff_detect_result_t *result);

/**
 * @brief Print geometry info (for debugging)
 */
void uft_ff_print_geometry(const uft_ff_geometry_t *geom);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FF_FORMATS_H */
