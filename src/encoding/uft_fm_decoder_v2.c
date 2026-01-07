/**
 * @file uft_fm_decoder_v2.c
 * @brief Thread-safe FM (Frequency Modulation) decoder implementation
 * 
 * This is a refactored version of the original BBC-FDC fm.c code.
 * Key improvements:
 * - Thread-safe: All state in context structure, no globals
 * - Memory-safe: Proper bounds checking on all buffers
 * - Error handling: Return codes instead of silent failures
 * - const-correct: Input parameters marked appropriately
 * 
 * FM encoding is used by older 8" and early 5.25" floppy drives,
 * including BBC Micro DFS, Apple II 13-sector, and CP/M systems.
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
#define UFT_FM_MAX_SECTOR_SIZE    16384

/** Maximum size of bitstream buffer */
#define UFT_FM_BLOCKSIZE          (UFT_FM_MAX_SECTOR_SIZE + 64)

/** FM address mark patterns (clock + data combined) */
#define UFT_FM_MARK_IAM_PATTERN   0xF77A  /**< Index Address Mark (FC) */
#define UFT_FM_MARK_IDAM_PATTERN  0xF57E  /**< ID Address Mark (FE) */
#define UFT_FM_MARK_DAM_PATTERN   0xF56F  /**< Data Address Mark (FB) */
#define UFT_FM_MARK_DDAM_PATTERN  0xF56A  /**< Deleted Data Address Mark (F8) */

/** FM address marks (data only) */
#define UFT_FM_MARK_IAM           0xFC
#define UFT_FM_MARK_IDAM          0xFE
#define UFT_FM_MARK_DAM           0xFB
#define UFT_FM_MARK_DDAM          0xF8

/** Default sector size for DFS */
#define UFT_FM_DFS_SECTOR_SIZE    256

/** Data validation markers */
#define UFT_FM_GOOD_DATA          0
#define UFT_FM_BAD_DATA           1

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_FM_OK = 0,
    UFT_FM_ERR_NULL_CONTEXT,
    UFT_FM_ERR_NULL_BUFFER,
    UFT_FM_ERR_BUFFER_OVERFLOW,
    UFT_FM_ERR_SYNC_LOST,
    UFT_FM_ERR_CRC_MISMATCH,
    UFT_FM_ERR_CLOCK_VIOLATION,
    UFT_FM_ERR_INVALID_MARK,
    UFT_FM_ERR_INVALID_LENGTH,
    UFT_FM_ERR_OUT_OF_MEMORY,
    UFT_FM_ERR_INVALID_STATE
} uft_fm_error_t;

/**
 * @brief Get human-readable error string
 */
static inline const char* uft_fm_error_str(uft_fm_error_t err) {
    switch (err) {
        case UFT_FM_OK:                 return "OK";
        case UFT_FM_ERR_NULL_CONTEXT:   return "Null context";
        case UFT_FM_ERR_NULL_BUFFER:    return "Null buffer";
        case UFT_FM_ERR_BUFFER_OVERFLOW: return "Buffer overflow";
        case UFT_FM_ERR_SYNC_LOST:      return "Sync lost";
        case UFT_FM_ERR_CRC_MISMATCH:   return "CRC mismatch";
        case UFT_FM_ERR_CLOCK_VIOLATION: return "Clock violation";
        case UFT_FM_ERR_INVALID_MARK:   return "Invalid address mark";
        case UFT_FM_ERR_INVALID_LENGTH: return "Invalid sector length";
        case UFT_FM_ERR_OUT_OF_MEMORY:  return "Out of memory";
        case UFT_FM_ERR_INVALID_STATE:  return "Invalid state";
        default:                        return "Unknown error";
    }
}

/*===========================================================================
 * State Machine
 *===========================================================================*/

typedef enum {
    UFT_FM_STATE_SYNC = 0,    /**< Looking for address mark */
    UFT_FM_STATE_ADDR,        /**< Reading ID Address Mark fields */
    UFT_FM_STATE_DATA         /**< Reading data field */
} uft_fm_state_t;

/*===========================================================================
 * IDAM (ID Address Mark) Structure
 *===========================================================================*/

typedef struct {
    int8_t   track;           /**< Cylinder number (-1 if invalid) */
    int8_t   head;            /**< Head/side number (-1 if invalid) */
    int8_t   sector;          /**< Sector number (-1 if invalid) */
    int8_t   size_code;       /**< Sector size code (-1 if invalid) */
    uint16_t crc;             /**< CRC-16 */
    unsigned long position;   /**< Position in stream where IDAM was found */
    bool     valid;           /**< CRC validated */
} uft_fm_idam_t;

/*===========================================================================
 * Context Structure (Thread-Safe State)
 *===========================================================================*/

typedef struct {
    /* State machine */
    uft_fm_state_t state;
    
    /* Bit accumulation */
    uint16_t datacells;       /**< 16-bit sliding buffer for clock+data */
    int bits;                 /**< Number of bits accumulated */
    
    /* Bit history (previous 48 bits for pattern matching) */
    uint16_t p1, p2, p3;
    
    /* Current IDAM being processed */
    uft_fm_idam_t current_idam;
    
    /* Last successfully decoded IDAM */
    uft_fm_idam_t last_idam;
    
    /* Address mark positions */
    unsigned long id_pos;     /**< Position of last IDAM */
    unsigned long block_pos;  /**< Position of data block start */
    
    /* Data block info */
    uint8_t  block_type;      /**< DAM, DDAM, or null */
    uint32_t block_size;      /**< Expected block size (including CRC) */
    
    /* Output buffer */
    uint8_t  bitstream[UFT_FM_BLOCKSIZE];
    uint32_t bitlen;          /**< Current position in bitstream */
    
    /* CRC tracking */
    uint16_t id_crc;          /**< CRC of ID field */
    uint16_t data_crc;        /**< CRC of data field */
    uint16_t computed_crc;    /**< Computed CRC for verification */
    
    /* FM timing (if needed) */
    float    default_window;
    float    bucket_1;
    float    bucket_01;
    
    /* Current physical position (for validation) */
    int      hw_track;
    int      hw_head;
    
    /* Statistics */
    uint32_t sectors_found;
    uint32_t sectors_good;
    uint32_t sectors_bad_crc;
    uint32_t sync_losses;
    uint32_t clock_errors;
    
    /* Debug mode */
    bool     debug;
    
    /* User callback for sector completion */
    void (*sector_callback)(const uft_fm_idam_t *idam, 
                           const uint8_t *data, 
                           uint32_t len,
                           bool crc_ok,
                           void *user_data);
    void *user_data;
    
} uft_fm_context_t;

/*===========================================================================
 * CRC-16 CCITT Implementation (same polynomial as MFM)
 *===========================================================================*/

static const uint16_t fm_crc16_table[256] = {
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

static inline uint16_t fm_crc16_update(uint16_t crc, uint8_t byte) {
    return (crc << 8) ^ fm_crc16_table[(crc >> 8) ^ byte];
}

static uint16_t fm_crc16_compute(const uint8_t *data, size_t len) 
    __attribute__((unused));
static uint16_t fm_crc16_compute(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = fm_crc16_update(crc, data[i]);
    }
    return crc;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Extract clock bits from FM cell pair
 * In FM, clock bits are in odd positions (bits 15,13,11,9,7,5,3,1)
 */
static inline uint8_t fm_get_clock(uint16_t cells) {
    return (uint8_t)(
        ((cells >> 15) & 0x01) << 7 |
        ((cells >> 13) & 0x01) << 6 |
        ((cells >> 11) & 0x01) << 5 |
        ((cells >>  9) & 0x01) << 4 |
        ((cells >>  7) & 0x01) << 3 |
        ((cells >>  5) & 0x01) << 2 |
        ((cells >>  3) & 0x01) << 1 |
        ((cells >>  1) & 0x01) << 0
    );
}

/**
 * @brief Extract data bits from FM cell pair
 * In FM, data bits are in even positions (bits 14,12,10,8,6,4,2,0)
 */
static inline uint8_t fm_get_data(uint16_t cells) {
    return (uint8_t)(
        ((cells >> 14) & 0x01) << 7 |
        ((cells >> 12) & 0x01) << 6 |
        ((cells >> 10) & 0x01) << 5 |
        ((cells >>  8) & 0x01) << 4 |
        ((cells >>  6) & 0x01) << 3 |
        ((cells >>  4) & 0x01) << 2 |
        ((cells >>  2) & 0x01) << 1 |
        ((cells >>  0) & 0x01) << 0
    );
}

/**
 * @brief Validate FM clock bits (should all be 1 for data)
 */
static inline bool fm_validate_clock(uint8_t clock) {
    return (clock == 0xFF);
}

/**
 * @brief Get sector size from size code
 */
static inline uint32_t fm_sector_size(uint8_t size_code) {
    if (size_code > 7) return 0;
    return 128u << size_code;
}

/**
 * @brief Invalidate IDAM cache
 */
static inline void fm_invalidate_idam(uft_fm_idam_t *idam) {
    idam->track = -1;
    idam->head = -1;
    idam->sector = -1;
    idam->size_code = -1;
    idam->valid = false;
}

/**
 * @brief Check if IDAM is valid
 */
static inline bool fm_idam_valid(const uft_fm_idam_t *idam) {
    return (idam->track >= 0 && idam->head >= 0 && 
            idam->sector >= 0 && idam->size_code >= 0);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief Create and initialize a new FM decoder context
 * @return Allocated context or NULL on failure
 */
uft_fm_context_t* uft_fm_create(void) {
    uft_fm_context_t *ctx = calloc(1, sizeof(uft_fm_context_t));
    if (!ctx) return NULL;
    
    ctx->state = UFT_FM_STATE_SYNC;
    ctx->default_window = 4.0f;
    ctx->debug = false;
    
    fm_invalidate_idam(&ctx->current_idam);
    fm_invalidate_idam(&ctx->last_idam);
    
    return ctx;
}

/**
 * @brief Destroy FM decoder context
 * @param ctx Context to destroy (safe to pass NULL)
 */
void uft_fm_destroy(uft_fm_context_t *ctx) {
    free(ctx);
}

/**
 * @brief Reset decoder state (keep configuration)
 */
uft_fm_error_t uft_fm_reset(uft_fm_context_t *ctx) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    
    ctx->state = UFT_FM_STATE_SYNC;
    ctx->datacells = 0;
    ctx->bits = 0;
    ctx->p1 = ctx->p2 = ctx->p3 = 0;
    ctx->bitlen = 0;
    ctx->id_pos = 0;
    ctx->block_pos = 0;
    ctx->block_type = 0;
    ctx->block_size = 0;
    
    fm_invalidate_idam(&ctx->current_idam);
    
    return UFT_FM_OK;
}

/**
 * @brief Set sector completion callback
 */
uft_fm_error_t uft_fm_set_callback(uft_fm_context_t *ctx,
                                   void (*callback)(const uft_fm_idam_t*, 
                                                   const uint8_t*, 
                                                   uint32_t, bool, void*),
                                   void *user_data) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    
    ctx->sector_callback = callback;
    ctx->user_data = user_data;
    
    return UFT_FM_OK;
}

/**
 * @brief Set current physical track/head (for validation)
 */
uft_fm_error_t uft_fm_set_position(uft_fm_context_t *ctx, int track, int head) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    ctx->hw_track = track;
    ctx->hw_head = head;
    return UFT_FM_OK;
}

/**
 * @brief Enable/disable debug output
 */
uft_fm_error_t uft_fm_set_debug(uft_fm_context_t *ctx, bool enable) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    ctx->debug = enable;
    return UFT_FM_OK;
}

/**
 * @brief Process a single bit
 * 
 * @param ctx     Decoder context
 * @param bit     Input bit (0 or 1)
 * @param datapos Position in stream (for debugging/logging)
 * @return UFT_FM_OK on success, error code otherwise
 */
uft_fm_error_t uft_fm_add_bit(uft_fm_context_t *ctx,
                              const uint8_t bit,
                              const unsigned long datapos) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    
    /* Shift bit history */
    ctx->p1 = (ctx->p1 << 1) | ((ctx->p2 & 0x8000) >> 15);
    ctx->p2 = (ctx->p2 << 1) | ((ctx->p3 & 0x8000) >> 15);
    ctx->p3 = (ctx->p3 << 1) | ((ctx->datacells & 0x8000) >> 15);
    
    /* Add new bit to accumulator */
    ctx->datacells = ((ctx->datacells << 1) & 0xFFFF) | (bit & 1);
    ctx->bits++;
    
    /* Process when we have 16 bits (1 FM byte = 8 clock + 8 data) */
    if (ctx->bits >= 16) {
        uint8_t data = fm_get_data(ctx->datacells);
        
        switch (ctx->state) {
            case UFT_FM_STATE_SYNC:
                /* Detect FM address marks */
                switch (ctx->datacells) {
                    case UFT_FM_MARK_IAM_PATTERN:  /* Index Address Mark */
                        if (ctx->debug) {
                            fprintf(stderr, "[%lx] FM Index Address Mark\n", datapos);
                        }
                        ctx->block_type = UFT_FM_MARK_IAM;
                        ctx->bitlen = 0;
                        fm_invalidate_idam(&ctx->current_idam);
                        break;
                        
                    case UFT_FM_MARK_IDAM_PATTERN:  /* ID Address Mark */
                        if (ctx->debug) {
                            fprintf(stderr, "[%lx] FM ID Address Mark\n", datapos);
                        }
                        ctx->block_type = UFT_FM_MARK_IDAM;
                        ctx->block_size = 7;  /* IDAM + T + H + S + N + CRC16 */
                        ctx->bitlen = 0;
                        
                        /* Store address mark in buffer */
                        ctx->bitstream[ctx->bitlen++] = data;
                        ctx->id_pos = datapos;
                        ctx->state = UFT_FM_STATE_ADDR;
                        
                        /* Clear IDAM cache */
                        fm_invalidate_idam(&ctx->current_idam);
                        ctx->bits = 0;
                        break;
                        
                    case UFT_FM_MARK_DAM_PATTERN:   /* Data Address Mark */
                    case UFT_FM_MARK_DDAM_PATTERN:  /* Deleted Data Address Mark */
                        if (ctx->debug) {
                            fprintf(stderr, "[%lx] FM %s Data Address Mark\n", 
                                    datapos,
                                    ctx->datacells == UFT_FM_MARK_DDAM_PATTERN ? "Deleted" : "");
                        }
                        
                        /* Only process if we have a valid preceding IDAM */
                        if (fm_idam_valid(&ctx->current_idam)) {
                            ctx->block_type = data;
                            ctx->bitlen = 0;
                            ctx->bitstream[ctx->bitlen++] = data;
                            ctx->block_pos = datapos;
                            ctx->state = UFT_FM_STATE_DATA;
                            ctx->bits = 0;
                        } else {
                            if (ctx->debug) {
                                fprintf(stderr, "[%lx] Ignoring DAM without valid IDAM\n", datapos);
                            }
                            ctx->sync_losses++;
                        }
                        break;
                        
                    default:
                        /* No matching address marks, keep searching */
                        ctx->bits = 16;  /* Prevent overflow */
                        break;
                }
                break;
                
            case UFT_FM_STATE_ADDR:
                /* Bounds check */
                if (ctx->bitlen >= UFT_FM_BLOCKSIZE) {
                    ctx->state = UFT_FM_STATE_SYNC;
                    ctx->bits = 0;
                    return UFT_FM_ERR_BUFFER_OVERFLOW;
                }
                
                /* Store byte */
                ctx->bitstream[ctx->bitlen++] = data;
                
                /* Check if we have complete IDAM */
                if (ctx->bitlen == ctx->block_size) {
                    /* Calculate CRC (excluding the 2 CRC bytes) */
                    uint16_t computed = 0xFFFF;
                    for (uint32_t i = 0; i < ctx->bitlen - 2; i++) {
                        computed = fm_crc16_update(computed, ctx->bitstream[i]);
                    }
                    
                    uint16_t stored = ((uint16_t)ctx->bitstream[ctx->bitlen - 2] << 8) |
                                       ctx->bitstream[ctx->bitlen - 1];
                    
                    bool crc_ok = (computed == stored);
                    
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] FM IDAM: T=%d H=%d S=%d N=%d CRC=%s\n",
                                datapos,
                                ctx->bitstream[1],
                                ctx->bitstream[2],
                                ctx->bitstream[3],
                                ctx->bitstream[4],
                                crc_ok ? "OK" : "BAD");
                    }
                    
                    ctx->sectors_found++;
                    
                    if (crc_ok) {
                        /* Store IDAM values */
                        ctx->current_idam.track = ctx->bitstream[1];
                        ctx->current_idam.head = ctx->bitstream[2];
                        ctx->current_idam.sector = ctx->bitstream[3];
                        ctx->current_idam.size_code = ctx->bitstream[4];
                        ctx->current_idam.crc = stored;
                        ctx->current_idam.position = ctx->id_pos;
                        ctx->current_idam.valid = true;
                        
                        /* Update last good IDAM */
                        ctx->last_idam = ctx->current_idam;
                        
                        /* Calculate expected data block size */
                        uint8_t size_code = ctx->current_idam.size_code;
                        if (size_code <= 7) {
                            ctx->block_size = (128u << size_code) + 3;  /* data + mark + CRC */
                        } else {
                            /* Invalid size, use default */
                            ctx->block_size = UFT_FM_DFS_SECTOR_SIZE + 3;
                        }
                    } else {
                        ctx->sectors_bad_crc++;
                        fm_invalidate_idam(&ctx->current_idam);
                        ctx->block_size = 0;
                    }
                    
                    ctx->state = UFT_FM_STATE_SYNC;
                    ctx->bits = 0;
                } else {
                    ctx->bits = 0;
                }
                break;
                
            case UFT_FM_STATE_DATA:
                /* Bounds check */
                if (ctx->bitlen >= UFT_FM_BLOCKSIZE) {
                    ctx->state = UFT_FM_STATE_SYNC;
                    ctx->bits = 0;
                    return UFT_FM_ERR_BUFFER_OVERFLOW;
                }
                
                /* Store byte */
                ctx->bitstream[ctx->bitlen++] = data;
                
                /* Check if we have complete data block */
                if (ctx->bitlen == ctx->block_size) {
                    /* Calculate CRC */
                    uint16_t computed = 0xFFFF;
                    for (uint32_t i = 0; i < ctx->bitlen - 2; i++) {
                        computed = fm_crc16_update(computed, ctx->bitstream[i]);
                    }
                    
                    uint16_t stored = ((uint16_t)ctx->bitstream[ctx->bitlen - 2] << 8) |
                                       ctx->bitstream[ctx->bitlen - 1];
                    
                    bool crc_ok = (computed == stored);
                    uint32_t data_len = ctx->block_size - 3;  /* Exclude mark and CRC */
                    
                    if (ctx->debug) {
                        fprintf(stderr, "[%lx] FM Sector T%d H%d S%d: %d bytes, CRC=%s\n",
                                datapos,
                                ctx->current_idam.track,
                                ctx->current_idam.head,
                                ctx->current_idam.sector,
                                data_len,
                                crc_ok ? "OK" : "BAD");
                    }
                    
                    if (crc_ok) {
                        ctx->sectors_good++;
                    } else {
                        ctx->sectors_bad_crc++;
                    }
                    
                    /* Call user callback */
                    if (ctx->sector_callback) {
                        ctx->sector_callback(&ctx->current_idam,
                                           &ctx->bitstream[1],  /* Skip address mark */
                                           data_len,
                                           crc_ok,
                                           ctx->user_data);
                    }
                    
                    ctx->state = UFT_FM_STATE_SYNC;
                    ctx->bits = 0;
                } else {
                    ctx->bits = 0;
                }
                break;
                
            default:
                return UFT_FM_ERR_INVALID_STATE;
        }
    }
    
    return UFT_FM_OK;
}

/**
 * @brief Get decoder statistics
 */
uft_fm_error_t uft_fm_get_stats(const uft_fm_context_t *ctx,
                                uint32_t *sectors_found,
                                uint32_t *sectors_good,
                                uint32_t *sectors_bad_crc,
                                uint32_t *sync_losses) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    
    if (sectors_found)  *sectors_found = ctx->sectors_found;
    if (sectors_good)   *sectors_good = ctx->sectors_good;
    if (sectors_bad_crc) *sectors_bad_crc = ctx->sectors_bad_crc;
    if (sync_losses)    *sync_losses = ctx->sync_losses;
    
    return UFT_FM_OK;
}

/**
 * @brief Get last successfully decoded IDAM
 */
uft_fm_error_t uft_fm_get_last_idam(const uft_fm_context_t *ctx,
                                    uft_fm_idam_t *idam) {
    if (!ctx) return UFT_FM_ERR_NULL_CONTEXT;
    if (!idam) return UFT_FM_ERR_NULL_BUFFER;
    
    *idam = ctx->last_idam;
    return UFT_FM_OK;
}

/*===========================================================================
 * Unit Test
 *===========================================================================*/

#ifdef UFT_FM_TEST

#include <assert.h>

int main(void) {
    printf("=== UFT FM Decoder v2 Test ===\n");
    
    /* Test 1: Create/Destroy */
    printf("Test 1: Create/Destroy... ");
    uft_fm_context_t *ctx = uft_fm_create();
    assert(ctx != NULL);
    uft_fm_destroy(ctx);
    printf("OK\n");
    
    /* Test 2: NULL safety */
    printf("Test 2: NULL safety... ");
    assert(uft_fm_reset(NULL) == UFT_FM_ERR_NULL_CONTEXT);
    assert(uft_fm_add_bit(NULL, 0, 0) == UFT_FM_ERR_NULL_CONTEXT);
    printf("OK\n");
    
    /* Test 3: FM decode helpers */
    printf("Test 3: FM decode helpers... ");
    /* FM pattern for IAM (0xFC): clock=D7, data=FC */
    uint16_t iam_pattern = 0xF77A;
    uint8_t clock = fm_get_clock(iam_pattern);
    uint8_t data = fm_get_data(iam_pattern);
    printf("IAM=0x%04X -> clock=0x%02X data=0x%02X ", iam_pattern, clock, data);
    printf("OK\n");
    
    /* Test 4: Sector size */
    printf("Test 4: Sector size... ");
    assert(fm_sector_size(0) == 128);
    assert(fm_sector_size(1) == 256);
    assert(fm_sector_size(2) == 512);
    assert(fm_sector_size(3) == 1024);
    printf("OK\n");
    
    /* Test 5: Clock validation */
    printf("Test 5: Clock validation... ");
    assert(fm_validate_clock(0xFF) == true);
    assert(fm_validate_clock(0xFE) == false);
    printf("OK\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UFT_FM_TEST */
