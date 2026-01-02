/**
 * @file uft_format_params.c
 * @brief Format parameter presets implementation
 */

#include "uft/uft_format_params.h"
#include <string.h>

// ============================================================================
// Preset Definitions (from SAMdisk Format.cpp)
// ============================================================================

static const uft_format_params_t g_presets[] = {
    // UFT_PRESET_CUSTOM
    {0},
    
    // UFT_PRESET_PC_160K - 40×1×8×512
    {
        .cylinders = 40, .heads = 1, .sectors = 8, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_180K - 40×1×9×512
    {
        .cylinders = 40, .heads = 1, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_320K - 40×2×8×512
    {
        .cylinders = 40, .heads = 2, .sectors = 8, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_360K - 40×2×9×512
    {
        .cylinders = 40, .heads = 2, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_640K - 80×2×8×512
    {
        .cylinders = 80, .heads = 2, .sectors = 8, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xE5
    },
    
    // UFT_PRESET_PC_720K - 80×2×9×512
    {
        .cylinders = 80, .heads = 2, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_1200K - 80×2×15×512
    {
        .cylinders = 80, .heads = 2, .sectors = 15, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x54, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_1232K - 77×2×8×1024 (PC-98)
    {
        .cylinders = 77, .heads = 2, .sectors = 8, .size = 3,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x74, .fill = 0xE5
    },
    
    // UFT_PRESET_PC_1440K - 80×2×18×512
    {
        .cylinders = 80, .heads = 2, .sectors = 18, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x6C, .fill = 0xF6
    },
    
    // UFT_PRESET_PC_2880K - 80×2×36×512
    {
        .cylinders = 80, .heads = 2, .sectors = 36, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_1000K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x53, .fill = 0xF6
    },
    
    // UFT_PRESET_MGT - 80×2×10×512 (800K SAM Coupé)
    {
        .cylinders = 80, .heads = 2, .sectors = 10, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 24, .fill = 0x00
    },
    
    // UFT_PRESET_D2M - 80×2×10×512 (800K MGT D2M)
    {
        .cylinders = 80, .heads = 2, .sectors = 10, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 24, .fill = 0x00
    },
    
    // UFT_PRESET_D4M - 80×2×20×512 (1.6MB MGT D4M)
    {
        .cylinders = 80, .heads = 2, .sectors = 20, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 24, .fill = 0x00
    },
    
    // UFT_PRESET_CPC_DATA - 40×1×9×512
    {
        .cylinders = 40, .heads = 1, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 0xC1, .interleave = 1, .skew = 0, .gap3 = 0x4E, .fill = 0xE5
    },
    
    // UFT_PRESET_CPC_SYSTEM - 40×1×9×512 + boot
    {
        .cylinders = 40, .heads = 1, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 0x41, .interleave = 1, .skew = 0, .gap3 = 0x4E, .fill = 0xE5
    },
    
    // UFT_PRESET_TRDOS - 80×2×16×256
    {
        .cylinders = 80, .heads = 2, .sectors = 16, .size = 1,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 11, .fill = 0x00
    },
    
    // UFT_PRESET_OPUS - Opus Discovery
    {
        .cylinders = 40, .heads = 1, .sectors = 18, .size = 1,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 12, .fill = 0x00
    },
    
    // UFT_PRESET_QDOS - Sinclair QL
    {
        .cylinders = 80, .heads = 2, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 21, .fill = 0x00
    },
    
    // UFT_PRESET_AMIGA_DD - 80×2×11×512 (880K)
    {
        .cylinders = 80, .heads = 2, .sectors = 11, .size = 2,
        .fdc = UFT_FDC_AMIGA, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_AMIGA,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_AMIGA_HD - 80×2×22×512 (1.76MB)
    {
        .cylinders = 80, .heads = 2, .sectors = 22, .size = 2,
        .fdc = UFT_FDC_AMIGA, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_AMIGA,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_ATARIST_SS - 80×1×9×512 (360K)
    {
        .cylinders = 80, .heads = 1, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 40, .fill = 0x00
    },
    
    // UFT_PRESET_ATARIST_DS - 80×2×9×512 (720K)
    {
        .cylinders = 80, .heads = 2, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 40, .fill = 0x00
    },
    
    // UFT_PRESET_ATARIST_HD - 80×2×18×512 (1.44MB)
    {
        .cylinders = 80, .heads = 2, .sectors = 18, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 40, .fill = 0x00
    },
    
    // UFT_PRESET_C64_1541 - 35 tracks, GCR (variable sectors)
    {
        .cylinders = 35, .heads = 1, .sectors = 21, .size = 1,  // Max sectors
        .fdc = UFT_FDC_NONE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR_C64,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_C64_1571 - 70 tracks, GCR
    {
        .cylinders = 70, .heads = 1, .sectors = 21, .size = 1,
        .fdc = UFT_FDC_NONE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR_C64,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_C64_1581 - 80×2×10×512 MFM
    {
        .cylinders = 80, .heads = 2, .sectors = 10, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 30, .fill = 0x00
    },
    
    // UFT_PRESET_APPLE2_DOS - 35×1×16×256 GCR
    {
        .cylinders = 35, .heads = 1, .sectors = 16, .size = 1,
        .fdc = UFT_FDC_APPLE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR_APPLE,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_APPLE2_PRODOS - 35×1×16×256 GCR
    {
        .cylinders = 35, .heads = 1, .sectors = 16, .size = 1,
        .fdc = UFT_FDC_APPLE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR_APPLE,
        .base = 0, .interleave = 2, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_MAC_400K - 80×1×GCR (variable)
    {
        .cylinders = 80, .heads = 1, .sectors = 12, .size = 2,  // Average
        .fdc = UFT_FDC_APPLE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR,
        .base = 0, .interleave = 2, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_MAC_800K - 80×2×GCR (variable)
    {
        .cylinders = 80, .heads = 2, .sectors = 12, .size = 2,
        .fdc = UFT_FDC_APPLE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR,
        .base = 0, .interleave = 2, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_LIF - 77×2×16×256
    {
        .cylinders = 77, .heads = 2, .sectors = 16, .size = 1,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 30, .fill = 0x00
    },
    
    // UFT_PRESET_SAP - 80×16×256
    {
        .cylinders = 80, .heads = 1, .sectors = 16, .size = 1,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 7, .skew = 0, .gap3 = 20, .fill = 0xE5
    },
    
    // UFT_PRESET_PRODOS - 80×2×9×512
    {
        .cylinders = 80, .heads = 2, .sectors = 9, .size = 2,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 2, .skew = 2, .gap3 = 0x50, .fill = 0xE5
    },
    
    // UFT_PRESET_D80 - CBM 8050
    {
        .cylinders = 77, .heads = 1, .sectors = 29, .size = 1,  // Variable
        .fdc = UFT_FDC_NONE, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_GCR_C64,
        .base = 0, .interleave = 1, .skew = 0, .gap3 = 0, .fill = 0x00
    },
    
    // UFT_PRESET_D81 - CBM 1581
    {
        .cylinders = 80, .heads = 2, .sectors = 10, .size = 2,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM,
        .base = 1, .interleave = 1, .skew = 0, .gap3 = 30, .fill = 0x00
    },
};

// ============================================================================
// API Implementation
// ============================================================================

uft_format_params_t uft_format_get_preset(uft_format_preset_t preset) {
    if (preset >= UFT_PRESET_COUNT) {
        return g_presets[0];  // Return empty/custom
    }
    return g_presets[preset];
}

bool uft_format_validate(const uft_format_params_t* fmt) {
    if (!fmt) return false;
    if (fmt->cylinders < 1 || fmt->cylinders > 255) return false;
    if (fmt->heads < 1 || fmt->heads > 2) return false;
    if (fmt->sectors < 1 || fmt->sectors > 255) return false;
    if (fmt->size > 5) return false;
    return true;
}

const char* uft_encoding_name(uft_encoding_t encoding) {
    switch (encoding) {
        case UFT_ENCODING_FM:         return "FM";
        case UFT_ENCODING_MFM:        return "MFM";
        case UFT_ENCODING_M2FM:       return "M2FM";
        case UFT_ENCODING_GCR:        return "GCR";
        case UFT_ENCODING_GCR_APPLE:  return "Apple GCR";
        case UFT_ENCODING_GCR_C64:    return "C64 GCR";
        case UFT_ENCODING_GCR_VICTOR: return "Victor GCR";
        case UFT_ENCODING_GCR_BROTHER:return "Brother GCR";
        case UFT_ENCODING_AMIGA:      return "Amiga MFM";
        case UFT_ENCODING_RX02:       return "DEC RX02";
        default:                      return "Unknown";
    }
}

const char* uft_fdc_name(uft_fdc_type_t fdc) {
    switch (fdc) {
        case UFT_FDC_PC:    return "PC (NEC 765)";
        case UFT_FDC_WD:    return "Western Digital";
        case UFT_FDC_AMIGA: return "Amiga Custom";
        case UFT_FDC_APPLE: return "Apple IWM/SWIM";
        default:            return "None/Custom";
    }
}
