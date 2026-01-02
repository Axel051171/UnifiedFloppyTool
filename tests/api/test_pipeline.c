/**
 * @file test_pipeline.c
 * @brief Unit tests for uft_pipeline.h
 * 
 * GOD MODE: Kein Code ohne Test
 */

#include "uft/pipeline/uft_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) static int test_##name(void)
#define RUN(name) do { \
    printf("  TEST: %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

static int passed = 0, failed = 0, total = 0;

// ============================================================================
// TESTS
// ============================================================================

TEST(pipeline_create_destroy) {
    uft_pipeline_t* pipe = uft_pipeline_create();
    if (!pipe) return -1;
    uft_pipeline_destroy(pipe);
    return 0;
}

TEST(pipeline_config_default) {
    uft_pipeline_config_t config;
    uft_pipeline_config_default(&config);
    
    // Check sane defaults
    if (config.revolutions < 1 || config.revolutions > 20) return -1;
    if (config.heads < 1 || config.heads > 2) return -1;
    if (config.verify_export != true) return -1;
    
    return 0;
}

TEST(pipeline_configure) {
    uft_pipeline_t* pipe = uft_pipeline_create();
    if (!pipe) return -1;
    
    uft_pipeline_config_t config;
    uft_pipeline_config_default(&config);
    config.revolutions = 5;
    config.start_cylinder = 0;
    config.end_cylinder = 79;
    
    int result = uft_pipeline_configure(pipe, &config);
    uft_pipeline_destroy(pipe);
    
    return (result == 0) ? 0 : -1;
}

TEST(pipeline_stage_names) {
    const char* name;
    
    name = uft_pipeline_stage_name(UFT_STAGE_IDLE);
    if (!name || strlen(name) == 0) return -1;
    
    name = uft_pipeline_stage_name(UFT_STAGE_ACQUIRE);
    if (!name || strlen(name) == 0) return -1;
    
    name = uft_pipeline_stage_name(UFT_STAGE_DECODE);
    if (!name || strlen(name) == 0) return -1;
    
    name = uft_pipeline_stage_name(UFT_STAGE_NORMALIZE);
    if (!name || strlen(name) == 0) return -1;
    
    name = uft_pipeline_stage_name(UFT_STAGE_EXPORT);
    if (!name || strlen(name) == 0) return -1;
    
    name = uft_pipeline_stage_name(UFT_STAGE_VERIFY);
    if (!name || strlen(name) == 0) return -1;
    
    return 0;
}

TEST(flux_init_free) {
    uft_flux_t flux;
    memset(&flux, 0xFF, sizeof(flux));  // Garbage
    
    // Should handle clean init
    flux.flux_data = NULL;
    flux.flux_count = 0;
    flux.index_times = NULL;
    flux.index_count = 0;
    
    uft_flux_free(&flux);  // Should not crash
    return 0;
}

TEST(bitstream_init_free) {
    uft_bitstream_t bits;
    memset(&bits, 0xFF, sizeof(bits));
    
    bits.bits = NULL;
    bits.bit_count = 0;
    bits.weak_mask = NULL;
    bits.timing = NULL;
    
    uft_bitstream_free(&bits);
    return 0;
}

TEST(track_init_free) {
    uft_track_t track;
    memset(&track, 0, sizeof(track));
    
    track.sectors = NULL;
    track.sector_count = 0;
    track.raw_data = NULL;
    
    uft_track_free(&track);
    return 0;
}

TEST(image_init_free) {
    uft_image_t image;
    memset(&image, 0, sizeof(image));
    
    image.tracks = NULL;
    image.track_count = 0;
    
    uft_image_free(&image);
    return 0;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         UFT PIPELINE API TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    RUN(pipeline_create_destroy);
    RUN(pipeline_config_default);
    RUN(pipeline_configure);
    RUN(pipeline_stage_names);
    RUN(flux_init_free);
    RUN(bitstream_init_free);
    RUN(track_init_free);
    RUN(image_init_free);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════════\n");
    printf("         RESULTS: %d/%d passed, %d failed\n", passed, total, failed);
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    return (failed == 0) ? 0 : 1;
}
