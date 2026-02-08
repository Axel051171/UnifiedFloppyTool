/**
 * @file uft_bits.h
 * @brief Common bit manipulation and byte-order functions
 * 
 * Consolidates frequently duplicated static inline functions:
 * - set_bit / get_bit / clear_bit
 * - read_le16/32 / write_le16/32
 * - read_be16/32 / write_be16/32
 * 
 * Include this instead of defining locally.
 */
#ifndef UFT_BITS_H
#define UFT_BITS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * BIT MANIPULATION
 * ============================================================================= */

/**
 * @brief Set bit at position in byte array
 */
static inline void uft_set_bit(uint8_t *buf, size_t bit_pos, bool value) {
    size_t byte_pos = bit_pos >> 3;
    uint8_t bit_mask = 0x80 >> (bit_pos & 7);
    if (value) {
        buf[byte_pos] |= bit_mask;
    } else {
        buf[byte_pos] &= ~bit_mask;
    }
}

/**
 * @brief Get bit at position from byte array
 */
static inline bool uft_get_bit(const uint8_t *buf, size_t bit_pos) {
    size_t byte_pos = bit_pos >> 3;
    uint8_t bit_mask = 0x80 >> (bit_pos & 7);
    return (buf[byte_pos] & bit_mask) != 0;
}

/**
 * @brief Clear bit at position
 */
static inline void uft_clear_bit(uint8_t *buf, size_t bit_pos) {
    uft_set_bit(buf, bit_pos, false);
}

/**
 * @brief Toggle bit at position
 */
static inline void uft_toggle_bit(uint8_t *buf, size_t bit_pos) {
    size_t byte_pos = bit_pos >> 3;
    uint8_t bit_mask = 0x80 >> (bit_pos & 7);
    buf[byte_pos] ^= bit_mask;
}

/* =============================================================================
 * LITTLE-ENDIAN READ/WRITE
 * ============================================================================= */

static inline uint16_t uft_read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t uft_read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | 
           ((uint32_t)p[3] << 24);
}

static inline uint64_t uft_read_le64(const uint8_t *p) {
    return (uint64_t)uft_read_le32(p) | 
           ((uint64_t)uft_read_le32(p + 4) << 32);
}

static inline void uft_write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static inline void uft_write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static inline void uft_write_le64(uint8_t *p, uint64_t v) {
    uft_write_le32(p, (uint32_t)v);
    uft_write_le32(p + 4, (uint32_t)(v >> 32));
}

/* =============================================================================
 * BIG-ENDIAN READ/WRITE
 * ============================================================================= */

static inline uint16_t uft_read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t uft_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | 
           ((uint32_t)p[1] << 16) | 
           ((uint32_t)p[2] << 8) | 
           (uint32_t)p[3];
}

static inline uint64_t uft_read_be64(const uint8_t *p) {
    return ((uint64_t)uft_read_be32(p) << 32) | 
           (uint64_t)uft_read_be32(p + 4);
}

static inline void uft_write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline void uft_write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

static inline void uft_write_be64(uint8_t *p, uint64_t v) {
    uft_write_be32(p, (uint32_t)(v >> 32));
    uft_write_be32(p + 4, (uint32_t)v);
}

/* =============================================================================
 * SIGNED VARIANTS
 * ============================================================================= */

static inline int16_t uft_read_le16s(const uint8_t *p) {
    return (int16_t)uft_read_le16(p);
}

static inline int32_t uft_read_le32s(const uint8_t *p) {
    return (int32_t)uft_read_le32(p);
}

static inline int16_t uft_read_be16s(const uint8_t *p) {
    return (int16_t)uft_read_be16(p);
}

static inline int32_t uft_read_be32s(const uint8_t *p) {
    return (int32_t)uft_read_be32(p);
}

/* =============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================= */

/**
 * @brief Clamp uint32 value to range
 */
static inline uint32_t uft_clamp_u32(uint32_t v, uint32_t min, uint32_t max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

/**
 * @brief Clamp int32 value to range
 */
static inline int32_t uft_clamp_i32(int32_t v, int32_t min, int32_t max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

/**
 * @brief Count set bits (population count)
 */
static inline int uft_popcount8(uint8_t v) {
    v = v - ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    return (v + (v >> 4)) & 0x0F;
}

static inline int uft_popcount32(uint32_t v) {
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    return (((v + (v >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * @brief Reverse bits in byte
 */
static inline uint8_t uft_reverse_bits8(uint8_t b) {
    b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

/**
 * @brief Apple II 4+4 encoded decode
 */
static inline uint8_t uft_decode_44(uint8_t odd, uint8_t even) {
    return (uint8_t)(((odd & 0x55) << 1) | (even & 0x55));
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_BITS_H */
