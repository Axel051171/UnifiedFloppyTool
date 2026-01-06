/**
 * @file floppy_utils.h
 * @brief Floppy utility functions for UFT compatibility
 */

#ifndef FLOPPY_UTILS_H
#define FLOPPY_UTILS_H

#include "libhxcfe.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Bit/Byte Manipulation
 * ============================================================================ */

static inline uint8_t getbit(const uint8_t* buffer, uint32_t bit_offset) {
    return (buffer[bit_offset >> 3] >> (7 - (bit_offset & 7))) & 1;
}

static inline void setbit(uint8_t* buffer, uint32_t bit_offset, uint8_t value) {
    uint32_t byte_idx = bit_offset >> 3;
    uint8_t bit_mask = 0x80 >> (bit_offset & 7);
    if (value)
        buffer[byte_idx] |= bit_mask;
    else
        buffer[byte_idx] &= ~bit_mask;
}

static inline uint8_t getbyte(const uint8_t* buffer, uint32_t bit_offset) {
    uint32_t byte_idx = bit_offset >> 3;
    uint8_t bit_shift = bit_offset & 7;
    if (bit_shift == 0)
        return buffer[byte_idx];
    return (buffer[byte_idx] << bit_shift) | (buffer[byte_idx + 1] >> (8 - bit_shift));
}

/* ============================================================================
 * Track Utilities
 * ============================================================================ */

static inline uint32_t us2index(uint32_t us, uint32_t bitrate) {
    return (uint32_t)((uint64_t)us * bitrate / 1000000ULL);
}

static inline uint32_t index2us(uint32_t index, uint32_t bitrate) {
    return (uint32_t)((uint64_t)index * 1000000ULL / bitrate);
}

static inline uint32_t tracklen_us(int32_t rpm) {
    if (rpm <= 0) rpm = 300;
    return (60 * 1000000) / rpm;
}

static inline uint32_t tracklen_bits(int32_t bitrate, int32_t rpm) {
    return us2index(tracklen_us(rpm), bitrate);
}

/* ============================================================================
 * CRC Helpers
 * ============================================================================ */

static inline uint16_t crc16_ccitt_byte(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }
    return crc;
}

static inline uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = crc16_ccitt_byte(crc, data[i]);
    }
    return crc;
}

/* ============================================================================
 * MFM/FM Encoding Helpers
 * ============================================================================ */

/* MFM encoding lookup table */
static inline uint16_t mfm_encode_byte(uint8_t data, uint8_t last_bit) {
    uint16_t mfm = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        uint8_t clock = (last_bit == 0 && bit == 0) ? 1 : 0;
        mfm = (mfm << 2) | (clock << 1) | bit;
        last_bit = bit;
    }
    return mfm;
}

/* FM encoding */
static inline uint16_t fm_encode_byte(uint8_t data) {
    uint16_t fm = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        fm = (fm << 2) | 0x02 | bit;  /* Clock always 1 */
    }
    return fm;
}

#ifdef __cplusplus
}
#endif

#endif /* FLOPPY_UTILS_H */
