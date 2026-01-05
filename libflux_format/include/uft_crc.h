// uft_crc.h - CRC helpers (C11, no deps)
#ifndef UFT_CRC_H
#define UFT_CRC_H

#include <stdint.h>
#include <stddef.h>

/* CRC32 (IEEE 802.3) */
static inline uint32_t uft_crc32_ieee(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

#endif // UFT_CRC_H
