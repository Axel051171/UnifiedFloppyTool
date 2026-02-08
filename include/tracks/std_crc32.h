#ifndef STD_CRC32_H
#define STD_CRC32_H
#include <stdint.h>
#include <stddef.h>

uint32_t std_crc32(const uint8_t *data, size_t len);
uint32_t std_crc32_init(void);
uint32_t std_crc32_update(uint32_t crc, uint8_t byte);
uint32_t std_crc32_final(uint32_t crc);

#endif
