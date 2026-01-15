/**
 * @file whd_crc16.c
 * @brief WHDLoad CRC-16 implementation
 * @version 3.8.0
 */
#include "uft/whdload/whd_crc16.h"

uint16_t uft_crc16_ansi(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t*)data;
    uint16_t crc = 0x0000u;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)p[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1u) crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            else          crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}
