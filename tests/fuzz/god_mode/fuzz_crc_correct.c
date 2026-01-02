/**
 * @file fuzz_crc_correct.c
 * @brief Fuzz target for CRC correction
 * 
 * Ensures correction never produces false positives.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

typedef struct {
    bool success;
    int flips;
    size_t flip_pos[2];
    uint8_t corrected[64];
} correction_t;

correction_t try_correct(const uint8_t* data, size_t len, uint16_t expected) {
    correction_t r = {0};
    if (len > 64) len = 64;
    memcpy(r.corrected, data, len);
    
    // Check original
    if (crc16(data, len) == expected) {
        r.success = true;
        return r;
    }
    
    // Try 1-bit
    for (size_t b = 0; b < len * 8; b++) {
        r.corrected[b/8] ^= (1 << (7 - b%8));
        if (crc16(r.corrected, len) == expected) {
            r.success = true;
            r.flips = 1;
            r.flip_pos[0] = b;
            return r;
        }
        r.corrected[b/8] ^= (1 << (7 - b%8));
    }
    
    return r;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;
    
    // First 2 bytes = expected CRC
    uint16_t expected = (data[0] << 8) | data[1];
    const uint8_t* payload = data + 2;
    size_t len = size - 2;
    if (len > 62) len = 62;
    
    correction_t r = try_correct(payload, len, expected);
    
    // Verify correction is actually correct
    if (r.success && r.flips > 0) {
        uint16_t actual = crc16(r.corrected, len);
        if (actual != expected) {
            // False positive!
            __builtin_trap();
        }
    }
    
    return 0;
}
