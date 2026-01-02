/**
 * @file uft_p64.h
 * @brief Commodore P64 Flux Format Support
 * @version 3.1.4.004
 *
 * P64 is a high-precision flux format for Commodore disk preservation:
 * - 16 MHz sampling rate (3,200,000 samples per rotation at 300 RPM)
 * - Half-track resolution (tracks 1-42.5, or half-tracks 2-85)
 * - Per-pulse strength values for weak bit emulation
 * - Range-coded compression (FPAQ0-style)
 *
 * Format developed by Benjamin 'BeRo' Rosseaux for VICE emulator.
 *
 * Reference: https://vice-emu.sourceforge.io/
 * Based on p64tech.txt specification
 *
 * SPDX-License-Identifier: MIT (zlib-style, per original)
 */

#ifndef UFT_P64_H
#define UFT_P64_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * P64 Constants
 *============================================================================*/

/** File signature "P64-1541" */
#define UFT_P64_SIGNATURE       "P64-1541"
#define UFT_P64_SIGNATURE_LEN   8

/** Samples per rotation at 16 MHz, 300 RPM */
#define UFT_P64_SAMPLES_PER_ROT 3200000

/** Half-track range */
#define UFT_P64_FIRST_HALFTRACK 2
#define UFT_P64_LAST_HALFTRACK  85

/** Maximum number of half-tracks */
#define UFT_P64_MAX_HALFTRACKS  (UFT_P64_LAST_HALFTRACK - UFT_P64_FIRST_HALFTRACK + 2)

/** Chunk signature for half-track data: "HTPx" where x = half-track index */
#define UFT_P64_CHUNK_HTP       "HTP"

/** End-of-file chunk signature */
#define UFT_P64_CHUNK_DONE      "DONE"

/** Pulse strength values */
#define UFT_P64_STRENGTH_MAX    0xFFFFFFFF  /**< Always triggers */
#define UFT_P64_STRENGTH_MIN    0x00000001  /**< Almost never triggers */
#define UFT_P64_STRENGTH_NONE   0x00000000  /**< Never triggers */

/*============================================================================
 * Speed Zones (C64/1541)
 *============================================================================*/

/**
 * Speed zone timing for 1541 drive.
 * Zone determines data rate based on track number.
 */
typedef enum {
    UFT_P64_ZONE_0 = 0,  /**< Tracks 31-35: 250 kbit/s, 17 sectors */
    UFT_P64_ZONE_1 = 1,  /**< Tracks 25-30: 266.67 kbit/s, 18 sectors */
    UFT_P64_ZONE_2 = 2,  /**< Tracks 18-24: 285.71 kbit/s, 19 sectors */
    UFT_P64_ZONE_3 = 3,  /**< Tracks 1-17: 307.69 kbit/s, 21 sectors */
} uft_p64_speed_zone_t;

/**
 * @brief Get speed zone for track number
 * @param track Track number (1-35)
 * @return Speed zone (0-3)
 */
static inline uft_p64_speed_zone_t uft_p64_track_zone(int track) {
    if (track >= 31) return UFT_P64_ZONE_0;
    if (track >= 25) return UFT_P64_ZONE_1;
    if (track >= 18) return UFT_P64_ZONE_2;
    return UFT_P64_ZONE_3;
}

/**
 * @brief Get sectors per track for zone
 * @param zone Speed zone
 * @return Sectors per track
 */
static inline int uft_p64_zone_sectors(uft_p64_speed_zone_t zone) {
    static const int spt[] = { 17, 18, 19, 21 };
    return spt[zone & 3];
}

/**
 * @brief Get bit time in samples for zone
 * @param zone Speed zone
 * @return Samples per bit cell
 */
static inline int uft_p64_zone_bittime(uft_p64_speed_zone_t zone) {
    /* 16 MHz / bitrate */
    static const int times[] = { 64, 60, 56, 52 };
    return times[zone & 3];
}

/*============================================================================
 * P64 Structures
 *============================================================================*/

/** P64 file header (20 bytes) */
typedef struct {
    uint8_t     signature[8];   /**< "P64-1541" */
    uint32_t    version;        /**< Format version (0) */
    uint32_t    flags;          /**< Bit 0: write protect */
    uint32_t    data_size;      /**< Size of chunk stream */
    uint32_t    data_crc;       /**< CRC32 of chunk stream */
} uft_p64_header_t;

/** P64 chunk header (12 bytes) */
typedef struct {
    uint8_t     signature[4];   /**< Chunk type */
    uint32_t    size;           /**< Chunk data size */
    uint32_t    crc;            /**< CRC32 of chunk data */
} uft_p64_chunk_header_t;

/** Single flux pulse */
typedef struct {
    uint32_t    position;       /**< Position in samples (0 to 3199999) */
    uint32_t    strength;       /**< Pulse strength (0 to 0xFFFFFFFF) */
} uft_p64_pulse_t;

/** Pulse stream for one half-track */
typedef struct {
    uft_p64_pulse_t *pulses;    /**< Array of pulses */
    uint32_t    count;          /**< Number of pulses */
    uint32_t    capacity;       /**< Allocated capacity */
} uft_p64_pulse_stream_t;

/** P64 image handle */
typedef struct {
    uft_p64_pulse_stream_t streams[2][UFT_P64_MAX_HALFTRACKS]; /**< [side][halftrack] */
    uint32_t    flags;          /**< Write protect etc. */
    int         sides;          /**< Number of sides (1 or 2) */
} uft_p64_image_t;

/*============================================================================
 * Range Coder (FPAQ0-style)
 *============================================================================*/

/** Range coder state */
typedef struct {
    uint8_t    *buffer;         /**< Data buffer */
    uint32_t    buffer_size;    /**< Buffer size */
    uint32_t    buffer_pos;     /**< Current position */
    uint32_t    range_code;     /**< Current code value */
    uint32_t    range_low;      /**< Range low bound */
    uint32_t    range_high;     /**< Range high bound */
} uft_p64_range_coder_t;

/** Probability model */
typedef struct {
    uint32_t   *probs;          /**< Probability array */
    uint32_t    count;          /**< Number of probabilities */
} uft_p64_prob_model_t;

/*============================================================================
 * P64 API
 *============================================================================*/

/**
 * @brief Create empty P64 image
 * @return New image handle
 */
uft_p64_image_t *uft_p64_create(void);

/**
 * @brief Open P64 file
 * @param path File path
 * @return Image handle or NULL
 */
uft_p64_image_t *uft_p64_open(const char *path);

/**
 * @brief Open P64 from memory
 * @param data File data
 * @param len Data length
 * @return Image handle or NULL
 */
uft_p64_image_t *uft_p64_open_mem(const uint8_t *data, size_t len);

/**
 * @brief Save P64 to file
 * @param img Image handle
 * @param path Output path
 * @return 0 on success
 */
int uft_p64_save(const uft_p64_image_t *img, const char *path);

/**
 * @brief Close P64 image
 * @param img Image handle
 */
void uft_p64_close(uft_p64_image_t *img);

/*============================================================================
 * Pulse Stream Operations
 *============================================================================*/

/**
 * @brief Clear pulse stream
 * @param stream Pulse stream
 */
void uft_p64_stream_clear(uft_p64_pulse_stream_t *stream);

/**
 * @brief Add pulse to stream
 * @param stream Pulse stream
 * @param position Position in samples
 * @param strength Pulse strength
 */
void uft_p64_stream_add_pulse(uft_p64_pulse_stream_t *stream,
                              uint32_t position, uint32_t strength);

/**
 * @brief Get next pulse after position
 * @param stream Pulse stream
 * @param position Current position
 * @return Next pulse position, or P64_SAMPLES_PER_ROT if none
 */
uint32_t uft_p64_stream_next_pulse(const uft_p64_pulse_stream_t *stream,
                                   uint32_t position);

/**
 * @brief Get delta to next pulse
 * @param stream Pulse stream
 * @param position Current position
 * @return Samples to next pulse
 */
uint32_t uft_p64_stream_delta_to_next(const uft_p64_pulse_stream_t *stream,
                                      uint32_t position);

/*============================================================================
 * GCR Conversion
 *============================================================================*/

/**
 * @brief Convert GCR bytes to pulse stream
 *
 * @param stream Output pulse stream
 * @param bytes GCR encoded bytes
 * @param len Number of bytes
 * @param zone Speed zone
 */
void uft_p64_stream_from_gcr(uft_p64_pulse_stream_t *stream,
                             const uint8_t *bytes, uint32_t len,
                             uft_p64_speed_zone_t zone);

/**
 * @brief Convert pulse stream to GCR bytes
 *
 * @param stream Pulse stream
 * @param bytes Output buffer
 * @param len Buffer size
 * @param zone Speed zone
 * @return Number of bytes decoded
 */
uint32_t uft_p64_stream_to_gcr(const uft_p64_pulse_stream_t *stream,
                               uint8_t *bytes, uint32_t len,
                               uft_p64_speed_zone_t zone);

/**
 * @brief Convert pulse stream to GCR with logic analysis
 *
 * Uses more sophisticated decoding with PLL-like behavior.
 *
 * @param stream Pulse stream
 * @param bytes Output buffer
 * @param len Buffer size
 * @param zone Speed zone
 * @return Number of bytes decoded
 */
uint32_t uft_p64_stream_to_gcr_logic(const uft_p64_pulse_stream_t *stream,
                                     uint8_t *bytes, uint32_t len,
                                     uft_p64_speed_zone_t zone);

/*============================================================================
 * Range Coder Functions
 *============================================================================*/

/**
 * @brief Initialize range coder for encoding
 * @param rc Range coder state
 */
void uft_p64_rc_init_encode(uft_p64_range_coder_t *rc);

/**
 * @brief Initialize range coder for decoding
 * @param rc Range coder state
 * @param data Input data
 * @param len Data length
 */
void uft_p64_rc_init_decode(uft_p64_range_coder_t *rc,
                            const uint8_t *data, uint32_t len);

/**
 * @brief Encode single bit with probability
 * @param rc Range coder state
 * @param prob Probability (updated)
 * @param shift Adaptation shift (typically 4)
 * @param bit Bit value (0 or 1)
 */
void uft_p64_rc_encode_bit(uft_p64_range_coder_t *rc,
                           uint32_t *prob, int shift, int bit);

/**
 * @brief Decode single bit with probability
 * @param rc Range coder state
 * @param prob Probability (updated)
 * @param shift Adaptation shift
 * @return Decoded bit (0 or 1)
 */
int uft_p64_rc_decode_bit(uft_p64_range_coder_t *rc,
                          uint32_t *prob, int shift);

/**
 * @brief Flush encoder output
 * @param rc Range coder state
 */
void uft_p64_rc_flush(uft_p64_range_coder_t *rc);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Calculate P64 CRC32
 * @param data Data buffer
 * @param len Data length
 * @return CRC32 value
 */
uint32_t uft_p64_crc32(const uint8_t *data, uint32_t len);

/**
 * @brief Convert track number to half-track index
 * @param track Track number (1.0 to 42.5)
 * @return Half-track index (2-85)
 */
static inline int uft_p64_track_to_halftrack(float track) {
    return (int)(track * 2.0f);
}

/**
 * @brief Convert half-track index to track number
 * @param halftrack Half-track index
 * @return Track number
 */
static inline float uft_p64_halftrack_to_track(int halftrack) {
    return (float)halftrack / 2.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_P64_H */
