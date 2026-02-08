/**
 * @file uft_gcr_unified.h
 * @brief Unified GCR encoding/decoding for all platforms
 * 
 * P2-002: Consolidated GCR module replacing multiple implementations
 * 
 * Supported encodings:
 * - Commodore GCR (C64, C128, VIC-20, PET)
 * - Apple II GCR (5&3, 6&2)
 * - Victor 9000 GCR
 * 
 * Features:
 * - Encode/decode lookup tables
 * - Illegal GCR handling
 * - Sync pattern detection
 * - Error detection & correction
 */

#ifndef UFT_GCR_UNIFIED_H
#define UFT_GCR_UNIFIED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * GCR Type Selection
 * ============================================================================ */

typedef enum {
    GCR_TYPE_C64,           /**< Commodore 64/1541 */
    GCR_TYPE_APPLE_62,      /**< Apple II 6&2 (DOS 3.3/ProDOS) */
    GCR_TYPE_APPLE_53,      /**< Apple II 5&3 (DOS 3.2) */
    GCR_TYPE_VICTOR,        /**< Victor 9000 */
} gcr_type_t;

/* ============================================================================
 * Commodore GCR (4-to-5 encoding)
 * ============================================================================ */

/* Sync patterns */
#define C64_GCR_SYNC        0xFF    /* 10 consecutive 1-bits */
#define C64_GCR_SYNC_LEN    10      /* Minimum sync 1-bits */

/* Header block markers */
#define C64_HEADER_ID       0x08    /* Sector header ID */
#define C64_DATA_ID         0x07    /* Sector data ID */

/**
 * @brief Commodore GCR encode table (4-bit -> 5-bit)
 */
extern const uint8_t gcr_encode_c64[16];

/**
 * @brief Commodore GCR decode table (5-bit -> 4-bit, 0xFF = invalid)
 */
extern const uint8_t gcr_decode_c64[32];

/**
 * @brief Encode 4 bytes to 5 GCR bytes (C64)
 */
void gcr_encode_c64_4to5(const uint8_t *src, uint8_t *dst);

/**
 * @brief Decode 5 GCR bytes to 4 bytes (C64)
 * @return 0 on success, -1 on invalid GCR
 */
int gcr_decode_c64_5to4(const uint8_t *src, uint8_t *dst);

/**
 * @brief Encode complete sector (256 bytes -> 325 bytes)
 */
size_t gcr_encode_c64_sector(const uint8_t *data, size_t data_len,
                             uint8_t *gcr, size_t gcr_capacity);

/**
 * @brief Decode complete sector
 */
int gcr_decode_c64_sector(const uint8_t *gcr, size_t gcr_len,
                          uint8_t *data, size_t *data_len);

/**
 * @brief Check if GCR nibble is valid (C64)
 */
bool gcr_valid_c64(uint8_t nibble);

/**
 * @brief Count illegal GCR nibbles in data
 */
int gcr_count_illegal_c64(const uint8_t *gcr, size_t len);

/* ============================================================================
 * Apple II GCR (6&2 encoding)
 * ============================================================================ */

/* Sync patterns */
#define APPLE_SYNC_BYTE     0xFF
#define APPLE_ADDR_PROLOGUE "\xD5\xAA\x96"
#define APPLE_DATA_PROLOGUE "\xD5\xAA\xAD"
#define APPLE_EPILOGUE      "\xDE\xAA\xEB"

/**
 * @brief Apple 6&2 GCR encode table (6-bit -> 8-bit)
 */
extern const uint8_t gcr_encode_apple_62[64];

/**
 * @brief Apple 6&2 GCR decode table (8-bit -> 6-bit, 0xFF = invalid)
 */
extern const uint8_t gcr_decode_apple_62[256];

/**
 * @brief Encode using Apple 6&2 GCR
 */
uint8_t gcr_encode_apple_62_byte(uint8_t val);

/**
 * @brief Decode Apple 6&2 GCR byte
 * @return Decoded value or 0xFF if invalid
 */
uint8_t gcr_decode_apple_62_byte(uint8_t gcr);

/**
 * @brief Check if byte is valid Apple GCR
 */
bool gcr_valid_apple_62(uint8_t byte);

/**
 * @brief Encode 256-byte sector to 342 GCR bytes (6&2)
 */
void gcr_encode_apple_62_sector(const uint8_t *data, uint8_t *gcr);

/**
 * @brief Decode 342 GCR bytes to 256-byte sector
 */
int gcr_decode_apple_62_sector(const uint8_t *gcr, uint8_t *data);

/* ============================================================================
 * Apple II GCR (5&3 encoding) - DOS 3.2
 * ============================================================================ */

/**
 * @brief Apple 5&3 GCR encode table (5-bit -> 8-bit)
 */
extern const uint8_t gcr_encode_apple_53[32];

/**
 * @brief Apple 5&3 GCR decode table
 */
extern const uint8_t gcr_decode_apple_53[256];

/**
 * @brief Encode using Apple 5&3 GCR
 */
uint8_t gcr_encode_apple_53_byte(uint8_t val);

/**
 * @brief Decode Apple 5&3 GCR byte
 */
uint8_t gcr_decode_apple_53_byte(uint8_t gcr);

/* ============================================================================
 * Victor 9000 GCR
 * ============================================================================ */

/**
 * @brief Victor 9000 GCR encode table
 */
extern const uint8_t gcr_encode_victor[16];

/**
 * @brief Victor 9000 GCR decode table
 */
extern const uint8_t gcr_decode_victor[32];

/* ============================================================================
 * Generic GCR Operations
 * ============================================================================ */

/**
 * @brief GCR encoder/decoder context
 */
typedef struct {
    gcr_type_t type;
    const uint8_t *encode_table;
    const uint8_t *decode_table;
    size_t encode_bits;     /* Input bits per symbol */
    size_t decode_bits;     /* Output bits per symbol */
} gcr_context_t;

/**
 * @brief Initialize GCR context for specific type
 */
void gcr_init(gcr_context_t *ctx, gcr_type_t type);

/**
 * @brief Encode data using context
 */
size_t gcr_encode(const gcr_context_t *ctx,
                  const uint8_t *data, size_t data_len,
                  uint8_t *gcr, size_t gcr_capacity);

/**
 * @brief Decode GCR using context
 */
int gcr_decode(const gcr_context_t *ctx,
               const uint8_t *gcr, size_t gcr_len,
               uint8_t *data, size_t *data_len);

/* ============================================================================
 * Sync Pattern Detection
 * ============================================================================ */

/**
 * @brief Find sync pattern in bitstream
 * @param bits Raw bitstream
 * @param bit_count Number of bits
 * @param sync_pattern Pattern to find
 * @param sync_len Pattern length in bits
 * @param start_bit Starting bit position
 * @return Bit position of sync or -1 if not found
 */
int gcr_find_sync(const uint8_t *bits, size_t bit_count,
                  uint32_t sync_pattern, int sync_len,
                  size_t start_bit);

/**
 * @brief Count sync patterns in track
 */
int gcr_count_syncs(const uint8_t *bits, size_t bit_count,
                    gcr_type_t type);

/**
 * @brief Find all sector headers in track
 */
typedef struct {
    size_t bit_offset;
    uint8_t track;
    uint8_t sector;
    uint8_t checksum;
    bool valid;
} gcr_sector_header_t;

int gcr_find_headers(const uint8_t *bits, size_t bit_count,
                     gcr_type_t type,
                     gcr_sector_header_t *headers, size_t max_headers);

/* ============================================================================
 * Error Detection & Correction
 * ============================================================================ */

/**
 * @brief GCR decode result
 */
typedef struct {
    bool success;
    int error_count;
    int corrected_count;
    size_t first_error_pos;
} gcr_decode_result_t;

/**
 * @brief Decode with error detection
 */
gcr_decode_result_t gcr_decode_ex(const gcr_context_t *ctx,
                                   const uint8_t *gcr, size_t gcr_len,
                                   uint8_t *data, size_t *data_len);

/**
 * @brief Attempt to correct illegal GCR using probability
 * @param gcr GCR data (modified in place)
 * @param confidence Per-byte confidence (optional)
 * @return Number of corrections made
 */
int gcr_correct_illegal(uint8_t *gcr, size_t len,
                        const uint8_t *confidence,
                        gcr_type_t type);

/* ============================================================================
 * Track Building
 * ============================================================================ */

/**
 * @brief Build complete C64 sector
 */
size_t gcr_build_c64_sector(uint8_t track, uint8_t sector,
                            uint8_t id1, uint8_t id2,
                            const uint8_t *data,
                            uint8_t *output, size_t output_capacity);

/**
 * @brief Build complete Apple sector
 */
size_t gcr_build_apple_sector(uint8_t volume, uint8_t track, uint8_t sector,
                              const uint8_t *data,
                              uint8_t *output, size_t output_capacity);

/**
 * @brief Calculate required track size
 */
size_t gcr_track_size(gcr_type_t type, int sectors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCR_UNIFIED_H */
