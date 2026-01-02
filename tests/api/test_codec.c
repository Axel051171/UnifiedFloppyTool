/**
 * @file test_codec.c
 * @brief Unit tests for uft_codec.h
 */

#include "uft/codec/uft_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

static int passed = 0, failed = 0, total = 0;

TEST(codec_names) {
    if (!uft_codec_name(UFT_CODEC_FM)) return -1;
    if (!uft_codec_name(UFT_CODEC_MFM)) return -1;
    if (!uft_codec_name(UFT_CODEC_GCR_CBM)) return -1;
    if (!uft_codec_name(UFT_CODEC_GCR_APPLE)) return -1;
    if (!uft_codec_name(UFT_CODEC_AMIGA_MFM)) return -1;
    return 0;
}

TEST(codec_default_bitcell) {
    uint32_t bc;
    
    bc = uft_codec_default_bitcell(UFT_CODEC_FM);
    if (bc < 3000 || bc > 5000) return -1;  // ~4000ns for FM
    
    bc = uft_codec_default_bitcell(UFT_CODEC_MFM);
    if (bc < 1500 || bc > 2500) return -1;  // ~2000ns for MFM DD
    
    bc = uft_codec_default_bitcell(UFT_CODEC_GCR_CBM);
    if (bc < 2500 || bc > 4500) return -1;  // Variable for C64
    
    return 0;
}

TEST(codec_config_default_mfm) {
    uft_codec_config_t config;
    uft_codec_config_default(UFT_CODEC_MFM, &config);
    
    if (config.type != UFT_CODEC_MFM) return -1;
    if (config.bit_cell_ns == 0) return -1;
    if (config.pll_gain <= 0 || config.pll_gain >= 1) return -1;
    if (config.sync_bits == 0) return -1;
    
    return 0;
}

TEST(codec_config_default_gcr) {
    uft_codec_config_t config;
    uft_codec_config_default(UFT_CODEC_GCR_CBM, &config);
    
    if (config.type != UFT_CODEC_GCR_CBM) return -1;
    if (config.viterbi_depth < 16) return -1;
    if (config.enable_bitslip != true) return -1;
    
    return 0;
}

TEST(bitstream_init_free) {
    uft_bitstream_t bs;
    uft_bitstream_init(&bs);
    
    if (bs.data != NULL) return -1;
    if (bs.bit_count != 0) return -1;
    
    uft_bitstream_free(&bs);
    return 0;
}

TEST(sector_init_free) {
    uft_sector_t sector;
    uft_sector_init(&sector);
    
    if (sector.data != NULL) return -1;
    if (sector.data_size != 0) return -1;
    if (sector.confidence != 0.0) return -1;
    
    uft_sector_free(&sector);
    return 0;
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         UFT CODEC API TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    RUN(codec_names);
    RUN(codec_default_bitcell);
    RUN(codec_config_default_mfm);
    RUN(codec_config_default_gcr);
    RUN(bitstream_init_free);
    RUN(sector_init_free);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", passed, total, failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return (failed == 0) ? 0 : 1;
}
