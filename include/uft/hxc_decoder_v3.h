// SPDX-License-Identifier: MIT
/**
 * @file hxc_decoder_v3.h
 * @brief GOD MODE ULTRA HxC Decoder API
 * @version 3.0.0-GOD-ULTRA
 * @date 2025-01-02
 *
 * Maximum accuracy decoder with:
 * - Viterbi soft-decision decoding
 * - Kalman PLL with jitter tracking
 * - Machine-learning weak bit prediction
 * - Copy protection detection
 * - Multi-format auto-detection
 */

#ifndef HXC_DECODER_V3_H
#define HXC_DECODER_V3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define HXC_V3_MAX_TRACKS       168
#define HXC_V3_MAX_SECTORS      32
#define HXC_V3_MAX_REVOLUTIONS  32

/* Encoding types */
#define HXC_V3_ENC_AUTO         0
#define HXC_V3_ENC_MFM          1
#define HXC_V3_ENC_FM           2
#define HXC_V3_ENC_GCR_CBM      3
#define HXC_V3_ENC_GCR_APPLE    4
#define HXC_V3_ENC_AMIGA        5

/* Copy protection flags */
#define HXC_V3_PROT_NONE        0x00
#define HXC_V3_PROT_WEAK_BITS   0x01
#define HXC_V3_PROT_LONG_TRACK  0x02
#define HXC_V3_PROT_NON_STD_GAP 0x04
#define HXC_V3_PROT_TIMING_VAR  0x08
#define HXC_V3_PROT_HALF_TRACK  0x10
#define HXC_V3_PROT_FUZZY_BITS  0x20

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct hxc_decoder_v3 hxc_decoder_v3_t;

/* Soft decision bit */
typedef struct {
    uint8_t hard_value;
    float confidence;
    float variance;
    uint8_t revolution_votes;
} hxc_soft_bit_t;

/* Decoded sector */
typedef struct {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    uint16_t data_size;
    
    uint16_t header_crc_read;
    uint16_t header_crc_calc;
    uint16_t data_crc_read;
    uint16_t data_crc_calc;
    bool header_crc_ok;
    bool data_crc_ok;
    
    uint8_t data[8192];
    hxc_soft_bit_t* soft_data;
    size_t soft_data_count;
    
    uint8_t weak_mask[8192];
    uint16_t weak_bit_count;
    bool has_weak_bits;
    
    float decode_confidence;
    float signal_quality;
    float timing_quality;
    
    uint8_t corrected_bytes;
    uint8_t protection_flags;
    
    uint32_t bit_start;
    uint32_t bit_end;
    float avg_bit_time_ns;
    float jitter_ns;
} hxc_sector_v3_t;

/* Track result */
typedef struct {
    int cylinder;
    int head;
    int encoding;
    
    uint32_t* flux_times;
    size_t flux_count;
    
    hxc_soft_bit_t* soft_bits;
    size_t soft_bit_count;
    
    uint8_t* bit_data;
    size_t bit_count;
    
    hxc_sector_v3_t sectors[HXC_V3_MAX_SECTORS];
    int sector_count;
    
    float avg_confidence;
    float min_confidence;
    int total_weak_bits;
    int crc_errors;
    int corrected_errors;
    
    uint8_t protection_flags;
    float track_length_ratio;
    
    /* Multi-revolution */
    uint32_t** rev_flux;
    size_t* rev_flux_count;
    int revolution_count;
    
    /* Visualization */
    float* bit_timing_histogram;
    size_t histogram_bins;
} hxc_track_v3_t;

/* Format detection result */
typedef struct {
    int encoding;
    int sectors_per_track;
    int sector_size;
    int interleave;
    float confidence;
    const char* format_name;
    uint8_t protection_flags;
} hxc_format_detect_t;

/* Configuration */
typedef struct {
    /* PLL */
    double pll_bandwidth;           /* 0.01-0.15, default 0.05 */
    double pll_damping;             /* 0.5-2.0, default 1.0 */
    bool pll_adaptive;
    
    /* Viterbi */
    bool enable_viterbi;            /* default true */
    int viterbi_depth;              /* 8-64, default 32 */
    float viterbi_threshold;        /* 0.1-0.9, default 0.5 */
    
    /* Weak bits */
    bool detect_weak_bits;          /* default true */
    int weak_bit_revolutions;       /* 2-32, default 3 */
    float weak_bit_threshold;       /* 0.05-0.3, default 0.15 */
    bool predict_weak_bits;
    
    /* Error correction */
    bool enable_ecc;
    int ecc_mode;                   /* 0=off, 1=RS, 2=BCH */
    
    /* Copy protection */
    bool detect_protection;         /* default true */
    bool preserve_protection;       /* default true */
    
    /* Threading */
    int thread_count;               /* 1-16, default 4 */
    bool enable_work_stealing;
    
    /* Streaming */
    bool streaming_mode;
    size_t stream_buffer_size;
    
    /* Visualization */
    bool export_timing_data;
    bool export_soft_data;
} hxc_config_v3_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

void hxc_v3_config_init(hxc_config_v3_t* config);
hxc_decoder_v3_t* hxc_v3_create(const hxc_config_v3_t* config);
void hxc_v3_destroy(hxc_decoder_v3_t* dec);

int hxc_v3_decode_track(hxc_decoder_v3_t* dec,
                        const uint32_t* flux_times,
                        size_t flux_count,
                        int cylinder, int head,
                        hxc_track_v3_t* track_out);

void hxc_v3_free_track(hxc_track_v3_t* track);

void hxc_v3_get_stats(hxc_decoder_v3_t* dec,
                      uint64_t* tracks, uint64_t* sectors,
                      uint64_t* bits, uint64_t* weak_bits);

void hxc_v3_set_progress_callback(hxc_decoder_v3_t* dec,
                                   void (*cb)(int, int, float, void*),
                                   void* user_data);

void hxc_v3_set_error_callback(hxc_decoder_v3_t* dec,
                                void (*cb)(const char*, int, void*),
                                void* user_data);

/*============================================================================
 * GUI PARAMETER CONSTRAINTS
 *============================================================================*/

/* PLL */
#define HXC_V3_PLL_BW_MIN       0.01
#define HXC_V3_PLL_BW_MAX       0.15
#define HXC_V3_PLL_BW_DEFAULT   0.05

/* Viterbi */
#define HXC_V3_VITERBI_DEPTH_MIN    8
#define HXC_V3_VITERBI_DEPTH_MAX    64
#define HXC_V3_VITERBI_DEPTH_DEF    32

/* Weak bits */
#define HXC_V3_WEAK_REV_MIN     2
#define HXC_V3_WEAK_REV_MAX     32
#define HXC_V3_WEAK_REV_DEFAULT 3

#define HXC_V3_WEAK_THRESH_MIN  0.05f
#define HXC_V3_WEAK_THRESH_MAX  0.30f
#define HXC_V3_WEAK_THRESH_DEF  0.15f

/* Threading */
#define HXC_V3_THREAD_MIN       1
#define HXC_V3_THREAD_MAX       16
#define HXC_V3_THREAD_DEFAULT   4

#ifdef __cplusplus
}
#endif

#endif /* HXC_DECODER_V3_H */
