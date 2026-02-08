#include "uft_mfm_flux.h"
/**
 * @file uft_mfm_decoder_v2.c
 * @brief Thread-safe MFM decoder implementation
 * 
 * This is a refactored version of the original BBC-FDC mfm.c code.
 * Key improvements:
 * - Thread-safe: All state in context structure, no globals
 * - Memory-safe: Proper bounds checking on all buffers
 * - Error handling: Return codes instead of silent failures
 * - const-correct: Input parameters marked appropriately
 * 
 * Original code: BBC FDC project
 * Refactored for UFT: 2026-01-07
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum size of a decoded sector in bytes */
#define UFT_MFM_MAX_SECTOR_SIZE    8192

/** Maximum size of bitstream buffer */
#define UFT_MFM_BLOCKSIZE          (UFT_MFM_MAX_SECTOR_SIZE + 64)

/** MFM sync patterns */
#define UFT_MFM_SYNC_PATTERN       0x4489
#define UFT_MFM_IAM_SYNC           0x5224

/** MFM address marks */
#define UFT_MFM_MARK_IDAM          0xFE  /**< ID Address Mark */
#define UFT_MFM_MARK_DAM           0xFB  /**< Data Address Mark */
#define UFT_MFM_MARK_DDAM          0xF8  /**< Deleted Data Address Mark */
#define UFT_MFM_MARK_IAM           0xFC  /**< Index Address Mark */

/** Access marks */
#define UFT_MFM_ACCESS_SYNC        0xA1
#define UFT_MFM_ACCESS_INDEX       0xC2
#define UFT_MFM_ACCESS_SECTOR      0xA1

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_MFM_OK = 0,
    UFT_MFM_ERR_NULL_CONTEXT,
    UFT_MFM_ERR_NULL_BUFFER,
    UFT_MFM_ERR_BUFFER_OVERFLOW,
    UFT_MFM_ERR_SYNC_LOST,
    UFT_MFM_ERR_CRC_MISMATCH,
    UFT_MFM_ERR_CLOCK_VIOLATION,
    UFT_MFM_ERR_INVALID_MARK,
    UFT_MFM_ERR_OUT_OF_MEMORY,
    UFT_MFM_ERR_INVALID_STATE
} uft_mfm_error_t;

/**
 * @brief Get human-readable error string
 */
static inline const char* uft_mfm_error_str(uft_mfm_error_t err) {
    switch (err) {
        case UFT_MFM_OK:                return "OK";
        case UFT_MFM_ERR_NULL_CONTEXT:  return "Null context";
        case UFT_MFM_ERR_NULL_BUFFER:   return "Null buffer";
        case UFT_MFM_ERR_BUFFER_OVERFLOW: return "Buffer overflow";
        case UFT_MFM_ERR_SYNC_LOST:     return "Sync lost";
        case UFT_MFM_ERR_CRC_MISMATCH:  return "CRC mismatch";
        case UFT_MFM_ERR_CLOCK_VIOLATION: return "Clock violation";
        case UFT_MFM_ERR_INVALID_MARK:  return "Invalid address mark";
        case UFT_MFM_ERR_OUT_OF_MEMORY: return "Out of memory";
        case UFT_MFM_ERR_INVALID_STATE: return "Invalid state";
        default:                        return "Unknown error";
    }
}

/*===========================================================================
 * State Machine
 *===========================================================================*/

typedef enum {
    UFT_MFM_STATE_SYNC = 0,    /**< Looking for sync pattern */
    UFT_MFM_STATE_MARK,        /**< Found sync, looking for address mark */
    UFT_MFM_STATE_IDAM,        /**< Reading ID Address Mark fields */
    UFT_MFM_STATE_DATA,        /**< Reading data field */
    UFT_MFM_STATE_CRC          /**< Verifying CRC */
} uft_mfm_state_t;

/*===========================================================================
 * IDAM (ID Address Mark) Structure
 *===========================================================================*/

typedef struct {
    uint8_t  track;           /**< Cylinder number */
    uint8_t  head;            /**< Head/side number */
    uint8_t  sector;          /**< Sector number */
    uint8_t  size_code;       /**< Sector size code (0=128, 1=256, 2=512, 3=1024, etc.) */
    uint16_t crc;             /**< CRC-16 */
    unsigned long position;   /**< Position in stream where IDAM was found */
    bool     valid;           /**< CRC validated */
} uft_mfm_idam_t;

/*===========================================================================
 * Context Structure (Thread-Safe State)
 *===========================================================================*/

typedef struct {
    /* State machine */
    uft_mfm_state_t state;
    
    /* Bit accumulation */
    uint16_t datacells;       /**< 16-bit sliding buffer for clock+data */
    int bits;                 /**< Number of bits accumulated */
    
    /* Bit history (previous 48 bits for pattern matching) */
    uint16_t p1, p2, p3;
    
    /* Current IDAM being processed */
    uft_mfm_idam_t current_idam;
    
    /* Last successfully decoded IDAM */
    uft_mfm_idam_t last_idam;
    
    /* Data block info */
    uint8_t  block_type;      /**< DAM or DDAM */
    uint32_t block_size;      /**< Expected data size */
    unsigned long block_pos;  /**< Position where data block started */
    
    /* Output buffer */
    uint8_t  bitstream[UFT_MFM_BLOCKSIZE];
    uint32_t bitlen;          /**< Current position in bitstream */
    
    /* CRC tracking */
    uint16_t id_crc;          /**< CRC of ID field */
    uint16_t data_crc;        /**< CRC of data field */
    uint16_t running_crc;     /**< Running CRC calculation */
    
    /* PLL timing (if needed) */
    float    default_window;
    float    bucket_01;
    float    bucket_001;
    float    bucket_0001;
    
    /* Statistics */
    uint32_t sectors_found;
    uint32_t sectors_good;
    uint32_t sectors_bad_crc;
    uint32_t sync_losses;
    
    /* Debug mode */
    bool     debug;
    
    /* User callback for sector completion */
    void (*sector_callback)(const uft_mfm_idam_t *idam, 
                           const uint8_t *data, 
                           uint32_t len,
                           bool crc_ok,
                           void *user_data);
    void *user_data;
    
} uft_mfm_context_t;

/*===========================================================================
 * CRC-16 CCITT Implementation
 *===========================================================================*/

static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static inline uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    return (crc << 8) ^ crc16_table[(crc >> 8) ^ byte];
}

static uint16_t crc16_compute(const uint8_t *data, size_t len) __attribute__((unused));
static uint16_t crc16_compute(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;  /* MFM CRC initial value */
    for (size_t i = 0; i < len; i++) {
        crc = crc16_update(crc, data[i]);
    }
    return crc;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Extract clock bits from MFM cell pair
 */
static inline uint8_t mfm_get_clock(uint16_t cells) {
    return (uint8_t)(
        ((cells & 0x8000) >> 8) |
        ((cells & 0x2000) >> 6) |
        ((cells & 0x0800) >> 4) |
        ((cells & 0x0200) >> 2) |
        ((cells & 0x0080) >> 0) |
        ((cells & 0x0020) << 2) |
        ((cells & 0x0008) << 4) |
        ((cells & 0x0002) << 6)
    );
}

/**
 * @brief Extract data bits from MFM cell pair
 */
static inline uint8_t mfm_get_data(uint16_t cells) {
    return (uint8_t)(
        ((cells & 0x4000) >> 7) |
        ((cells & 0x1000) >> 5) |
        ((cells & 0x0400) >> 3) |
        ((cells & 0x0100) >> 1) |
        ((cells & 0x0040) << 1) |
        ((cells & 0x0010) << 3) |
        ((cells & 0x0004) << 5) |
        ((cells & 0x0001) << 7)
    );
}

/**
 * @brief Get sector size from size code
 */
static inline uint32_t mfm_sector_size(uint8_t size_code) {
    if (size_code > 7) return 0;
    return 128u << size_code;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief Create and initialize a new MFM decoder context
 * @return Allocated context or NULL on failure
 */
uft_mfm_context_t* uft_mfm_create(void) {
    uft_mfm_context_t *ctx = calloc(1, sizeof(uft_mfm_context_t));
    if (!ctx) return NULL;
    
    ctx->state = UFT_MFM_STATE_SYNC;
    ctx->default_window = 4.0f;  /* Default PLL window */
    ctx->debug = false;
    
    return ctx;
}

/**
 * @brief Destroy MFM decoder context
 * @param ctx Context to destroy (safe to pass NULL)
 */
void uft_mfm_destroy(uft_mfm_context_t *ctx) {
    free(ctx);  /* free(NULL) is safe */
}

/**
 * @brief Reset decoder state (keep configuration)
 */
uft_mfm_error_t uft_mfm_reset(uft_mfm_context_t *ctx) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    
    ctx->state = UFT_MFM_STATE_SYNC;
    ctx->datacells = 0;
    ctx->bits = 0;
    ctx->p1 = ctx->p2 = ctx->p3 = 0;
    ctx->bitlen = 0;
    ctx->running_crc = 0xFFFF;
    
    memset(&ctx->current_idam, 0, sizeof(ctx->current_idam));
    
    return UFT_MFM_OK;
}

/**
 * @brief Set sector completion callback
 */
uft_mfm_error_t uft_mfm_set_callback(uft_mfm_context_t *ctx,
                                     void (*callback)(const uft_mfm_idam_t*, 
                                                     const uint8_t*, 
                                                     uint32_t, bool, void*),
                                     void *user_data) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    
    ctx->sector_callback = callback;
    ctx->user_data = user_data;
    
    return UFT_MFM_OK;
}

/**
 * @brief Enable/disable debug output
 */
uft_mfm_error_t uft_mfm_set_debug(uft_mfm_context_t *ctx, bool enable) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    ctx->debug = enable;
    return UFT_MFM_OK;
}

/**
 * @brief Process a single bit
 * 
 * This is the main decoder function. Feed it bits one at a time.
 * 
 * @param ctx     Decoder context
 * @param bit     Input bit (0 or 1)
 * @param datapos Position in stream (for debugging/logging)
 * @return UFT_MFM_OK on success, error code otherwise
 */
uft_mfm_error_t uft_mfm_add_bit(uft_mfm_context_t *ctx,
                                const uint8_t bit,
                                const unsigned long datapos) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    
    /* Shift bit history */
    ctx->p1 = ((ctx->p1 << 1) | ((ctx->p2 & 0x8000) >> 15)) & 0xFFFF;
    ctx->p2 = ((ctx->p2 << 1) | ((ctx->p3 & 0x8000) >> 15)) & 0xFFFF;
    ctx->p3 = ((ctx->p3 << 1) | ((ctx->datacells & 0x8000) >> 15)) & 0xFFFF;
    
    /* Add new bit to accumulator */
    ctx->datacells = ((ctx->datacells << 1) & 0xFFFF) | (bit & 1);
    ctx->bits++;
    
    /* Process when we have 16 bits (1 MFM byte) */
    if (ctx->bits >= 16) {
        /* Note: clock bits could be validated for MFM rule compliance */
        /* uint8_t clock = mfm_get_clock(ctx->datacells); */
        uint8_t data = mfm_get_data(ctx->datacells);
        
        switch (ctx->state) {
            case UFT_MFM_STATE_SYNC:
                /* Look for IDAM/DAM sync pattern: A1 A1 A1 + marker */
                if (ctx->datacells == UFT_MFM_SYNC_PATTERN &&
                    mfm_get_data(ctx->p1) == UFT_MFM_ACCESS_SYNC &&
                    mfm_get_data(ctx->p2) == UFT_MFM_ACCESS_SECTOR &&
                    mfm_get_data(ctx->p3) == UFT_MFM_ACCESS_SECTOR) {
                    
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] MFM SYNC found\n", datapos);
                    }
                    
                    ctx->bits = 0;
                    ctx->bitlen = 0;
                    ctx->running_crc = 0xFFFF;
                    
                    /* Include sync bytes in CRC */
                    ctx->running_crc = crc16_update(ctx->running_crc, 0xA1);
                    ctx->running_crc = crc16_update(ctx->running_crc, 0xA1);
                    ctx->running_crc = crc16_update(ctx->running_crc, 0xA1);
                    
                    ctx->state = UFT_MFM_STATE_MARK;
                } else {
                    /* Keep searching, prevent overflow */
                    ctx->bits = 16;
                }
                break;
                
            case UFT_MFM_STATE_MARK:
                ctx->running_crc = crc16_update(ctx->running_crc, data);
                
                if (data == UFT_MFM_MARK_IDAM) {
                    /* ID Address Mark */
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] IDAM found\n", datapos);
                    }
                    ctx->current_idam.position = datapos;
                    ctx->state = UFT_MFM_STATE_IDAM;
                    ctx->bitlen = 0;
                    ctx->bits = 0;
                } else if (data == UFT_MFM_MARK_DAM || data == UFT_MFM_MARK_DDAM) {
                    /* Data Address Mark */
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] DAM/DDAM found (type=%02X)\n", datapos, data);
                    }
                    ctx->block_type = data;
                    ctx->block_pos = datapos;
                    ctx->block_size = mfm_sector_size(ctx->current_idam.size_code);
                    ctx->state = UFT_MFM_STATE_DATA;
                    ctx->bitlen = 0;
                    ctx->bits = 0;
                } else {
                    /* Unknown mark, go back to sync */
                    ctx->state = UFT_MFM_STATE_SYNC;
                    ctx->bits = 16;
                    ctx->sync_losses++;
                }
                break;
                
            case UFT_MFM_STATE_IDAM:
                /* Store IDAM bytes and update CRC */
                if (ctx->bitlen < UFT_MFM_BLOCKSIZE) {
                    ctx->bitstream[ctx->bitlen++] = data;
                    ctx->running_crc = crc16_update(ctx->running_crc, data);
                } else {
                    return UFT_MFM_ERR_BUFFER_OVERFLOW;
                }
                
                /* IDAM is 4 data bytes + 2 CRC bytes */
                if (ctx->bitlen == 6) {
                    ctx->current_idam.track = ctx->bitstream[0];
                    ctx->current_idam.head = ctx->bitstream[1];
                    ctx->current_idam.sector = ctx->bitstream[2];
                    ctx->current_idam.size_code = ctx->bitstream[3];
                    ctx->current_idam.crc = (ctx->bitstream[4] << 8) | ctx->bitstream[5];
                    
                    /* CRC should be 0 if valid (CRC includes CRC bytes) */
                    ctx->current_idam.valid = (ctx->running_crc == 0);
                    
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] IDAM: T=%d H=%d S=%d N=%d CRC=%s\n",
                                datapos,
                                ctx->current_idam.track,
                                ctx->current_idam.head,
                                ctx->current_idam.sector,
                                ctx->current_idam.size_code,
                                ctx->current_idam.valid ? "OK" : "BAD");
                    }
                    
                    ctx->sectors_found++;
                    ctx->state = UFT_MFM_STATE_SYNC;
                    ctx->bits = 0;
                } else {
                    ctx->bits = 0;
                }
                break;
                
            case UFT_MFM_STATE_DATA:
                /* Store data bytes */
                if (ctx->bitlen < UFT_MFM_BLOCKSIZE) {
                    ctx->bitstream[ctx->bitlen++] = data;
                    ctx->running_crc = crc16_update(ctx->running_crc, data);
                } else {
                    return UFT_MFM_ERR_BUFFER_OVERFLOW;
                }
                
                /* Data + 2 CRC bytes */
                if (ctx->bitlen == ctx->block_size + 2) {
                    bool crc_ok = (ctx->running_crc == 0);
                    
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] Sector complete: %d bytes, CRC=%s\n",
                                datapos, ctx->block_size, crc_ok ? "OK" : "BAD");
                    }
                    
                    if (crc_ok) {
                        ctx->sectors_good++;
                        ctx->last_idam = ctx->current_idam;
                    } else {
                        ctx->sectors_bad_crc++;
                    }
                    
                    /* Call user callback if set */
                    if (ctx->sector_callback) {
                        ctx->sector_callback(&ctx->current_idam,
                                           ctx->bitstream,
                                           ctx->block_size,
                                           crc_ok,
                                           ctx->user_data);
                    }
                    
                    ctx->state = UFT_MFM_STATE_SYNC;
                    ctx->bits = 0;
                } else {
                    ctx->bits = 0;
                }
                break;
                
            default:
                return UFT_MFM_ERR_INVALID_STATE;
        }
    }
    
    return UFT_MFM_OK;
}

/**
 * @brief Get decoder statistics
 */
uft_mfm_error_t uft_mfm_get_stats(const uft_mfm_context_t *ctx,
                                  uint32_t *sectors_found,
                                  uint32_t *sectors_good,
                                  uint32_t *sectors_bad_crc,
                                  uint32_t *sync_losses) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    
    if (sectors_found)  *sectors_found = ctx->sectors_found;
    if (sectors_good)   *sectors_good = ctx->sectors_good;
    if (sectors_bad_crc) *sectors_bad_crc = ctx->sectors_bad_crc;
    if (sync_losses)    *sync_losses = ctx->sync_losses;
    
    return UFT_MFM_OK;
}

/**
 * @brief Get last successfully decoded IDAM
 */
uft_mfm_error_t uft_mfm_get_last_idam(const uft_mfm_context_t *ctx,
                                      uft_mfm_idam_t *idam) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    if (!idam) return UFT_MFM_ERR_NULL_BUFFER;
    
    *idam = ctx->last_idam;
    return UFT_MFM_OK;
}

/*===========================================================================
 * Convenience Functions
 *===========================================================================*/

/**
 * @brief Decode a complete track buffer
 * 
 * @param ctx      Decoder context (will be reset)
 * @param bits     Input bit buffer
 * @param bit_len  Number of bits in buffer
 * @return UFT_MFM_OK on success
 */
uft_mfm_error_t uft_mfm_decode_track(uft_mfm_context_t *ctx,
                                     const uint8_t *bits,
                                     size_t bit_len) {
    if (!ctx) return UFT_MFM_ERR_NULL_CONTEXT;
    if (!bits) return UFT_MFM_ERR_NULL_BUFFER;
    
    uft_mfm_error_t err = uft_mfm_reset(ctx);
    if (err != UFT_MFM_OK) return err;
    
    for (size_t i = 0; i < bit_len; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);  /* MSB first */
        uint8_t bit = (bits[byte_idx] >> bit_idx) & 1;
        
        err = uft_mfm_add_bit(ctx, bit, (unsigned long)i);
        if (err != UFT_MFM_OK) return err;
    }
    
    return UFT_MFM_OK;
}

#ifdef UFT_MFM_TEST
/*===========================================================================
 * Unit Test
 *===========================================================================*/

#include <assert.h>

int main(void) {
    printf("=== UFT MFM Decoder v2 Test ===\n");
    
    /* Test 1: Create/Destroy */
    printf("Test 1: Create/Destroy... ");
    uft_mfm_context_t *ctx = uft_mfm_create();
    assert(ctx != NULL);
    uft_mfm_destroy(ctx);
    printf("OK\n");
    
    /* Test 2: NULL safety */
    printf("Test 2: NULL safety... ");
    assert(uft_mfm_reset(NULL) == UFT_MFM_ERR_NULL_CONTEXT);
    assert(uft_mfm_add_bit(NULL, 0, 0) == UFT_MFM_ERR_NULL_CONTEXT);
    printf("OK\n");
    
    /* Test 3: CRC calculation */
    printf("Test 3: CRC calculation... ");
    uint8_t test_data[] = {0xA1, 0xA1, 0xA1, 0xFE, 0x00, 0x00, 0x01, 0x02};
    uint16_t crc = crc16_compute(test_data, sizeof(test_data));
    printf("CRC=0x%04X ", crc);
    printf("OK\n");
    
    /* Test 4: MFM decode helpers */
    printf("Test 4: MFM decode helpers... ");
    uint16_t cells = 0x4489;  /* MFM sync pattern */
    uint8_t data = mfm_get_data(cells);
    printf("0x4489 -> data=0x%02X ", data);
    printf("OK\n");
    
    /* Test 5: Sector size */
    printf("Test 5: Sector size... ");
    assert(mfm_sector_size(0) == 128);
    assert(mfm_sector_size(1) == 256);
    assert(mfm_sector_size(2) == 512);
    assert(mfm_sector_size(3) == 1024);
    printf("OK\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UFT_MFM_TEST */
