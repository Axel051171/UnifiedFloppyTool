/**
 * @file fuzz_adf_variants.c
 * @brief Fuzz target for ADF variant detection
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    int fs_type;      // 0=OFS, 1=FFS, etc.
    bool is_hd;
    bool is_pc_fat;
    bool is_bootable;
    int confidence;
} adf_info_t;

bool detect_adf(const uint8_t* data, size_t size, adf_info_t* info) {
    memset(info, 0, sizeof(*info));
    
    // Size check
    if (size == 901120) {
        info->is_hd = false;
    } else if (size == 1802240) {
        info->is_hd = true;
    } else {
        return false;
    }
    
    info->confidence = 60;  // Base confidence
    
    // PC-FAT detection
    if (size >= 512) {
        if (data[510] == 0x55 && data[511] == 0xAA) {
            if (data[0] == 0xEB || data[0] == 0xE9) {
                info->is_pc_fat = true;
                info->is_bootable = true;
                info->confidence = 95;
                return true;
            }
        }
    }
    
    // Amiga DOS detection
    if (size >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
        info->fs_type = data[3];
        info->is_bootable = true;
        info->confidence = 98;
        
        // Validate fs_type
        if (info->fs_type > 7) {
            info->fs_type = -1;  // Unknown
            info->confidence = 70;
        }
    }
    
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Only test ADF-compatible sizes
    if (size != 901120 && size != 1802240) return 0;
    
    adf_info_t info;
    bool detected = detect_adf(data, size, &info);
    
    if (detected) {
        // Confidence must be valid
        if (info.confidence < 0 || info.confidence > 100) {
            __builtin_trap();
        }
        
        // fs_type must be valid range
        if (info.fs_type < -1 || info.fs_type > 7) {
            __builtin_trap();
        }
        
        // HD flag consistency
        if (info.is_hd && size != 1802240) {
            __builtin_trap();
        }
    }
    
    return 0;
}
