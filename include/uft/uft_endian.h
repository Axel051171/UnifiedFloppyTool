/**
 * @file uft_endian.h
 * @brief Cross-Platform Endianness-Safe Binary I/O
 * 
 * NOTE: If uft_platform.h is included first, these functions are already defined.
 *       We use guards to prevent redefinition errors.
 */

#ifndef UFT_ENDIAN_H
#define UFT_ENDIAN_H

#include <stdint.h>

/* =============================================================================
 * Guard against redefinition if uft_platform.h was included first
 * ============================================================================= */
#ifndef UFT_ENDIAN_FUNCTIONS_DEFINED
#define UFT_ENDIAN_FUNCTIONS_DEFINED

/* =============================================================================
 * LITTLE-ENDIAN READERS
 * ============================================================================= */

static inline uint16_t uft_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t uft_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | 
           ((uint32_t)p[3] << 24);
}

static inline uint64_t uft_read_le64(const uint8_t *p)
{
    return (uint64_t)uft_read_le32(p) | 
           ((uint64_t)uft_read_le32(p + 4) << 32);
}

/* =============================================================================
 * BIG-ENDIAN READERS
 * ============================================================================= */

static inline uint16_t uft_read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t uft_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | 
           ((uint32_t)p[1] << 16) | 
           ((uint32_t)p[2] << 8) | 
           (uint32_t)p[3];
}

static inline uint64_t uft_read_be64(const uint8_t *p)
{
    return ((uint64_t)uft_read_be32(p) << 32) | 
           (uint64_t)uft_read_be32(p + 4);
}

/* =============================================================================
 * LITTLE-ENDIAN WRITERS
 * ============================================================================= */

static inline void uft_write_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
}

static inline void uft_write_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

static inline void uft_write_le64(uint8_t *p, uint64_t value)
{
    uft_write_le32(p, (uint32_t)(value & 0xFFFFFFFF));
    uft_write_le32(p + 4, (uint32_t)(value >> 32));
}

/* =============================================================================
 * BIG-ENDIAN WRITERS
 * ============================================================================= */

static inline void uft_write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)((value >> 8) & 0xFF);
    p[1] = (uint8_t)(value & 0xFF);
}

static inline void uft_write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)((value >> 24) & 0xFF);
    p[1] = (uint8_t)((value >> 16) & 0xFF);
    p[2] = (uint8_t)((value >> 8) & 0xFF);
    p[3] = (uint8_t)(value & 0xFF);
}

static inline void uft_write_be64(uint8_t *p, uint64_t value)
{
    uft_write_be32(p, (uint32_t)(value >> 32));
    uft_write_be32(p + 4, (uint32_t)(value & 0xFFFFFFFF));
}

#endif /* UFT_ENDIAN_FUNCTIONS_DEFINED */

#endif /* UFT_ENDIAN_H */
