/*
 * uft_crc.c
 *
 * CRC and checksum calculation utilities implementation.
 *
 * Sources:
 * - CRC-16-CCITT: EasySplit by Thomas Giesel (zlib license)
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_crc.c
 */

#include "uft_crc.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-16-CCITT
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * CRC-CCITT: x^16 + x^12 + x^5 + 1
 * Based on EasySplit by Thomas Giesel (zlib license)
 */
uint16_t uft_crc16_update(uint16_t crc, uint8_t data)
{
    crc = ((crc >> 8) & 0xFF) | (crc << 8);
    crc ^= data;
    crc ^= (crc & 0xFF) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xFF) << 5;
    return crc;
}

uint16_t uft_crc16_calc(const uint8_t *data, size_t length, uint16_t init)
{
    uint16_t crc = init;
    
    for (size_t i = 0; i < length; i++) {
        crc = uft_crc16_update(crc, data[i]);
    }
    
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-32
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * CRC-32 polynomial: 0x04C11DB7 (reflected: 0xEDB88320)
 */
#define CRC32_POLYNOMIAL 0xEDB88320

static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

void uft_crc32_init(void)
{
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    
    crc32_table_initialized = 1;
}

uint32_t uft_crc32_slow(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t uft_crc32_fast(const uint8_t *data, size_t length)
{
    if (!crc32_table_initialized) {
        uft_crc32_init();
    }
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t uft_crc32_update(uint32_t crc, uint8_t data)
{
    if (!crc32_table_initialized) {
        uft_crc32_init();
    }
    
    return (crc >> 8) ^ crc32_table[(crc ^ data) & 0xFF];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * XOR CHECKSUMS
 * ═══════════════════════════════════════════════════════════════════════════ */

uint8_t uft_xor_checksum(const uint8_t *data, size_t length)
{
    uint8_t checksum = 0;
    
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    
    return checksum;
}

uint8_t uft_c64_header_checksum(uint8_t track, uint8_t sector, 
                                const uint8_t *id)
{
    return track ^ sector ^ id[0] ^ id[1];
}

uint8_t uft_c64_data_checksum(const uint8_t *data)
{
    return uft_xor_checksum(data, 256);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AMIGA CHECKSUMS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Read big-endian 32-bit value */
static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* Write big-endian 32-bit value */
static inline void write_be32(uint8_t *p, uint32_t val)
{
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)val;
}

uint32_t uft_amiga_checksum(const uint8_t *data, size_t length)
{
    uint32_t checksum = 0;
    
    /* Process 4 bytes at a time */
    size_t i;
    for (i = 0; i + 3 < length; i += 4) {
        checksum ^= read_be32(data + i);
    }
    
    /* Handle remaining bytes */
    if (i < length) {
        uint32_t last = 0;
        for (size_t j = 0; j < length - i; j++) {
            last |= (uint32_t)data[i + j] << (24 - j * 8);
        }
        checksum ^= last;
    }
    
    return checksum;
}

uint32_t uft_amiga_bootblock_checksum(const uint8_t *bootblock)
{
    uint32_t checksum = 0;
    uint32_t prev_checksum;
    
    for (size_t i = 0; i < 1024; i += 4) {
        prev_checksum = checksum;
        checksum += read_be32(bootblock + i);
        
        /* Handle carry (overflow) */
        if (checksum < prev_checksum) {
            checksum++;
        }
    }
    
    return ~checksum;
}

bool uft_amiga_bootblock_verify(const uint8_t *bootblock)
{
    /* Stored checksum is at offset 4 */
    uint32_t stored_checksum = read_be32(bootblock + 4);
    
    /* Calculate checksum with checksum field zeroed */
    uint8_t temp[1024];
    memcpy(temp, bootblock, 1024);
    write_be32(temp + 4, 0);
    
    uint32_t calc_checksum = uft_amiga_bootblock_checksum(temp);
    
    return (calc_checksum == stored_checksum);
}

void uft_amiga_bootblock_fix(uint8_t *bootblock)
{
    /* Zero the checksum field first */
    write_be32(bootblock + 4, 0);
    
    /* Calculate new checksum */
    uint32_t checksum = uft_amiga_bootblock_checksum(bootblock);
    
    /* Write checksum to offset 4 */
    write_be32(bootblock + 4, checksum);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OTHER CHECKSUMS
 * ═══════════════════════════════════════════════════════════════════════════ */

uint16_t uft_fletcher16(const uint8_t *data, size_t length)
{
    uint16_t sum1 = 0xFF, sum2 = 0xFF;
    
    while (length > 0) {
        size_t block_len = (length > 20) ? 20 : length;
        length -= block_len;
        
        do {
            sum1 += *data++;
            sum2 += sum1;
        } while (--block_len);
        
        sum1 = (sum1 & 0xFF) + (sum1 >> 8);
        sum2 = (sum2 & 0xFF) + (sum2 >> 8);
    }
    
    /* Second reduction to reduce sums to 8 bits */
    sum1 = (sum1 & 0xFF) + (sum1 >> 8);
    sum2 = (sum2 & 0xFF) + (sum2 >> 8);
    
    return (sum2 << 8) | sum1;
}

uint32_t uft_adler32(const uint8_t *data, size_t length)
{
    const uint32_t MOD_ADLER = 65521;
    uint32_t a = 1, b = 0;
    
    for (size_t i = 0; i < length; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    
    return (b << 16) | a;
}
