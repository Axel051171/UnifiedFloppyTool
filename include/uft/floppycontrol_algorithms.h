// SPDX-License-Identifier: MIT
/*
 * floppycontrol_algorithms.h - FloppyControl Algorithms for UFT
 * 
 * Ported from FloppyControl (C# -> C)
 * Original: https://github.com/Skaizo/FloppyControl
 * 
 * This header provides access to advanced floppy disk recovery algorithms
 * including adaptive MFM processing, error correction, and format-specific
 * decoders for Amiga and PC DOS disks.
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef FLOPPYCONTROL_ALGORITHMS_H
#define FLOPPYCONTROL_ALGORITHMS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * ADAPTIVE MFM PROCESSING
 * 
 * Dynamic threshold algorithm that adapts to disk conditions.
 * Essential for recovering data from degraded media.
 *============================================================================*/

typedef struct {
    int fourus;                 /* 4µs timing threshold */
    int sixus;                  /* 6µs timing threshold */
    int eightus;                /* 8µs timing threshold */
    float rate_of_change;       /* Adaptation rate */
    int lowpass_radius;         /* Low-pass filter window */
    int start;                  /* Start offset in input */
    int end;                    /* End offset in input */
    bool is_hd;                 /* High-density flag */
    bool add_noise;             /* Enable noise injection */
    int noise_amount;           /* Noise amplitude */
    int noise_limit_start;      /* Noise window start */
    int noise_limit_end;        /* Noise window end */
} adaptive_settings_t;

typedef struct {
    uint8_t *mfm_data;          /* Output MFM bitstream */
    size_t mfm_length;          /* Output length */
    float *entropy;             /* Timing deviation per sample */
    size_t entropy_length;      /* Entropy array length */
    int stat_4us;               /* Count of 4µs pulses */
    int stat_6us;               /* Count of 6µs pulses */
    int stat_8us;               /* Count of 8µs pulses */
} adaptive_result_t;

void adaptive_default_settings(adaptive_settings_t *settings);
int adaptive_period_to_mfm(const uint8_t *rxbuf, size_t rxbuf_len,
                           const adaptive_settings_t *settings,
                           adaptive_result_t *result);
int adaptive_auto_configure(const uint8_t *rxbuf, size_t len, 
                           adaptive_settings_t *settings);
void adaptive_free_result(adaptive_result_t *result);

/*============================================================================*
 * AMIGA MFM DECODING
 * 
 * Supports AmigaDOS (OFS/FFS) and DiskSpare formats.
 *============================================================================*/

typedef enum {
    AMIGA_FORMAT_UNKNOWN = 0,
    AMIGA_FORMAT_ADOS,          /* AmigaDOS OFS/FFS */
    AMIGA_FORMAT_DISKSPARE,     /* DiskSpare */
    AMIGA_FORMAT_PFS            /* Professional File System */
} amiga_format_t;

typedef struct {
    uint8_t format;
    uint8_t track;
    uint8_t sector;
    uint8_t sectors_to_gap;
    uint16_t os_recovery;
    uint32_t header_checksum;
    uint32_t data_checksum;
    bool header_ok;
    bool data_ok;
    uint8_t data[512];
} amiga_sector_t;

typedef struct {
    int marker_position;
    int rxbuf_position;
    amiga_sector_t sector;
} amiga_marker_t;

int amiga_find_markers(const uint8_t *mfm, size_t mfm_len,
                       amiga_marker_t *markers, int max_markers,
                       amiga_format_t format);
int amiga_decode_sector_ados(const uint8_t *mfm, int marker_pos,
                             amiga_sector_t *sector);
int amiga_decode_sector_diskspare(const uint8_t *mfm, int marker_pos,
                                  amiga_sector_t *sector);
int amiga_create_adf(const amiga_sector_t *sectors, int num_sectors,
                     amiga_format_t format, uint8_t *image_out, 
                     size_t image_size);
amiga_format_t amiga_detect_format(const uint8_t *mfm, size_t mfm_len);

/*============================================================================*
 * PC DOS MFM DECODING
 * 
 * Supports PC DOS DD/HD, MSX, Atari ST formats.
 *============================================================================*/

typedef enum {
    PC_FORMAT_UNKNOWN = 0,
    PC_FORMAT_DD,           /* 720K */
    PC_FORMAT_HD,           /* 1.44M */
    PC_FORMAT_DD_360,       /* 360K */
    PC_FORMAT_HD_1200,      /* 1.2M */
    PC_FORMAT_2M,           /* Extended */
    PC_FORMAT_MSX,
    PC_FORMAT_ATARI_ST
} pc_format_t;

typedef struct {
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    uint16_t crc;
    bool header_ok;
    bool data_ok;
    bool deleted;
    int sector_size;
    uint8_t *data;
} pc_sector_t;

typedef struct {
    int marker_position;
    pc_sector_t sector;
} pc_marker_t;

uint16_t crc16_ccitt(const uint8_t *data, size_t len);
uint16_t crc16_with_sync(const uint8_t *data, size_t len);
int pc_find_markers(const uint8_t *mfm, size_t mfm_len,
                    pc_marker_t *markers, int max_markers);
int pc_decode_header(const uint8_t *mfm, int marker_pos, pc_sector_t *sector);
int pc_decode_data(const uint8_t *mfm, int data_marker_pos, pc_sector_t *sector);
void pc_free_sector(pc_sector_t *sector);
int pc_create_image(const pc_sector_t *sectors, int num_sectors,
                    pc_format_t format, uint8_t *image_out, size_t image_size);
pc_format_t pc_detect_format(const pc_sector_t *sectors, int num_sectors);

/*============================================================================*
 * ERROR CORRECTION
 * 
 * Brute-force bit-flipping to recover bad sectors.
 *============================================================================*/

typedef enum {
    EC_SUCCESS = 0,
    EC_NOT_FOUND,
    EC_TIMEOUT,
    EC_INVALID_PARAM,
    EC_NO_MEMORY
} ec_result_t;

typedef struct {
    int bit_positions[12];
    int num_flips;
    uint32_t iterations;
} ec_correction_t;

typedef struct {
    int start_bit;
    int end_bit;
    int max_flips;
    uint16_t expected_crc;
    bool verbose;
    int (*progress_cb)(int current, int total, void *user_data);
    void *user_data;
} ec_params_t;

typedef struct {
    int start;
    int end;
    float confidence;
} error_region_t;

ec_result_t ec_correct_single_bit(uint8_t *data, size_t data_len,
                                  const ec_params_t *params,
                                  ec_correction_t *correction);
ec_result_t ec_correct_multi_bit(uint8_t *data, size_t data_len,
                                 const ec_params_t *params,
                                 ec_correction_t *correction);
int ec_detect_error_regions(const uint8_t *capture1, const uint8_t *capture2,
                           size_t len, error_region_t *regions, int max_regions);
void ec_print_correction(const ec_correction_t *correction);
const char *ec_result_string(ec_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* FLOPPYCONTROL_ALGORITHMS_H */
