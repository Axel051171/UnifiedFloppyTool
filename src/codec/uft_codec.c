/**
 * @file uft_codec.c
 * @brief Codec Implementation
 */

#include "uft/codec/uft_codec.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// CODEC NAMES
// ============================================================================

static const char* codec_names[] = {
    "AUTO",
    "FM",
    "MFM",
    "GCR-CBM",
    "GCR-Apple",
    "GCR-Apple53",
    "GCR-Victor",
    "Amiga-MFM"
};

const char* uft_codec_name(uft_codec_type_t type) {
    if (type < 0 || type >= UFT_CODEC_COUNT) return "UNKNOWN";
    return codec_names[type];
}

// ============================================================================
// DEFAULT BIT CELLS (nanoseconds)
// ============================================================================

static const uint32_t default_bitcells[] = {
    2000,   // AUTO (MFM default)
    4000,   // FM
    2000,   // MFM
    3200,   // GCR-CBM (average)
    4000,   // GCR-Apple
    4000,   // GCR-Apple53
    3200,   // GCR-Victor
    2000    // Amiga-MFM
};

uint32_t uft_codec_default_bitcell(uft_codec_type_t type) {
    if (type < 0 || type >= UFT_CODEC_COUNT) return 2000;
    return default_bitcells[type];
}

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

void uft_codec_config_default(uft_codec_type_t type, uft_codec_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->type = type;
    config->bit_cell_ns = uft_codec_default_bitcell(type);
    config->clock_tolerance_ns = config->bit_cell_ns / 4;
    
    // PLL defaults
    config->pll_gain = 0.05;
    config->pll_bandwidth = 0.02;
    config->pll_lock_bits = 32;
    
    // Sync defaults
    config->min_sync_bits = 8;
    
    // Error correction
    config->enable_correction = true;
    config->max_correction_bits = 2;
    config->enable_bitslip = (type == UFT_CODEC_GCR_CBM || type == UFT_CODEC_GCR_APPLE);
    config->max_bitslip = 3;
    
    // Viterbi (GCR only)
    if (type == UFT_CODEC_GCR_CBM || type == UFT_CODEC_GCR_APPLE) {
        config->viterbi_depth = 64;
        config->viterbi_candidates = 4;
    }
    
    // Type-specific sync patterns
    switch (type) {
        case UFT_CODEC_MFM:
        case UFT_CODEC_AMIGA_MFM:
            config->sync_pattern = 0x4489448944894489ULL;  // A1 sync
            config->sync_bits = 48;
            break;
        case UFT_CODEC_FM:
            config->sync_pattern = 0xF57E;
            config->sync_bits = 16;
            break;
        case UFT_CODEC_GCR_CBM:
            config->sync_pattern = 0x3FF;  // 10 ones
            config->sync_bits = 10;
            break;
        case UFT_CODEC_GCR_APPLE:
            config->sync_pattern = 0xD5AA;
            config->sync_bits = 16;
            break;
        default:
            break;
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void uft_bitstream_init(uft_bitstream_t* bs) {
    if (!bs) return;
    memset(bs, 0, sizeof(*bs));
}

void uft_bitstream_free(uft_bitstream_t* bs) {
    if (!bs) return;
    free(bs->data);
    free(bs->weak_mask);
    free(bs->clock_ns);
    memset(bs, 0, sizeof(*bs));
}

void uft_sector_init(uft_sector_t* sector) {
    if (!sector) return;
    memset(sector, 0, sizeof(*sector));
}

void uft_sector_free(uft_sector_t* sector) {
    if (!sector) return;
    free(sector->data);
    memset(sector, 0, sizeof(*sector));
}
