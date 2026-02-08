/**
 * @file uft_decoders_v2.h
 * @brief Thread-safe floppy disk decoders - Public API
 * 
 * This header provides unified access to all v2 decoder implementations:
 * - MFM (Modified Frequency Modulation) - PC/Amiga/Atari ST
 * - FM (Frequency Modulation) - BBC Micro/early 8" drives
 * - Apple GCR (5/3 and 6/2) - Apple II
 * - C64 GCR (4/5) - Commodore 64/1541
 * 
 * All decoders are thread-safe with no global state.
 * 
 * @version 2.0
 * @date 2026-01-07
 */

#ifndef UFT_DECODERS_V2_H
#define UFT_DECODERS_V2_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Common Types
 *===========================================================================*/

/** Forward declarations for opaque context pointers */
typedef struct uft_mfm_context_s   uft_mfm_context_t;
typedef struct uft_fm_context_s    uft_fm_context_t;
typedef struct uft_apple_context_s uft_apple_context_t;
typedef struct uft_c64_context_s   uft_c64_context_t;

/*===========================================================================
 * MFM Decoder API
 *===========================================================================*/

/** MFM error codes */
typedef enum {
    UFT_MFM_OK = 0,
    UFT_MFM_ERR_NULL_CONTEXT,
    UFT_MFM_ERR_NULL_BUFFER,
    UFT_MFM_ERR_BUFFER_OVERFLOW,
    UFT_MFM_ERR_SYNC_LOST,
    UFT_MFM_ERR_CRC_MISMATCH,
    UFT_MFM_ERR_OUT_OF_MEMORY,
    UFT_MFM_ERR_INVALID_STATE
} uft_mfm_error_t;

/** MFM IDAM (ID Address Mark) */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint16_t crc;
    unsigned long position;
    bool     valid;
} uft_mfm_idam_t;

/** MFM sector callback */
typedef void (*uft_mfm_callback_t)(const uft_mfm_idam_t *idam,
                                   const uint8_t *data,
                                   uint32_t len,
                                   bool crc_ok,
                                   void *user_data);

/** Create MFM decoder */
uft_mfm_context_t* uft_mfm_create(void);

/** Destroy MFM decoder */
void uft_mfm_destroy(uft_mfm_context_t *ctx);

/** Reset decoder state */
uft_mfm_error_t uft_mfm_reset(uft_mfm_context_t *ctx);

/** Set sector callback */
uft_mfm_error_t uft_mfm_set_callback(uft_mfm_context_t *ctx,
                                     uft_mfm_callback_t callback,
                                     void *user_data);

/** Enable debug output */
uft_mfm_error_t uft_mfm_set_debug(uft_mfm_context_t *ctx, bool enable);

/** Process a single bit */
uft_mfm_error_t uft_mfm_add_bit(uft_mfm_context_t *ctx,
                                uint8_t bit,
                                unsigned long datapos);

/** Decode complete track */
uft_mfm_error_t uft_mfm_decode_track(uft_mfm_context_t *ctx,
                                     const uint8_t *bits,
                                     size_t bit_len);

/** Get statistics */
uft_mfm_error_t uft_mfm_get_stats(const uft_mfm_context_t *ctx,
                                  uint32_t *sectors_found,
                                  uint32_t *sectors_good,
                                  uint32_t *sectors_bad_crc,
                                  uint32_t *sync_losses);

/** Get error string */
const char* uft_mfm_error_str(uft_mfm_error_t err);

/*===========================================================================
 * FM Decoder API
 *===========================================================================*/

/** FM error codes */
typedef enum {
    UFT_FM_OK = 0,
    UFT_FM_ERR_NULL_CONTEXT,
    UFT_FM_ERR_NULL_BUFFER,
    UFT_FM_ERR_BUFFER_OVERFLOW,
    UFT_FM_ERR_SYNC_LOST,
    UFT_FM_ERR_CRC_MISMATCH,
    UFT_FM_ERR_OUT_OF_MEMORY,
    UFT_FM_ERR_INVALID_STATE
} uft_fm_error_t;

/** FM IDAM */
typedef struct {
    int8_t   track;
    int8_t   head;
    int8_t   sector;
    int8_t   size_code;
    uint16_t crc;
    unsigned long position;
    bool     valid;
} uft_fm_idam_t;

/** FM sector callback */
typedef void (*uft_fm_callback_t)(const uft_fm_idam_t *idam,
                                  const uint8_t *data,
                                  uint32_t len,
                                  bool crc_ok,
                                  void *user_data);

uft_fm_context_t* uft_fm_create(void);
void uft_fm_destroy(uft_fm_context_t *ctx);
uft_fm_error_t uft_fm_reset(uft_fm_context_t *ctx);
uft_fm_error_t uft_fm_set_callback(uft_fm_context_t *ctx,
                                   uft_fm_callback_t callback,
                                   void *user_data);
uft_fm_error_t uft_fm_set_debug(uft_fm_context_t *ctx, bool enable);
uft_fm_error_t uft_fm_add_bit(uft_fm_context_t *ctx,
                              uint8_t bit,
                              unsigned long datapos);
uft_fm_error_t uft_fm_get_stats(const uft_fm_context_t *ctx,
                                uint32_t *sectors_found,
                                uint32_t *sectors_good,
                                uint32_t *sectors_bad_crc,
                                uint32_t *sync_losses);
const char* uft_fm_error_str(uft_fm_error_t err);

/*===========================================================================
 * Apple GCR Decoder API
 *===========================================================================*/

/** Apple GCR error codes */
typedef enum {
    UFT_APPLE_OK = 0,
    UFT_APPLE_ERR_NULL_CONTEXT,
    UFT_APPLE_ERR_NULL_BUFFER,
    UFT_APPLE_ERR_BUFFER_OVERFLOW,
    UFT_APPLE_ERR_CHECKSUM,
    UFT_APPLE_ERR_INVALID_GCR,
    UFT_APPLE_ERR_OUT_OF_MEMORY,
    UFT_APPLE_ERR_INVALID_STATE
} uft_apple_error_t;

/** Apple GCR modes */
typedef enum {
    UFT_APPLE_GCR_53 = 0,  /**< DOS 3.2: 5/3 GCR, 13 sectors */
    UFT_APPLE_GCR_62       /**< DOS 3.3: 6/2 GCR, 16 sectors */
} uft_apple_gcr_mode_t;

/** Apple address field */
typedef struct {
    uint8_t  volume;
    uint8_t  track;
    uint8_t  sector;
    uint8_t  checksum;
    unsigned long position;
    bool     valid;
} uft_apple_addr_t;

/** Apple sector callback */
typedef void (*uft_apple_callback_t)(const uft_apple_addr_t *addr,
                                     const uint8_t *data,
                                     uint32_t len,
                                     bool checksum_ok,
                                     void *user_data);

uft_apple_context_t* uft_apple_create(void);
void uft_apple_destroy(uft_apple_context_t *ctx);
uft_apple_error_t uft_apple_reset(uft_apple_context_t *ctx);
uft_apple_error_t uft_apple_set_gcr_mode(uft_apple_context_t *ctx,
                                         uft_apple_gcr_mode_t mode);
uft_apple_error_t uft_apple_set_callback(uft_apple_context_t *ctx,
                                         uft_apple_callback_t callback,
                                         void *user_data);
uft_apple_error_t uft_apple_set_debug(uft_apple_context_t *ctx, bool enable);
uft_apple_error_t uft_apple_add_bit(uft_apple_context_t *ctx,
                                    uint8_t bit,
                                    unsigned long datapos);
uft_apple_error_t uft_apple_get_stats(const uft_apple_context_t *ctx,
                                      uint32_t *found, uint32_t *good,
                                      uint32_t *bad, uint32_t *sync_losses);
const char* uft_apple_error_str(uft_apple_error_t err);

/*===========================================================================
 * C64 GCR Decoder API
 *===========================================================================*/

/** C64 GCR error codes */
typedef enum {
    UFT_C64_OK = 0,
    UFT_C64_ERR_NULL_CONTEXT,
    UFT_C64_ERR_NULL_BUFFER,
    UFT_C64_ERR_BUFFER_OVERFLOW,
    UFT_C64_ERR_CHECKSUM,
    UFT_C64_ERR_INVALID_GCR,
    UFT_C64_ERR_OUT_OF_MEMORY,
    UFT_C64_ERR_INVALID_STATE
} uft_c64_error_t;

/** C64 header block */
typedef struct {
    uint8_t  block_type;
    uint8_t  checksum;
    uint8_t  sector;
    uint8_t  track;
    uint8_t  id1;
    uint8_t  id2;
    unsigned long position;
    bool     valid;
} uft_c64_header_t;

/** C64 sector callback */
typedef void (*uft_c64_callback_t)(const uft_c64_header_t *header,
                                   const uint8_t *data,
                                   uint32_t len,
                                   bool checksum_ok,
                                   void *user_data);

uft_c64_context_t* uft_c64_create(void);
void uft_c64_destroy(uft_c64_context_t *ctx);
uft_c64_error_t uft_c64_reset(uft_c64_context_t *ctx);
uft_c64_error_t uft_c64_set_callback(uft_c64_context_t *ctx,
                                     uft_c64_callback_t callback,
                                     void *user_data);
uft_c64_error_t uft_c64_set_debug(uft_c64_context_t *ctx, bool enable);
uft_c64_error_t uft_c64_add_bit(uft_c64_context_t *ctx,
                                uint8_t bit,
                                unsigned long datapos);
uft_c64_error_t uft_c64_get_stats(const uft_c64_context_t *ctx,
                                  uint32_t *found, uint32_t *good,
                                  uint32_t *bad, uint32_t *gcr_errors);
const char* uft_c64_error_str(uft_c64_error_t err);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/** Get sector size from size code (MFM/FM) */
static inline uint32_t uft_sector_size_from_code(uint8_t size_code) {
    return (size_code <= 7) ? (128u << size_code) : 0;
}

/** Get C64 sectors per track */
static inline int uft_c64_sectors_per_track(int track) {
    if (track >= 1 && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 35) return 17;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODERS_V2_H */
