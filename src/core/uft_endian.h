#ifndef UFT_ENDIAN_H
#define UFT_ENDIAN_H
/**
 * @file uft_endian.h
 * @brief Portable Endianness Conversion for UFT
 * 
 * Safe byte-order conversion without alignment issues.
 * Works on any architecture (x86, ARM, big/little endian).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Little-Endian Read (Intel, most disk formats)
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint16_t uft_read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t uft_read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t uft_read_le64(const uint8_t *p) {
    return (uint64_t)uft_read_le32(p) | 
           ((uint64_t)uft_read_le32(p + 4) << 32);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Little-Endian Write
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * Big-Endian Read (Motorola, Amiga, network)
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint16_t uft_read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t uft_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t uft_read_be64(const uint8_t *p) {
    return ((uint64_t)uft_read_be32(p) << 32) | 
           (uint64_t)uft_read_be32(p + 4);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Big-Endian Write
 * ═══════════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * Byte Swap
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint16_t uft_bswap16(uint16_t v) {
    return ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
}

static inline uint32_t uft_bswap32(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Host to/from Little/Big Endian
 * ═══════════════════════════════════════════════════════════════════════════ */

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  #define UFT_HOST_BIG_ENDIAN 1
  #define uft_htole16(x) uft_bswap16(x)
  #define uft_htole32(x) uft_bswap32(x)
  #define uft_letoh16(x) uft_bswap16(x)
  #define uft_letoh32(x) uft_bswap32(x)
  #define uft_htobe16(x) (x)
  #define uft_htobe32(x) (x)
  #define uft_betoh16(x) (x)
  #define uft_betoh32(x) (x)
#else
  #define UFT_HOST_LITTLE_ENDIAN 1
  #define uft_htole16(x) (x)
  #define uft_htole32(x) (x)
  #define uft_letoh16(x) (x)
  #define uft_letoh32(x) (x)
  #define uft_htobe16(x) uft_bswap16(x)
  #define uft_htobe32(x) uft_bswap32(x)
  #define uft_betoh16(x) uft_bswap16(x)
  #define uft_betoh32(x) uft_bswap32(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_ENDIAN_H */
