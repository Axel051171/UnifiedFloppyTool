/**
 * @file uft_c64gcr_decoder_v2.c
 * @brief Thread-safe Commodore 64 GCR decoder implementation
 * 
 * Supports:
 * - C64/1541 disk format (35 tracks, variable sectors)
 * - 4/5 GCR encoding
 * - Zone-based bitrates (250-307 kbit/s)
 * 
 * Key improvements:
 * - Thread-safe: All state in context structure
 * - Memory-safe: Bounds checking on all buffers
 * - Error handling: Explicit return codes
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

/** C64 disk geometry */
#define UFT_C64_TRACKS           35
#define UFT_C64_SECTOR_SIZE      256
#define UFT_C64_MAX_SECTORS      21  /**< Max sectors per track (zone 1) */

/** GCR sync pattern */
#define UFT_C64_SYNC_BYTE        0xFF
#define UFT_C64_SYNC_COUNT       5   /**< Minimum sync bytes */

/** Block types */
#define UFT_C64_BLOCK_HEADER     0x08
#define UFT_C64_BLOCK_DATA       0x07

/** Buffer sizes */
#define UFT_C64_GCR_BUFFER_SIZE  (1024 * 1024)
#define UFT_C64_DECODE_BUFFER    512

/** Zone bitrates (bits per second) */
#define UFT_C64_ZONE1_BITRATE    307692  /**< Tracks 1-17: 21 sectors */
#define UFT_C64_ZONE2_BITRATE    285714  /**< Tracks 18-24: 19 sectors */
#define UFT_C64_ZONE3_BITRATE    266667  /**< Tracks 25-30: 18 sectors */
#define UFT_C64_ZONE4_BITRATE    250000  /**< Tracks 31-35: 17 sectors */

/*===========================================================================
 * GCR 4/5 Tables
 *===========================================================================*/

/**
 * 4-to-5 GCR encoding table
 * Each nibble (0-15) encodes to 5 bits
 */
static const uint8_t gcr_encode_table[16] = {
    0x0A, 0x0B, 0x12, 0x13,  /* 0-3:  01010, 01011, 10010, 10011 */
    0x0E, 0x0F, 0x16, 0x17,  /* 4-7:  01110, 01111, 10110, 10111 */
    0x09, 0x19, 0x1A, 0x1B,  /* 8-B:  01001, 11001, 11010, 11011 */
    0x0D, 0x1D, 0x1E, 0x15   /* C-F:  01101, 11101, 11110, 10101 */
};

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_C64_OK = 0,
    UFT_C64_ERR_NULL_CONTEXT,
    UFT_C64_ERR_NULL_BUFFER,
    UFT_C64_ERR_BUFFER_OVERFLOW,
    UFT_C64_ERR_SYNC_LOST,
    UFT_C64_ERR_CHECKSUM,
    UFT_C64_ERR_INVALID_GCR,
    UFT_C64_ERR_INVALID_TRACK,
    UFT_C64_ERR_INVALID_SECTOR,
    UFT_C64_ERR_OUT_OF_MEMORY,
    UFT_C64_ERR_INVALID_STATE
} uft_c64_error_t;

static inline const char* uft_c64_error_str(uft_c64_error_t err) {
    switch (err) {
        case UFT_C64_OK:               return "OK";
        case UFT_C64_ERR_NULL_CONTEXT: return "Null context";
        case UFT_C64_ERR_NULL_BUFFER:  return "Null buffer";
        case UFT_C64_ERR_BUFFER_OVERFLOW: return "Buffer overflow";
        case UFT_C64_ERR_SYNC_LOST:    return "Sync lost";
        case UFT_C64_ERR_CHECKSUM:     return "Checksum error";
        case UFT_C64_ERR_INVALID_GCR:  return "Invalid GCR";
        case UFT_C64_ERR_INVALID_TRACK: return "Invalid track";
        case UFT_C64_ERR_INVALID_SECTOR: return "Invalid sector";
        case UFT_C64_ERR_OUT_OF_MEMORY: return "Out of memory";
        case UFT_C64_ERR_INVALID_STATE: return "Invalid state";
        default:                        return "Unknown error";
    }
}

/*===========================================================================
 * State Machine
 *===========================================================================*/

typedef enum {
    UFT_C64_STATE_SYNC = 0,    /**< Looking for sync bytes */
    UFT_C64_STATE_HEADER,      /**< Reading header block */
    UFT_C64_STATE_DATA         /**< Reading data block */
} uft_c64_state_t;

/*===========================================================================
 * Header Structure
 *===========================================================================*/

typedef struct {
    uint8_t  block_type;      /**< Should be 0x08 */
    uint8_t  checksum;        /**< XOR of track, sector, id1, id2 */
    uint8_t  sector;          /**< Sector number */
    uint8_t  track;           /**< Track number (1-35) */
    uint8_t  id1;             /**< Disk ID byte 1 */
    uint8_t  id2;             /**< Disk ID byte 2 */
    unsigned long position;   /**< Stream position */
    bool     valid;           /**< Checksum verified */
} uft_c64_header_t;

/*===========================================================================
 * Context Structure (Thread-Safe State)
 *===========================================================================*/

typedef struct {
    /* State machine */
    uft_c64_state_t state;
    
    /* Bit accumulation */
    uint64_t datacells;       /**< Bit shift register */
    int bits;                 /**< Bits accumulated */
    int sync_count;           /**< Consecutive sync bytes */
    
    /* GCR decode table (built on init) */
    uint8_t gcr_decode[32];   /**< 5-bit to 4-bit decode */
    
    /* Current header */
    uft_c64_header_t current_header;
    
    /* Last good header */
    uft_c64_header_t last_header;
    
    /* Positions */
    unsigned long header_pos;
    unsigned long data_pos;
    
    /* GCR buffer (accumulates 5-bit groups) */
    uint8_t  gcr_buffer[UFT_C64_GCR_BUFFER_SIZE];
    uint32_t gcr_len;
    
    /* Decoded data buffer */
    uint8_t  decoded[UFT_C64_DECODE_BUFFER];
    uint32_t decoded_len;
    
    /* Zone detection */
    int      current_zone;    /**< 1-4 based on track */
    uint32_t expected_bitrate;
    
    /* Statistics */
    uint32_t sectors_found;
    uint32_t sectors_good;
    uint32_t sectors_bad;
    uint32_t sync_losses;
    uint32_t gcr_errors;
    
    /* Debug */
    bool     debug;
    
    /* User callback */
    void (*sector_callback)(const uft_c64_header_t *header,
                           const uint8_t *data,
                           uint32_t len,
                           bool checksum_ok,
                           void *user_data);
    void *user_data;
    
} uft_c64_context_t;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Build GCR decode table from encode table
 */
static void build_gcr_decode_table(uint8_t *decode) {
    memset(decode, 0xFF, 32);  /* Invalid entries */
    for (int i = 0; i < 16; i++) {
        decode[gcr_encode_table[i]] = (uint8_t)i;
    }
}

/**
 * @brief Get sectors per track for given track number
 */
static inline int get_sectors_for_track(int track) {
    if (track >= 1 && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 35) return 17;
    return 0;  /* Invalid track */
}

/**
 * @brief Get zone (1-4) for given track
 */
static inline int get_zone_for_track(int track) {
    if (track >= 1 && track <= 17) return 1;
    if (track >= 18 && track <= 24) return 2;
    if (track >= 25 && track <= 30) return 3;
    if (track >= 31 && track <= 35) return 4;
    return 0;
}

/**
 * @brief Get bitrate for zone
 */
static inline uint32_t get_bitrate_for_zone(int zone) {
    switch (zone) {
        case 1: return UFT_C64_ZONE1_BITRATE;
        case 2: return UFT_C64_ZONE2_BITRATE;
        case 3: return UFT_C64_ZONE3_BITRATE;
        case 4: return UFT_C64_ZONE4_BITRATE;
        default: return UFT_C64_ZONE4_BITRATE;
    }
}

/**
 * @brief Decode GCR bytes to data bytes
 * 
 * 5 GCR bytes -> 4 data bytes
 */
static bool decode_gcr_group(const uft_c64_context_t *ctx,
                             const uint8_t *gcr, 
                             uint8_t *data) {
    /* Extract 5-bit values from 5 GCR bytes (40 bits -> 8 nibbles) */
    uint64_t bits = 0;
    for (int i = 0; i < 5; i++) {
        bits = (bits << 8) | gcr[i];
    }
    
    /* Extract 8 nibbles (5 bits each) */
    uint8_t nibbles[8];
    for (int i = 7; i >= 0; i--) {
        nibbles[i] = ctx->gcr_decode[bits & 0x1F];
        if (nibbles[i] == 0xFF) return false;  /* Invalid GCR */
        bits >>= 5;
    }
    
    /* Combine nibbles into 4 bytes */
    data[0] = (nibbles[0] << 4) | nibbles[1];
    data[1] = (nibbles[2] << 4) | nibbles[3];
    data[2] = (nibbles[4] << 4) | nibbles[5];
    data[3] = (nibbles[6] << 4) | nibbles[7];
    
    return true;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief Create C64 GCR decoder context
 */
uft_c64_context_t* uft_c64_create(void) {
    uft_c64_context_t *ctx = calloc(1, sizeof(uft_c64_context_t));
    if (!ctx) return NULL;
    
    build_gcr_decode_table(ctx->gcr_decode);
    ctx->state = UFT_C64_STATE_SYNC;
    ctx->debug = false;
    
    return ctx;
}

/**
 * @brief Destroy context
 */
void uft_c64_destroy(uft_c64_context_t *ctx) {
    free(ctx);
}

/**
 * @brief Reset decoder
 */
uft_c64_error_t uft_c64_reset(uft_c64_context_t *ctx) {
    if (!ctx) return UFT_C64_ERR_NULL_CONTEXT;
    
    ctx->state = UFT_C64_STATE_SYNC;
    ctx->datacells = 0;
    ctx->bits = 0;
    ctx->sync_count = 0;
    ctx->gcr_len = 0;
    ctx->decoded_len = 0;
    
    memset(&ctx->current_header, 0, sizeof(ctx->current_header));
    
    return UFT_C64_OK;
}

/**
 * @brief Set callback
 */
uft_c64_error_t uft_c64_set_callback(uft_c64_context_t *ctx,
                                     void (*callback)(const uft_c64_header_t*,
                                                     const uint8_t*,
                                                     uint32_t, bool, void*),
                                     void *user_data) {
    if (!ctx) return UFT_C64_ERR_NULL_CONTEXT;
    ctx->sector_callback = callback;
    ctx->user_data = user_data;
    return UFT_C64_OK;
}

/**
 * @brief Set debug mode
 */
uft_c64_error_t uft_c64_set_debug(uft_c64_context_t *ctx, bool enable) {
    if (!ctx) return UFT_C64_ERR_NULL_CONTEXT;
    ctx->debug = enable;
    return UFT_C64_OK;
}

/**
 * @brief Process a single bit
 */
uft_c64_error_t uft_c64_add_bit(uft_c64_context_t *ctx,
                                const uint8_t bit,
                                const unsigned long datapos) {
    if (!ctx) return UFT_C64_ERR_NULL_CONTEXT;
    
    /* Shift bit into accumulator */
    ctx->datacells = ((ctx->datacells << 1) | (bit & 1));
    ctx->bits++;
    
    /* Look for complete byte */
    if (ctx->bits >= 8) {
        uint8_t byte = ctx->datacells & 0xFF;
        ctx->bits = 0;
        
        switch (ctx->state) {
            case UFT_C64_STATE_SYNC:
                if (byte == UFT_C64_SYNC_BYTE) {
                    ctx->sync_count++;
                } else if (ctx->sync_count >= UFT_C64_SYNC_COUNT) {
                    /* End of sync, check block type */
                    ctx->sync_count = 0;
                    ctx->gcr_len = 0;
                    
                    /* Store first GCR byte */
                    if (ctx->gcr_len < UFT_C64_GCR_BUFFER_SIZE) {
                        ctx->gcr_buffer[ctx->gcr_len++] = byte;
                    }
                    
                    /* Need 10 GCR bytes for header (8 data bytes) */
                    /* or 325 GCR bytes for data (260 bytes: 1+256+1+2) */
                    ctx->state = UFT_C64_STATE_HEADER;
                    ctx->header_pos = datapos;
                } else {
                    ctx->sync_count = 0;
                }
                break;
                
            case UFT_C64_STATE_HEADER:
                if (ctx->gcr_len < UFT_C64_GCR_BUFFER_SIZE) {
                    ctx->gcr_buffer[ctx->gcr_len++] = byte;
                }
                
                /* Header: 10 GCR bytes = 8 data bytes */
                if (ctx->gcr_len >= 10) {
                    uint8_t header_data[8];
                    bool ok = true;
                    
                    /* Decode 2 groups of 5 GCR bytes */
                    ok = ok && decode_gcr_group(ctx, &ctx->gcr_buffer[0], &header_data[0]);
                    ok = ok && decode_gcr_group(ctx, &ctx->gcr_buffer[5], &header_data[4]);
                    
                    if (ok) {
                        ctx->current_header.block_type = header_data[0];
                        ctx->current_header.checksum = header_data[1];
                        ctx->current_header.sector = header_data[2];
                        ctx->current_header.track = header_data[3];
                        ctx->current_header.id2 = header_data[4];
                        ctx->current_header.id1 = header_data[5];
                        ctx->current_header.position = ctx->header_pos;
                        
                        /* Verify checksum */
                        uint8_t computed = ctx->current_header.sector ^
                                          ctx->current_header.track ^
                                          ctx->current_header.id1 ^
                                          ctx->current_header.id2;
                        ctx->current_header.valid = 
                            (ctx->current_header.block_type == UFT_C64_BLOCK_HEADER) &&
                            (computed == ctx->current_header.checksum);
                        
                        if (ctx->debug) {
                            fprintf(stderr, "[%lx] C64 Header: T=%d S=%d ID=%02X%02X %s\n",
                                    datapos,
                                    ctx->current_header.track,
                                    ctx->current_header.sector,
                                    ctx->current_header.id1,
                                    ctx->current_header.id2,
                                    ctx->current_header.valid ? "OK" : "BAD");
                        }
                        
                        ctx->sectors_found++;
                        if (ctx->current_header.valid) {
                            ctx->last_header = ctx->current_header;
                            ctx->current_zone = get_zone_for_track(ctx->current_header.track);
                        }
                    } else {
                        ctx->gcr_errors++;
                    }
                    
                    ctx->state = UFT_C64_STATE_SYNC;
                    ctx->gcr_len = 0;
                }
                break;
                
            case UFT_C64_STATE_DATA:
                /* Data block handling would go here */
                /* 325 GCR bytes -> 260 data bytes (1 + 256 + 1 + 2 padding) */
                ctx->state = UFT_C64_STATE_SYNC;
                break;
        }
    }
    
    return UFT_C64_OK;
}

/**
 * @brief Get statistics
 */
uft_c64_error_t uft_c64_get_stats(const uft_c64_context_t *ctx,
                                  uint32_t *found, uint32_t *good,
                                  uint32_t *bad, uint32_t *gcr_errors) {
    if (!ctx) return UFT_C64_ERR_NULL_CONTEXT;
    
    if (found) *found = ctx->sectors_found;
    if (good)  *good = ctx->sectors_good;
    if (bad)   *bad = ctx->sectors_bad;
    if (gcr_errors) *gcr_errors = ctx->gcr_errors;
    
    return UFT_C64_OK;
}

/*===========================================================================
 * Unit Test
 *===========================================================================*/

#ifdef UFT_C64_TEST

#include <assert.h>

int main(void) {
    printf("=== UFT C64 GCR Decoder v2 Test ===\n");
    
    /* Test 1: Create/Destroy */
    printf("Test 1: Create/Destroy... ");
    uft_c64_context_t *ctx = uft_c64_create();
    assert(ctx != NULL);
    uft_c64_destroy(ctx);
    printf("OK\n");
    
    /* Test 2: NULL safety */
    printf("Test 2: NULL safety... ");
    assert(uft_c64_reset(NULL) == UFT_C64_ERR_NULL_CONTEXT);
    printf("OK\n");
    
    /* Test 3: GCR decode table */
    printf("Test 3: GCR decode table... ");
    ctx = uft_c64_create();
    assert(ctx->gcr_decode[0x0A] == 0x00);  /* 01010 -> 0 */
    assert(ctx->gcr_decode[0x0B] == 0x01);  /* 01011 -> 1 */
    assert(ctx->gcr_decode[0x15] == 0x0F);  /* 10101 -> F */
    assert(ctx->gcr_decode[0x00] == 0xFF);  /* Invalid */
    uft_c64_destroy(ctx);
    printf("OK\n");
    
    /* Test 4: Track zones */
    printf("Test 4: Track zones... ");
    assert(get_zone_for_track(1) == 1);
    assert(get_zone_for_track(17) == 1);
    assert(get_zone_for_track(18) == 2);
    assert(get_zone_for_track(31) == 4);
    assert(get_sectors_for_track(1) == 21);
    assert(get_sectors_for_track(35) == 17);
    printf("OK\n");
    
    /* Test 5: Bitrates */
    printf("Test 5: Bitrates... ");
    assert(get_bitrate_for_zone(1) == 307692);
    assert(get_bitrate_for_zone(4) == 250000);
    printf("OK\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UFT_C64_TEST */
