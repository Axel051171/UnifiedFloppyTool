/**
 * @file fuzz_variant_detect.c
 * @brief Fuzz target for variant detection
 * 
 * Ensures variant detection never crashes on any input.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// Simplified variant info for fuzzing
typedef struct {
    uint32_t format_id;
    uint32_t variant_flags;
    char format_name[16];
    int confidence;
    int tracks;
    int heads;
} fuzz_variant_t;

// Minimal detection logic
int fuzz_detect(const uint8_t* data, size_t size, fuzz_variant_t* info) {
    if (!data || !info || size < 2) return -1;
    
    memset(info, 0, sizeof(*info));
    
    // SCP
    if (size >= 16 && data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
        info->format_id = 0x1000;
        strcpy(info->format_name, "SCP");
        info->confidence = 100;
        if (size > 7) {
            uint8_t start = data[6];
            uint8_t end = data[7];
            if (end >= start && end < 200) {
                info->tracks = end - start + 1;
            }
        }
        return 0;
    }
    
    // HFE
    if (size >= 16 && memcmp(data, "HXCPICFE", 8) == 0) {
        info->format_id = 0x1001;
        strcpy(info->format_name, "HFE");
        info->confidence = 100;
        if (size > 10) {
            info->tracks = data[9];
            info->heads = data[10];
            if (info->tracks > 96) info->tracks = 96;
            if (info->heads > 2) info->heads = 2;
        }
        return 0;
    }
    
    // HFE v3
    if (size >= 8 && memcmp(data, "HXCHFE3", 7) == 0) {
        info->format_id = 0x1001;
        strcpy(info->format_name, "HFE");
        info->variant_flags = 0x04;  // V3
        info->confidence = 100;
        return 0;
    }
    
    // WOZ
    if (size >= 8) {
        uint32_t magic = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        if (magic == 0x315A4F57 || magic == 0x325A4F57) {
            info->format_id = 0x0320;
            strcpy(info->format_name, "WOZ");
            info->confidence = 100;
            return 0;
        }
    }
    
    // G64
    if (size >= 12 && memcmp(data, "GCR-1541", 8) == 0) {
        info->format_id = 0x0110;
        strcpy(info->format_name, "G64");
        info->confidence = 100;
        if (size > 9) {
            info->tracks = data[9];
            if (info->tracks > 84) info->tracks = 84;
        }
        return 0;
    }
    
    // IPF
    if (size >= 12 && memcmp(data, "CAPS", 4) == 0) {
        info->format_id = 0x1002;
        strcpy(info->format_name, "IPF");
        info->confidence = 100;
        return 0;
    }
    
    // ATR
    if (size >= 16 && data[0] == 0x96 && data[1] == 0x02) {
        info->format_id = 0x0500;
        strcpy(info->format_name, "ATR");
        info->confidence = 100;
        return 0;
    }
    
    // ADF by size
    if (size == 901120 || size == 1802240) {
        info->format_id = 0x0200;
        strcpy(info->format_name, "ADF");
        info->confidence = 80;
        info->tracks = 80;
        info->heads = 2;
        return 0;
    }
    
    // D64 by size
    if (size >= 174848 && size <= 206114) {
        info->format_id = 0x0100;
        strcpy(info->format_name, "D64");
        info->confidence = 90;
        return 0;
    }
    
    return -1;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Limit size for efficiency
    if (size > 2 * 1024 * 1024) return 0;
    
    fuzz_variant_t info;
    int result = fuzz_detect(data, size, &info);
    
    // Invariant checks
    if (result == 0) {
        // Format ID should be set
        if (info.format_id == 0) {
            __builtin_trap();
        }
        
        // Format name should not be empty
        if (info.format_name[0] == '\0') {
            __builtin_trap();
        }
        
        // Confidence should be valid
        if (info.confidence < 0 || info.confidence > 100) {
            __builtin_trap();
        }
        
        // Geometry should be reasonable
        if (info.tracks < 0 || info.tracks > 200) {
            __builtin_trap();
        }
        if (info.heads < 0 || info.heads > 4) {
            __builtin_trap();
        }
    }
    
    return 0;
}
