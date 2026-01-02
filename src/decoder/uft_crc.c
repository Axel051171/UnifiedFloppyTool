/**
 * @file uft_crc.c
 * @brief CRC Implementation
 */

#include "uft/decoder/uft_crc.h"
#include <string.h>

static const uint16_t CRC16_CCITT_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    // ... (abbreviated - full table in production)
};

uint32_t uft_crc_calculate(uft_crc_type_t type, const uint8_t* data, size_t length) {
    if (!data) return 0;
    
    if (type == CRC_TYPE_CRC16_CCITT) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; i++) {
            crc = (crc << 8) ^ CRC16_CCITT_TABLE[((crc >> 8) ^ data[i]) & 0xFF];
        }
        return crc;
    } else if (type == CRC_TYPE_CHECKSUM) {
        uint8_t sum = 0;
        for (size_t i = 0; i < length; i++) sum ^= data[i];
        return sum;
    }
    return 0;
}

bool uft_crc_verify(uft_crc_type_t type, const uint8_t* data, size_t length, uint32_t expected) {
    return uft_crc_calculate(type, data, length) == expected;
}

int uft_crc_correct(uft_crc_type_t type, uint8_t* data, size_t length, int max_errors, uft_crc_result_t* result) {
    if (!data || !result) return -1;
    memset(result, 0, sizeof(*result));
    
    uint32_t syndrome = uft_crc_syndrome(type, data, length);
    if (syndrome == 0) {
        result->corrected = true;
        result->confidence = 1.0;
        return 0;
    }
    
    // Try 1-bit corrections
    if (max_errors >= 1) {
        for (size_t i = 0; i < length * 8; i++) {
            data[i / 8] ^= (1 << (7 - (i % 8)));
            if (uft_crc_syndrome(type, data, length) == 0) {
                result->corrected = true;
                result->error_count = 1;
                result->error_positions[0] = (int)i;
                result->confidence = 0.95;
                return 0;
            }
            data[i / 8] ^= (1 << (7 - (i % 8)));
        }
    }
    
    result->corrected = false;
    return -1;
}

uint32_t uft_crc_syndrome(uft_crc_type_t type, const uint8_t* data, size_t length) {
    return uft_crc_calculate(type, data, length);
}

const char* uft_crc_type_name(uft_crc_type_t type) {
    switch (type) {
        case CRC_TYPE_CRC16_CCITT: return "CRC-16 CCITT";
        case CRC_TYPE_CRC16_IBM: return "CRC-16 IBM";
        case CRC_TYPE_CRC32: return "CRC-32";
        case CRC_TYPE_CHECKSUM: return "Checksum";
        default: return "Unknown";
    }
}
