/**
 * @file uft_cfi.h
 * @brief CFI (Compressed Floppy Image) format support
 * @version 3.9.0
 * 
 * CFI format created by FDCOPY.COM under DOS.
 * Used for distributing Amstrad PC boot floppies.
 * 
 * Format structure:
 * - Track blocks with 2-byte length header
 * - Each block contains RLE-compressed data
 * - No file header - format is detected by validation
 * 
 * Reference: libdsk drvcfi.c by John Elliott
 */

#ifndef UFT_CFI_H
#define UFT_CFI_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CFI has no signature - detection based on BPB validation */
#define CFI_MAX_TRACK_SIZE      32768
#define CFI_MIN_FILE_SIZE       512

/**
 * @brief CFI read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Image info (from BPB) */
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    
    /* Statistics */
    size_t compressed_size;
    size_t uncompressed_size;
    uint32_t track_count;
    
} cfi_read_result_t;

/**
 * @brief CFI write options
 */
typedef struct {
    bool use_compression;        /* Use RLE compression */
} cfi_write_options_t;

/* ============================================================================
 * CFI Compression
 * 
 * CFI uses simple RLE:
 * - 2-byte block header (little-endian length)
 * - If high bit of length[1] set: RLE block (length, fill_byte)
 * - Otherwise: literal data block
 * ============================================================================ */

/**
 * @brief Decompress CFI track data
 * @return Decompressed size, or -1 on error
 */
int cfi_decompress_track(const uint8_t *input, size_t track_block_size,
                         uint8_t *output, size_t output_capacity);

/**
 * @brief Compress track data for CFI format
 * @return Compressed size (including block headers), or -1 on error
 */
int cfi_compress_track(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_capacity);

/* ============================================================================
 * CFI File I/O
 * ============================================================================ */

/**
 * @brief Read CFI file
 */
uft_error_t uft_cfi_read(const char *path,
                         uft_disk_image_t **out_disk,
                         cfi_read_result_t *result);

/**
 * @brief Read CFI from memory
 */
uft_error_t uft_cfi_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             cfi_read_result_t *result);

/**
 * @brief Write CFI file
 */
uft_error_t uft_cfi_write(const uft_disk_image_t *disk,
                          const char *path,
                          const cfi_write_options_t *opts);

/**
 * @brief Probe if data is CFI format
 * Note: CFI detection requires full decompression and BPB validation
 */
bool uft_cfi_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Initialize write options with defaults
 */
void uft_cfi_write_options_init(cfi_write_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CFI_H */
