/**
 * @file uft_format_params.c
 * @brief Format parameter implementation with presets
 */

#include "uft/formats/uft_format_params.h"
#include <stddef.h>

// ============================================================================
// Preset Format Definitions (from SAMdisk Format.cpp)
// ============================================================================

static const uft_regular_format_t g_preset_formats[] = {
    // UFT_PRESET_MGT - 800K MGT +D
    [UFT_PRESET_MGT] = {
        .cyls = 80, .heads = 2, .sectors = 10, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 24,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PRODOS - 720K Apple ProDOS
    [UFT_PRESET_PRODOS] = {
        .cyls = 80, .heads = 2, .sectors = 9, .size = 2,
        .base = 1, .interleave = 2, .skew = 2, .gap3 = 0x50, .fill = 0xE5,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC320 - 320K PC
    [UFT_PRESET_PC320] = {
        .cyls = 40, .heads = 2, .sectors = 8, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC360 - 360K PC
    [UFT_PRESET_PC360] = {
        .cyls = 40, .heads = 2, .sectors = 9, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC640 - 640K PC
    [UFT_PRESET_PC640] = {
        .cyls = 80, .heads = 2, .sectors = 8, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xE5,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC720 - 720K PC
    [UFT_PRESET_PC720] = {
        .cyls = 80, .heads = 2, .sectors = 9, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC1200 - 1.2MB PC
    [UFT_PRESET_PC1200] = {
        .cyls = 80, .heads = 2, .sectors = 15, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x54, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC1232 - 1.232MB PC (Japanese)
    [UFT_PRESET_PC1232] = {
        .cyls = 77, .heads = 2, .sectors = 8, .size = 3,  // 1024 bytes/sector
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x54, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC1440 - 1.44MB PC
    [UFT_PRESET_PC1440] = {
        .cyls = 80, .heads = 2, .sectors = 18, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x54, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_PC2880 - 2.88MB PC
    [UFT_PRESET_PC2880] = {
        .cyls = 80, .heads = 2, .sectors = 36, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x53, .fill = 0xF6,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_1000K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_AMIGADOS - Amiga DD
    [UFT_PRESET_AMIGADOS] = {
        .cyls = 80, .heads = 2, .sectors = 11, .size = 2,
        .base = 0, .interleave = 1, .gap3 = 0,
        .fdc = UFT_FDC_AMIGA, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_AMIGA
    },
    
    // UFT_PRESET_AMIGADOS_HD - Amiga HD
    [UFT_PRESET_AMIGADOS_HD] = {
        .cyls = 80, .heads = 2, .sectors = 22, .size = 2,
        .base = 0, .interleave = 1, .gap3 = 0,
        .fdc = UFT_FDC_AMIGA, .datarate = UFT_DATARATE_500K, .encoding = UFT_ENCODING_AMIGA
    },
    
    // UFT_PRESET_D81 - Commodore 1581
    [UFT_PRESET_D81] = {
        .cyls = 80, .heads = 2, .sectors = 10, .size = 2,
        .base = 1, .interleave = 1, .gap3 = 0x50,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_ATARI_ST - Atari ST
    [UFT_PRESET_ATARI_ST] = {
        .cyls = 80, .heads = 2, .sectors = 9, .size = 2,
        .base = 1, .interleave = 1, .skew = 1, .gap3 = 0x50,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_LIF - HP LIF
    [UFT_PRESET_LIF] = {
        .cyls = 77, .heads = 2, .sectors = 16, .size = 1,  // 256 bytes/sector
        .base = 0, .interleave = 2, .gap3 = 0x50,
        .fdc = UFT_FDC_PC, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_TRDOS - TR-DOS (Spectrum)
    [UFT_PRESET_TRDOS] = {
        .cyls = 80, .heads = 2, .sectors = 16, .size = 1,  // 256 bytes/sector
        .base = 1, .interleave = 1, .gap3 = 0x2A,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // UFT_PRESET_QDOS - Sinclair QL
    [UFT_PRESET_QDOS] = {
        .cyls = 80, .heads = 2, .sectors = 9, .size = 2,
        .base = 1, .interleave = 1, .gap3 = 0x50,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    // Thomson formats
    [UFT_PRESET_TO_640K_MFM] = {
        .cyls = 80, .heads = 2, .sectors = 16, .size = 1,
        .base = 1, .interleave = 7, .gap3 = 0x46,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
    
    [UFT_PRESET_TO_320K_MFM] = {
        .cyls = 80, .heads = 1, .sectors = 16, .size = 1,
        .base = 1, .interleave = 7, .gap3 = 0x46,
        .fdc = UFT_FDC_WD, .datarate = UFT_DATARATE_250K, .encoding = UFT_ENCODING_MFM
    },
};

// ============================================================================
// Default IBM Parameters
// ============================================================================

static const uft_ibm_params_t g_default_ibm_params = {
    .emit_iam = true,
    .use_fm = false,
    .idam_byte = 0x5554,
    .dam_byte = 0x5545,
    .gap_fill_byte = 0x9254,
    .gap0 = 80,
    .gap1 = 50,
    .gap2 = 22,
    .gap3 = 80,
    .ignore_side_byte = false,
    .ignore_track_byte = false,
    .invert_side_byte = false,
};

// ============================================================================
// Default Timing Parameters
// ============================================================================

static const uft_timing_params_t g_default_timing_250k = {
    .clock_period_us = 4.0,
    .rotational_period_ms = 200.0,  // 300 RPM
    .post_index_gap_ms = 0.5,
    .pre_header_sync_bits = 80,
    .pre_data_sync_bits = 40,
    .post_data_gap_bits = 80,
};

static const uft_timing_params_t g_default_timing_500k = {
    .clock_period_us = 2.0,
    .rotational_period_ms = 200.0,
    .post_index_gap_ms = 0.25,
    .pre_header_sync_bits = 80,
    .pre_data_sync_bits = 40,
    .post_data_gap_bits = 80,
};

static const uft_timing_params_t g_default_timing_1000k = {
    .clock_period_us = 1.0,
    .rotational_period_ms = 200.0,
    .post_index_gap_ms = 0.125,
    .pre_header_sync_bits = 80,
    .pre_data_sync_bits = 40,
    .post_data_gap_bits = 80,
};

// ============================================================================
// API Implementation
// ============================================================================

const uft_regular_format_t* uft_get_preset_format(uft_format_preset_t preset) {
    if (preset >= sizeof(g_preset_formats) / sizeof(g_preset_formats[0])) {
        return NULL;
    }
    return &g_preset_formats[preset];
}

const uft_ibm_params_t* uft_get_default_ibm_params(void) {
    return &g_default_ibm_params;
}

const uft_timing_params_t* uft_get_default_timing(uft_datarate_t datarate) {
    switch (datarate) {
        case UFT_DATARATE_250K:
        case UFT_DATARATE_300K:
            return &g_default_timing_250k;
        case UFT_DATARATE_500K:
            return &g_default_timing_500k;
        case UFT_DATARATE_1000K:
            return &g_default_timing_1000k;
        default:
            return &g_default_timing_250k;
    }
}
