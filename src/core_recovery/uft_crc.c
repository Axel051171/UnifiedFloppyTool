/**
 * @file uft_crc.c
 * @brief CRC calculation for recovery
 * @version 3.8.0
 */
#include "uft/uft_crc.h"

static uint16_t crc16_ccitt_false_update(uint16_t crc, const uint8_t *p, size_t len)
{
    while (len--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t uft_crc16_ccitt_false(const void *data, size_t len)
{
    if (!data || len == 0) return 0xFFFF;
    return crc16_ccitt_false_update(0xFFFF, (const uint8_t *)data, len);
}

static uint16_t crc16_reflect8(uint16_t v)
{
    v = (uint16_t)(((v & 0xF0) >> 4) | ((v & 0x0F) << 4));
    v = (uint16_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = (uint16_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

uint16_t uft_crc16_ibm_arc(const void *data, size_t len)
{
    if (!data || len == 0) return 0x0000;
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0x0000;
    while (len--) {
        uint8_t b = (uint8_t)crc16_reflect8(*p++);
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}
