/*
 * uft_crc.h
 *
 * CRC and checksum calculation utilities for UFT.
 *
 * Implemented algorithms:
 * - CRC-16-CCITT (x^16 + x^12 + x^5 + 1)
 * - CRC-32 (IEEE 802.3, Ethernet, ZIP, etc.)
 * - Amiga bootblock checksum
 * - Simple XOR checksum (1541 style)
 *
 * Sources:
 * - EasySplit by Thomas Giesel (zlib license)
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_crc.c
 */

#ifndef UFT_CRC_H
#define UFT_CRC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-16-CCITT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * CRC-16-CCITT polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 *
 * Common initial values:
 * - 0xFFFF for CRC-CCITT-FALSE (standard)
 * - 0x0000 for CRC-CCITT-ZERO
 * - 0x1D0F for CRC-CCITT-1D0F
 */

/**
 * Update CRC-16-CCITT with one byte.
 *
 * @param crc      Current CRC value
 * @param data     Byte to process
 * @return         Updated CRC
 */
uint16_t uft_crc16_update(uint16_t crc, uint8_t data);

/**
 * Calculate CRC-16-CCITT for buffer.
 *
 * @param data     Data buffer
 * @param length   Buffer length
 * @param init     Initial CRC value (typically 0xFFFF)
 * @return         Calculated CRC
 */
uint16_t uft_crc16_calc(const uint8_t *data, size_t length, uint16_t init);

/**
 * Calculate CRC-16 with standard initial value (0xFFFF).
 */
static inline uint16_t uft_crc16(const uint8_t *data, size_t length)
{
    return uft_crc16_calc(data, length, 0xFFFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-32
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * CRC-32 polynomial: 0x04C11DB7 (IEEE 802.3)
 */

/**
 * Initialize CRC-32 lookup table.
 * Call once before using fast CRC-32.
 */
void uft_crc32_init(void);

/**
 * Calculate CRC-32 (slow, no table).
 *
 * @param data     Data buffer
 * @param length   Buffer length
 * @return         CRC-32 value
 */
uint32_t uft_crc32_slow(const uint8_t *data, size_t length);

/**
 * Calculate CRC-32 (fast, table-based).
 * Requires uft_crc32_init() called first.
 *
 * @param data     Data buffer
 * @param length   Buffer length
 * @return         CRC-32 value
 */
uint32_t uft_crc32_fast(const uint8_t *data, size_t length);

/**
 * Update CRC-32 with one byte.
 *
 * @param crc      Current CRC value (init with 0xFFFFFFFF)
 * @param data     Byte to process
 * @return         Updated CRC
 */
uint32_t uft_crc32_update(uint32_t crc, uint8_t data);

/**
 * Finalize CRC-32 calculation.
 *
 * @param crc      CRC after all updates
 * @return         Final CRC value (XOR with 0xFFFFFFFF)
 */
static inline uint32_t uft_crc32_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * XOR CHECKSUMS (CBM/1541 style)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate XOR checksum of buffer.
 *
 * @param data     Data buffer
 * @param length   Buffer length
 * @return         XOR checksum (single byte)
 */
uint8_t uft_xor_checksum(const uint8_t *data, size_t length);

/**
 * Calculate 1541 sector header checksum.
 * Checksum = track ^ sector ^ id0 ^ id1
 *
 * @param track    Track number
 * @param sector   Sector number
 * @param id       Disk ID (2 bytes)
 * @return         Header checksum
 */
uint8_t uft_c64_header_checksum(uint8_t track, uint8_t sector, 
                                const uint8_t *id);

/**
 * Calculate 1541 sector data checksum.
 *
 * @param data     Sector data (256 bytes)
 * @return         Data checksum
 */
uint8_t uft_c64_data_checksum(const uint8_t *data);

/* ═══════════════════════════════════════════════════════════════════════════
 * AMIGA CHECKSUMS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate Amiga MFM sector checksum.
 * XOR of all 32-bit words.
 *
 * @param data     Data buffer
 * @param length   Buffer length (must be multiple of 4)
 * @return         32-bit XOR checksum
 */
uint32_t uft_amiga_checksum(const uint8_t *data, size_t length);

/**
 * Calculate Amiga bootblock checksum.
 * Special carry-add checksum for bootblock.
 *
 * @param bootblock  Bootblock data (1024 bytes)
 * @return           Bootblock checksum
 */
uint32_t uft_amiga_bootblock_checksum(const uint8_t *bootblock);

/**
 * Verify Amiga bootblock checksum.
 *
 * @param bootblock  Bootblock data (1024 bytes)
 * @return           true if checksum is valid
 */
bool uft_amiga_bootblock_verify(const uint8_t *bootblock);

/**
 * Fix Amiga bootblock checksum.
 * Updates checksum at offset 4.
 *
 * @param bootblock  Bootblock data (1024 bytes, modified in place)
 */
void uft_amiga_bootblock_fix(uint8_t *bootblock);

/* ═══════════════════════════════════════════════════════════════════════════
 * OTHER CHECKSUMS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate Fletcher-16 checksum.
 *
 * @param data     Data buffer
 * @param length   Buffer length
 * @return         Fletcher-16 checksum
 */
uint16_t uft_fletcher16(const uint8_t *data, size_t length);

/**
 * Calculate Adler-32 checksum.
 *
 * @param data     Data buffer
 * @param length   Buffer length
 * @return         Adler-32 checksum
 */
uint32_t uft_adler32(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_H */
