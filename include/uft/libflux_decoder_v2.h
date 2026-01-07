// SPDX-License-Identifier: MIT
/**
 * @file libflux_decoder_v2.h
 * @brief GOD MODE HxC Decoder API
 * @version 2.0.0-GOD
 * @date 2025-01-02
 *
 * High-performance MFM/GCR decoder with:
 * - SIMD-optimized decoding
 * - Weak bit detection
 * - Multi-threaded track processing
 * - GUI parameter integration
 */

#ifndef HXC_DECODER_V2_H
#define HXC_DECODER_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define HXC_V2_MAX_TRACKS       168
#define HXC_V2_MAX_SECTORS      32
#define HXC_V2_SECTOR_SIZE_MAX  8192

/* Encoding types */
#define HXC_ENCODING_MFM        0
#define HXC_ENCODING_GCR_C64    1
#define HXC_ENCODING_GCR_APPLE  2
#define HXC_ENCODING_FM         3

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct libflux_decoder_v2 libflux_decoder_v2_t;

/* Decoded sector */
typedef struct {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    uint16_t data_size;
    uint16_t crc_read;
    uint16_t crc_calc;
    bool crc_ok;
    bool has_weak_bits;
    uint8_t weak_bit_count;
    uint8_t data[HXC_V2_SECTOR_SIZE_MAX];
    uint8_t weak_mask[HXC_V2_SECTOR_SIZE_MAX];
    float confidence;
} libflux_sector_v2_t;

/* Track result */
typedef struct {
    int cylinder;
    int head;
    uint8_t* raw_data;
    size_t raw_size;
    size_t bit_count;
    
    libflux_sector_v2_t sectors[HXC_V2_MAX_SECTORS];
    int sector_count;
    
    float avg_confidence;
    int weak_bits_total;
    int crc_errors;
    
    /* Multi-rev weak bit detection */
    uint8_t** revolutions;
    int rev_count;
    float* bit_variance;
} libflux_track_v2_t;

/* GUI parameters */
typedef struct {
    /* MFM parameters */
    float mfm_pll_bandwidth;        /* 1-15%, default 5 */
    int mfm_sync_threshold;         /* 3-10, default 4 */
    bool mfm_ignore_crc;
    
    /* GCR parameters */
    float gcr_pll_bandwidth;        /* 1-15%, default 5 */
    bool gcr_allow_illegal;
    
    /* Weak bit detection */
    bool detect_weak_bits;
    int weak_bit_revolutions;       /* 2-16, default 3 */
    float weak_bit_threshold;       /* 0.1-0.5, default 0.15 */
    
    /* Threading */
    int thread_count;               /* 1-8, default 4 */
    bool enable_cache;
    
    /* Error handling */
    int max_crc_errors;
    bool abort_on_error;
} libflux_params_v2_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize parameters with defaults
 */
void libflux_v2_params_init(libflux_params_v2_t* params);

/**
 * @brief Validate parameters
 */
bool libflux_v2_params_validate(const libflux_params_v2_t* params);

/**
 * @brief Create decoder instance
 */
libflux_decoder_v2_t* libflux_v2_create(const libflux_params_v2_t* params);

/**
 * @brief Destroy decoder instance
 */
void libflux_v2_destroy(libflux_decoder_v2_t* dec);

/**
 * @brief Decode single track
 * @param dec Decoder instance
 * @param raw_data Raw track data (caller owns)
 * @param raw_size Size in bytes
 * @param cylinder Cylinder number
 * @param head Head number (0 or 1)
 * @param encoding HXC_ENCODING_* constant
 * @param track_out Decoded track output
 * @return 0 on success
 */
int libflux_v2_decode_track(libflux_decoder_v2_t* dec,
                        const uint8_t* raw_data,
                        size_t raw_size,
                        int cylinder, int head,
                        int encoding,
                        libflux_track_v2_t* track_out);

/**
 * @brief Get decoder statistics
 */
void libflux_v2_get_stats(libflux_decoder_v2_t* dec,
                      uint64_t* tracks, uint64_t* sectors,
                      uint64_t* crc_errors, uint64_t* weak_bits);

/**
 * @brief Set progress callback
 */
void libflux_v2_set_progress_callback(libflux_decoder_v2_t* dec,
                                   void (*cb)(int track, int sector, void*),
                                   void* user_data);

/**
 * @brief Set error callback
 */
void libflux_v2_set_error_callback(libflux_decoder_v2_t* dec,
                                void (*cb)(const char* msg, void*),
                                void* user_data);

/*============================================================================
 * GUI PARAMETER CONSTRAINTS
 *============================================================================*/

/* PLL bandwidth */
#define HXC_V2_PLL_BW_MIN       1.0f
#define HXC_V2_PLL_BW_MAX       15.0f
#define HXC_V2_PLL_BW_DEFAULT   5.0f

/* Sync threshold */
#define HXC_V2_SYNC_MIN         3
#define HXC_V2_SYNC_MAX         10
#define HXC_V2_SYNC_DEFAULT     4

/* Weak bit detection */
#define HXC_V2_WEAK_REV_MIN     2
#define HXC_V2_WEAK_REV_MAX     16
#define HXC_V2_WEAK_REV_DEFAULT 3

#define HXC_V2_WEAK_THRESH_MIN  0.1f
#define HXC_V2_WEAK_THRESH_MAX  0.5f
#define HXC_V2_WEAK_THRESH_DEF  0.15f

/* Threading */
#define HXC_V2_THREAD_MIN       1
#define HXC_V2_THREAD_MAX       8
#define HXC_V2_THREAD_DEFAULT   4

#ifdef __cplusplus
}
#endif

#endif /* HXC_DECODER_V2_H */
