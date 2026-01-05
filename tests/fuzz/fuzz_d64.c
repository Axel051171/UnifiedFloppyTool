/**
 * @file fuzz_d64.c
 * @brief Fuzz target for D64 parser
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// D64 constants
#define D64_SIZE_35 174848
#define D64_SIZE_35_ERR 175531
#define D64_SIZE_40 196608
#define D64_SIZE_40_ERR 197376
#define D64_SIZE_42 205312
#define D64_SIZE_42_ERR 206114

#define D64_SECTOR_SIZE 256
#define D64_TRACKS_MAX 42

// Sectors per track
static const uint8_t d64_sectors_per_track[D64_TRACKS_MAX] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

// Track offsets (cumulative sectors)
static const uint16_t d64_track_offset[D64_TRACKS_MAX + 1] = {
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189, 210, 231, 252, 273, 294, 315, 336,
    357, 376, 395, 414, 433, 452, 471, 490, 508, 526, 544, 562, 580, 598,
    615, 632, 649, 666, 683, 700, 717, 734, 751, 768, 785, 802
};

// Determine D64 variant from size
int d64_detect_variant(size_t size, int* tracks, int* has_errors) {
    *has_errors = 0;
    
    switch (size) {
        case D64_SIZE_35:     *tracks = 35; return 0;
        case D64_SIZE_35_ERR: *tracks = 35; *has_errors = 1; return 0;
        case D64_SIZE_40:     *tracks = 40; return 0;
        case D64_SIZE_40_ERR: *tracks = 40; *has_errors = 1; return 0;
        case D64_SIZE_42:     *tracks = 42; return 0;
        case D64_SIZE_42_ERR: *tracks = 42; *has_errors = 1; return 0;
        default:
            // Unknown size - could be truncated
            if (size >= D64_SIZE_35) {
                *tracks = 35;
                return 0;
            }
            return -1;  // Too small
    }
}

// Safe sector read
int d64_read_sector_safe(const uint8_t* data, size_t size,
                          int track, int sector, uint8_t* out) {
    // Track validation (1-based)
    if (track < 1 || track > D64_TRACKS_MAX) {
        return -1;
    }
    
    // Sector validation
    int max_sector = d64_sectors_per_track[track - 1];
    if (sector < 0 || sector >= max_sector) {
        return -2;
    }
    
    // Calculate offset with overflow check
    size_t sector_index = d64_track_offset[track - 1] + (size_t)sector;
    
    // Check multiplication overflow
    if (sector_index > SIZE_MAX / D64_SECTOR_SIZE) {
        return -3;
    }
    size_t offset = sector_index * D64_SECTOR_SIZE;
    
    // Bounds check
    if (offset + D64_SECTOR_SIZE > size) {
        return -4;
    }
    
    // Safe to copy
    memcpy(out, data + offset, D64_SECTOR_SIZE);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    int tracks, has_errors;
    
    // Detect variant
    if (d64_detect_variant(size, &tracks, &has_errors) != 0) {
        return 0;  // Too small, skip
    }
    
    // Try reading all sectors
    uint8_t sector_buf[D64_SECTOR_SIZE];
    for (int t = 1; t <= tracks; t++) {
        int sectors = d64_sectors_per_track[t - 1];
        for (int s = 0; s < sectors; s++) {
            d64_read_sector_safe(data, size, t, s, sector_buf);
        }
    }
    
    return 0;
}
