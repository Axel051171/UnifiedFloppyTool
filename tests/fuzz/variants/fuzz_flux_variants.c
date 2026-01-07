/**
 * @file fuzz_flux_variants.c
 * @brief Fuzz target for flux format variant detection (SCP, HFE, IPF, WOZ)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    char format[8];
    int version;
    int tracks;
    int confidence;
    bool has_metadata;
} flux_info_t;

bool detect_flux(const uint8_t* data, size_t size, flux_info_t* info) {
    memset(info, 0, sizeof(*info));
    
    if (size < 12) return false;
    
    // SCP
    if (data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
        strncpy(info->format, "SCP", sizeof(info->format) - 1);
        info->version = data[3];
        info->confidence = 100;
        
        if (size >= 8) {
            uint8_t start = data[6];
            uint8_t end = data[7];
            if (end >= start) {
                info->tracks = end - start + 1;
            }
        }
        
        if (size >= 9 && (data[8] & 0x01)) {
            info->has_metadata = true;
        }
        
        return true;
    }
    
    // HFE v1/v2
    if (memcmp(data, "HXCPICFE", 8) == 0) {
        strncpy(info->format, "HFE", sizeof(info->format) - 1);
        info->version = data[8];
        info->confidence = 100;
        
        if (size >= 10) {
            info->tracks = data[9];
        }
        
        return true;
    }
    
    // HFE v3
    if (memcmp(data, "HXCHFE3", 7) == 0) {
        strncpy(info->format, "HFE", sizeof(info->format) - 1);
        info->version = 3;
        info->confidence = 100;
        info->has_metadata = true;
        return true;
    }
    
    // WOZ
    if (size >= 8) {
        uint32_t magic = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        uint32_t tail = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
        
        if (tail == 0x0A0D0AFF) {
            if (magic == 0x315A4F57) {
                strncpy(info->format, "WOZ", sizeof(info->format) - 1);
                info->version = 1;
                info->confidence = 100;
                return true;
            }
            if (magic == 0x325A4F57) {
                strncpy(info->format, "WOZ", sizeof(info->format) - 1);
                info->version = 2;
                info->confidence = 100;
                return true;
            }
        }
    }
    
    // IPF
    if (memcmp(data, "CAPS", 4) == 0) {
        strncpy(info->format, "IPF", sizeof(info->format) - 1);
        info->version = 2;
        info->confidence = 100;
        return true;
    }
    
    return false;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8 || size > 100 * 1024 * 1024) return 0;
    
    flux_info_t info;
    bool detected = detect_flux(data, size, &info);
    
    if (detected) {
        // Format should be set
        if (info.format[0] == '\0') {
            __builtin_trap();
        }
        
        // Version should be reasonable
        if (info.version < 0 || info.version > 255) {
            __builtin_trap();
        }
        
        // Tracks should be reasonable
        if (info.tracks < 0 || info.tracks > 200) {
            __builtin_trap();
        }
        
        // Confidence should be valid
        if (info.confidence < 0 || info.confidence > 100) {
            __builtin_trap();
        }
    }
    
    return 0;
}
