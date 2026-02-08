/**
 * @file uft_crc_aligned.h
 * @brief CRC-16 calculation with bit-alignment support
 * 
 * Fixes algorithmic weakness #3: CRC bit-alignment errors
 * 
 * Features:
 * - Bit-level CRC calculation
 * - Auto-alignment detection
 * - Multiple polynomial support (CCITT, IBM, DNP)
 * - Streaming and block modes
 */

#ifndef UFT_CRC_ALIGNED_H
#define UFT_CRC_ALIGNED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard CRC-16 polynomials */
#define UFT_CRC16_CCITT     0x1021  /* x^16 + x^12 + x^5 + 1 (IBM floppy) */
#define UFT_CRC16_IBM       0x8005  /* x^16 + x^15 + x^2 + 1 */
#define UFT_CRC16_DNP       0x3D65  /* DNP protocol */

/* Initial values */
#define UFT_CRC16_INIT_FFFF 0xFFFF  /* Standard for CCITT */
#define UFT_CRC16_INIT_0000 0x0000

/**
 * @brief CRC calculation context
 */
typedef struct {
    uint16_t crc;           /**< Current CRC value */
    uint16_t polynomial;    /**< Polynomial in use */
    uint16_t init_value;    /**< Initial value */
    bool     reflect_in;    /**< Reflect input bytes */
    bool     reflect_out;   /**< Reflect output */
    uint16_t xor_out;       /**< XOR output value */
    
    /* Bit-level state */
    uint8_t  bit_buffer;    /**< Accumulated bits */
    uint8_t  bit_count;     /**< Bits in buffer (0-7) */
    
} uft_crc16_ctx_t;

/**
 * @brief Alignment search result
 */
typedef struct {
    int      offset;        /**< Bit offset that matched (-7 to +7) */
    uint16_t crc;           /**< Calculated CRC at this offset */
    bool     found;         /**< True if valid alignment found */
    uint8_t  confidence;    /**< Confidence 0-100 */
} uft_crc_alignment_t;

/* ============================================================================
 * Standard CRC Functions
 * ============================================================================ */

/**
 * @brief Initialize CRC context with CCITT parameters (IBM floppy standard)
 */
void uft_crc16_init_ccitt(uft_crc16_ctx_t *ctx);

/**
 * @brief Initialize CRC context with custom parameters
 */
void uft_crc16_init(uft_crc16_ctx_t *ctx,
                    uint16_t polynomial,
                    uint16_t init_value,
                    bool reflect_in,
                    bool reflect_out,
                    uint16_t xor_out);

/**
 * @brief Reset CRC to initial value
 */
void uft_crc16_reset(uft_crc16_ctx_t *ctx);

/**
 * @brief Process a single byte
 */
void uft_crc16_byte(uft_crc16_ctx_t *ctx, uint8_t byte);

/**
 * @brief Process a buffer of bytes
 */
void uft_crc16_block(uft_crc16_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Get final CRC value
 */
uint16_t uft_crc16_final(uft_crc16_ctx_t *ctx);

/**
 * @brief Calculate CRC in one call (convenience function)
 */
uint16_t uft_crc16_calc(const uint8_t *data, size_t len);

/* ============================================================================
 * Bit-Level CRC Functions
 * ============================================================================ */

/**
 * @brief Process a single bit
 */
void uft_crc16_bit(uft_crc16_ctx_t *ctx, uint8_t bit);

/**
 * @brief Process multiple bits from a byte (MSB first)
 * @param ctx CRC context
 * @param bits Bit data
 * @param count Number of bits to process (1-8)
 */
void uft_crc16_bits(uft_crc16_ctx_t *ctx, uint8_t bits, uint8_t count);

/**
 * @brief Calculate CRC with bit offset
 * @param data Data buffer
 * @param len_bytes Length in bytes
 * @param bit_offset Bit offset to start from (-7 to +7)
 * @param polynomial CRC polynomial
 * @param init Initial value
 * @return Calculated CRC
 */
uint16_t uft_crc16_with_offset(const uint8_t *data,
                                size_t len_bytes,
                                int bit_offset,
                                uint16_t polynomial,
                                uint16_t init);

/* ============================================================================
 * Auto-Alignment Functions
 * ============================================================================ */

/**
 * @brief Find bit alignment that produces expected CRC
 * @param data Data buffer
 * @param len_bytes Length in bytes
 * @param expected_crc Expected CRC value
 * @param max_offset Maximum offset to try (typically 3-7)
 * @return Alignment result
 */
uft_crc_alignment_t uft_crc16_find_alignment(const uint8_t *data,
                                              size_t len_bytes,
                                              uint16_t expected_crc,
                                              int max_offset);

/**
 * @brief Verify CRC with auto-alignment
 * @param data Data buffer
 * @param len_bytes Length in bytes (including 2-byte CRC at end)
 * @param out_offset Output: detected offset (if any)
 * @return true if CRC valid (possibly with offset)
 */
bool uft_crc16_verify_auto(const uint8_t *data,
                           size_t len_bytes,
                           int *out_offset);

/* ============================================================================
 * IBM Floppy Specific
 * ============================================================================ */

/**
 * @brief Calculate CRC for IBM MFM sector header
 * @param c Cylinder
 * @param h Head
 * @param r Sector
 * @param n Size code
 * @return CRC-16 CCITT
 */
uint16_t uft_crc16_sector_header(uint8_t c, uint8_t h, uint8_t r, uint8_t n);

/**
 * @brief Calculate CRC for IBM MFM sector data
 * @param data Sector data (not including DAM byte)
 * @param len Data length
 * @param dam Data Address Mark (0xFB or 0xF8)
 * @return CRC-16 CCITT
 */
uint16_t uft_crc16_sector_data(const uint8_t *data, size_t len, uint8_t dam);

/**
 * @brief Verify IBM sector with CRC
 * @param header_plus_crc 6 bytes: C,H,R,N,CRC_H,CRC_L
 * @return true if valid
 */
bool uft_crc16_verify_header(const uint8_t *header_plus_crc);

/**
 * @brief Verify IBM sector data with CRC
 * @param dam_plus_data_plus_crc DAM byte + data + 2 CRC bytes
 * @param total_len Total length
 * @return true if valid
 */
bool uft_crc16_verify_data(const uint8_t *dam_plus_data_plus_crc, size_t total_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_ALIGNED_H */
