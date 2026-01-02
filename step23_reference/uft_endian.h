/**
 * @file uft_endian.h
 * @brief Cross-Platform Endianness-Safe Binary I/O
 * 
 * CRITICAL FIX: Endianness Handling
 * 
 * PROBLEM:
 * - struct casts break on different platforms
 * - Alignment issues cause crashes
 * - Big-endian vs Little-endian confusion
 * 
 * SOLUTION:
 * - Explicit LE/BE readers
 * - No struct casts on file bytes
 * - Works on all platforms (x86, ARM, PowerPC, etc.)
 * 
 * USAGE:
 *   uint8_t buf[4];
 *   fread(buf, 1, 4, fp);
 *   
 *   uint16_t value16 = uft_read_le16(buf);     // Little-endian
 *   uint32_t value32 = uft_read_be32(buf);     // Big-endian
 *   
 * FILE FORMATS:
 * - SCP: Little-endian
 * - HFE: Little-endian
 * - KryoFlux RAW: Little-endian
 * - Some old formats: Big-endian
 */

#ifndef UFT_ENDIAN_H
#define UFT_ENDIAN_H

#include <stdint.h>

/* =============================================================================
 * LITTLE-ENDIAN READERS
 * Used by: SCP, HFE, KryoFlux, most modern formats
 * ============================================================================= */

/**
 * @brief Read 16-bit little-endian value
 * @param p Pointer to 2 bytes
 * @return 16-bit value in host byte order
 */
static inline uint16_t uft_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] 
         | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian value
 * @param p Pointer to 4 bytes
 * @return 32-bit value in host byte order
 */
static inline uint32_t uft_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/**
 * @brief Read 64-bit little-endian value
 * @param p Pointer to 8 bytes
 * @return 64-bit value in host byte order
 */
static inline uint64_t uft_read_le64(const uint8_t *p)
{
    return (uint64_t)p[0]
         | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16)
         | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32)
         | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48)
         | ((uint64_t)p[7] << 56);
}

/* =============================================================================
 * BIG-ENDIAN READERS
 * Used by: Some old Apple formats, Motorola-based systems
 * ============================================================================= */

/**
 * @brief Read 16-bit big-endian value
 * @param p Pointer to 2 bytes
 * @return 16-bit value in host byte order
 */
static inline uint16_t uft_read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8)
         | (uint16_t)p[1];
}

/**
 * @brief Read 32-bit big-endian value
 * @param p Pointer to 4 bytes
 * @return 32-bit value in host byte order
 */
static inline uint32_t uft_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)
         | (uint32_t)p[3];
}

/**
 * @brief Read 64-bit big-endian value
 * @param p Pointer to 8 bytes
 * @return 64-bit value in host byte order
 */
static inline uint64_t uft_read_be64(const uint8_t *p)
{
    return ((uint64_t)p[0] << 56)
         | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40)
         | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24)
         | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] << 8)
         | (uint64_t)p[7];
}

/* =============================================================================
 * LITTLE-ENDIAN WRITERS
 * ============================================================================= */

/**
 * @brief Write 16-bit value as little-endian
 * @param p Pointer to 2-byte buffer
 * @param value Value to write
 */
static inline void uft_write_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
}

/**
 * @brief Write 32-bit value as little-endian
 * @param p Pointer to 4-byte buffer
 * @param value Value to write
 */
static inline void uft_write_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * @brief Write 64-bit value as little-endian
 * @param p Pointer to 8-byte buffer
 * @param value Value to write
 */
static inline void uft_write_le64(uint8_t *p, uint64_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
    p[4] = (uint8_t)((value >> 32) & 0xFF);
    p[5] = (uint8_t)((value >> 40) & 0xFF);
    p[6] = (uint8_t)((value >> 48) & 0xFF);
    p[7] = (uint8_t)((value >> 56) & 0xFF);
}

/* =============================================================================
 * BIG-ENDIAN WRITERS
 * ============================================================================= */

/**
 * @brief Write 16-bit value as big-endian
 * @param p Pointer to 2-byte buffer
 * @param value Value to write
 */
static inline void uft_write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)((value >> 8) & 0xFF);
    p[1] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Write 32-bit value as big-endian
 * @param p Pointer to 4-byte buffer
 * @param value Value to write
 */
static inline void uft_write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)((value >> 24) & 0xFF);
    p[1] = (uint8_t)((value >> 16) & 0xFF);
    p[2] = (uint8_t)((value >> 8) & 0xFF);
    p[3] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Write 64-bit value as big-endian
 * @param p Pointer to 8-byte buffer
 * @param value Value to write
 */
static inline void uft_write_be64(uint8_t *p, uint64_t value)
{
    p[0] = (uint8_t)((value >> 56) & 0xFF);
    p[1] = (uint8_t)((value >> 48) & 0xFF);
    p[2] = (uint8_t)((value >> 40) & 0xFF);
    p[3] = (uint8_t)((value >> 32) & 0xFF);
    p[4] = (uint8_t)((value >> 24) & 0xFF);
    p[5] = (uint8_t)((value >> 16) & 0xFF);
    p[6] = (uint8_t)((value >> 8) & 0xFF);
    p[7] = (uint8_t)(value & 0xFF);
}

/* =============================================================================
 * SAFETY EXAMPLES
 * ============================================================================= */

/*
 * WRONG WAY (Platform-specific, breaks on ARM/big-endian):
 * 
 * struct SCPHeader {
 *     uint32_t magic;
 *     uint16_t version;
 * };
 * 
 * uint8_t file_data[1024];
 * fread(file_data, 1, 1024, fp);
 * 
 * SCPHeader *header = (SCPHeader*)file_data;  // ❌ WRONG!
 * // - Alignment issues on ARM
 * // - Endianness wrong on big-endian
 * // - Padding/packing issues
 * 
 * 
 * RIGHT WAY (Cross-platform, always works):
 * 
 * uint8_t file_data[1024];
 * fread(file_data, 1, 1024, fp);
 * 
 * uint32_t magic = uft_read_le32(file_data);      // ✅ CORRECT!
 * uint16_t version = uft_read_le16(file_data + 4); // ✅ CORRECT!
 * 
 * // Works on: x86, x64, ARM, PowerPC, MIPS, etc.
 * // Works on: Little-endian AND big-endian systems
 */

#endif /* UFT_ENDIAN_H */
