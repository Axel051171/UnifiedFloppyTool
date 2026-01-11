/**
 * @file uft_mfm_image.h
 * @brief MFM native bitstream image format support
 * 
 * This format stores raw MFM bitstream data with timing information.
 * It preserves the exact pulse timing for forensic analysis.
 * 
 * File structure:
 *   - Header (48 bytes)
 *   - Track table (16 bytes per track)
 *   - Track data (variable length)
 */

#ifndef UFT_MFM_IMAGE_H
#define UFT_MFM_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic identifier */
#define UFT_MFM_MAGIC "MFM_IMG "
#define UFT_MFM_MAGIC_LEN 8

/**
 * @brief MFM image file header
 */
typedef struct {
    uint8_t  id_str[8];           /**< "MFM_IMG " */
    uint64_t track_table_offset;  /**< Byte offset to track table */
    uint64_t number_of_tracks;    /**< Total track count */
    uint64_t spindle_time_ns;     /**< Time for 1 rotation (ns) */
    uint64_t data_bit_rate;       /**< Bit rate (e.g., 500000 for MFM) */
    uint64_t sampling_rate;       /**< Sampling rate (e.g., 4000000) */
} uft_mfm_header_t;

/**
 * @brief MFM track table entry
 */
typedef struct {
    uint64_t offset;              /**< Absolute byte offset to track data */
    uint64_t length_bit;          /**< Track length in bits */
} uft_mfm_track_entry_t;

/**
 * @brief MFM image context
 */
typedef struct {
    FILE *fp;                     /**< File handle */
    bool is_write;                /**< True if opened for writing */
    
    uft_mfm_header_t header;      /**< File header */
    uft_mfm_track_entry_t *tracks; /**< Track table */
    size_t track_count;           /**< Number of tracks */
    
    /* Default parameters for new images */
    uint64_t default_spindle_ns;  /**< Default: 166666667 (300 RPM) */
    uint64_t default_bit_rate;    /**< Default: 500000 (MFM DD) */
    uint64_t default_sample_rate; /**< Default: 4000000 (4 MHz) */
} uft_mfm_ctx_t;

/**
 * @brief Open MFM image for reading
 * @param path File path
 * @param ctx Output context
 * @return 0 on success, negative on error
 */
int uft_mfm_open_read(const char *path, uft_mfm_ctx_t *ctx);

/**
 * @brief Open MFM image for writing
 * @param path File path
 * @param track_count Expected number of tracks
 * @param ctx Output context
 * @return 0 on success, negative on error
 */
int uft_mfm_open_write(const char *path, size_t track_count, uft_mfm_ctx_t *ctx);

/**
 * @brief Close MFM image
 * @param ctx Context to close
 */
void uft_mfm_close(uft_mfm_ctx_t *ctx);

/**
 * @brief Read track data
 * @param ctx Image context
 * @param track_num Track number (0-based)
 * @param bits Output buffer for bit data
 * @param bits_capacity Buffer capacity in bits
 * @param out_length Output: actual track length in bits
 * @return 0 on success, negative on error
 */
int uft_mfm_read_track(uft_mfm_ctx_t *ctx,
                       size_t track_num,
                       uint8_t *bits,
                       size_t bits_capacity,
                       size_t *out_length);

/**
 * @brief Write track data
 * @param ctx Image context
 * @param track_num Track number (0-based)
 * @param bits Bit data to write
 * @param length_bits Length in bits
 * @return 0 on success, negative on error
 */
int uft_mfm_write_track(uft_mfm_ctx_t *ctx,
                        size_t track_num,
                        const uint8_t *bits,
                        size_t length_bits);

/**
 * @brief Get track count
 * @param ctx Image context
 * @return Number of tracks
 */
static inline size_t uft_mfm_get_track_count(const uft_mfm_ctx_t *ctx) {
    return ctx ? ctx->track_count : 0;
}

/**
 * @brief Get track length in bits
 * @param ctx Image context
 * @param track_num Track number
 * @return Track length in bits, or 0 on error
 */
size_t uft_mfm_get_track_length(const uft_mfm_ctx_t *ctx, size_t track_num);

/**
 * @brief Get sampling rate
 * @param ctx Image context
 * @return Sampling rate in Hz
 */
static inline uint64_t uft_mfm_get_sample_rate(const uft_mfm_ctx_t *ctx) {
    return ctx ? ctx->header.sampling_rate : 0;
}

/**
 * @brief Get data bit rate
 * @param ctx Image context
 * @return Bit rate in Hz
 */
static inline uint64_t uft_mfm_get_bit_rate(const uft_mfm_ctx_t *ctx) {
    return ctx ? ctx->header.data_bit_rate : 0;
}

/**
 * @brief Get spindle rotation time
 * @param ctx Image context
 * @return Rotation time in nanoseconds
 */
static inline uint64_t uft_mfm_get_spindle_time_ns(const uft_mfm_ctx_t *ctx) {
    return ctx ? ctx->header.spindle_time_ns : 0;
}

/**
 * @brief Set image parameters (for writing)
 * @param ctx Image context
 * @param sample_rate Sampling rate in Hz
 * @param bit_rate Data bit rate in Hz
 * @param spindle_ns Spindle rotation time in ns
 */
void uft_mfm_set_params(uft_mfm_ctx_t *ctx,
                        uint64_t sample_rate,
                        uint64_t bit_rate,
                        uint64_t spindle_ns);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_IMAGE_H */
