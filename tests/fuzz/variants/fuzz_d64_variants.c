/**
 * @file fuzz_d64_variants.c
 * @brief Fuzz target for D64 variant detection
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    int tracks;
    bool has_errors;
    bool is_geos;
    bool is_speeddos;
    int confidence;
} d64_info_t;

bool detect_d64(const uint8_t* data, size_t size, d64_info_t* info) {
    memset(info, 0, sizeof(*info));
    
    // Size-based detection
    switch (size) {
        case 174848:
            info->tracks = 35;
            info->confidence = 95;
            break;
        case 175531:
            info->tracks = 35;
            info->has_errors = true;
            info->confidence = 98;
            break;
        case 196608:
            info->tracks = 40;
            info->confidence = 95;
            break;
        case 197376:
            info->tracks = 40;
            info->has_errors = true;
            info->confidence = 98;
            break;
        case 205312:
            info->tracks = 42;
            info->confidence = 90;
            break;
        case 206114:
            info->tracks = 42;
            info->has_errors = true;
            info->confidence = 93;
            break;
        default:
            return false;
    }
    
    // GEOS detection: Check directory at Track 18
    size_t dir_offset = 0x16500;  // Track 18, Sector 0
    if (size > dir_offset + 512) {
        for (int entry = 0; entry < 8; entry++) {
            size_t offset = dir_offset + 256 + (entry * 32);
            if (offset + 32 > size) break;
            
            uint8_t file_type = data[offset + 2];
            if ((file_type & 0x80) && file_type != 0x80) {
                info->is_geos = true;
                info->confidence = 97;
                break;
            }
        }
    }
    
    // SpeedDOS detection
    if (size > dir_offset + 2) {
        uint8_t bam_sector = data[dir_offset + 1];
        if (bam_sector != 0 && bam_sector != 1) {
            info->is_speeddos = true;
        }
    }
    
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Only test D64-compatible sizes
    if (size < 174848 || size > 210000) return 0;
    
    d64_info_t info;
    bool detected = detect_d64(data, size, &info);
    
    if (detected) {
        // Track count must be valid
        if (info.tracks < 35 || info.tracks > 42) {
            __builtin_trap();
        }
        
        // Confidence must be reasonable
        if (info.confidence < 50 || info.confidence > 100) {
            __builtin_trap();
        }
        
        // Error flag consistency
        if (info.has_errors) {
            // Must be one of the error sizes
            if (size != 175531 && size != 197376 && size != 206114) {
                __builtin_trap();
            }
        }
    }
    
    return 0;
}
