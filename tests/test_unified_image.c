/**
 * @file test_unified_image.c
 * @brief Unit Tests für Unified Image Model
 */

#include "uft/uft_unified_image.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define TEST(name) printf("TEST: %s... ", #name)
#define PASS() printf("PASS\n")
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_image_create_destroy(void) {
    TEST(image_create_destroy);
    
    uft_unified_image_t* img = uft_image_create();
    if (!img) FAIL("create returned NULL");
    
    uft_image_destroy(img);
    PASS();
    return 0;
}

static int test_image_geometry(void) {
    TEST(image_geometry);
    
    uft_unified_image_t* img = uft_image_create();
    if (!img) FAIL("create failed");
    
    // Set geometry
    img->geometry.cylinders = 80;
    img->geometry.heads = 2;
    img->geometry.sectors_per_track = 18;
    img->geometry.sector_size = 512;
    
    // Verify
    if (img->geometry.cylinders != 80) FAIL("cylinders mismatch");
    if (img->geometry.heads != 2) FAIL("heads mismatch");
    
    uft_image_destroy(img);
    PASS();
    return 0;
}

static int test_track_access(void) {
    TEST(track_access);
    
    uft_unified_image_t* img = uft_image_create();
    if (!img) FAIL("create failed");
    
    // Allocate tracks
    img->geometry.cylinders = 40;
    img->geometry.heads = 1;
    img->track_count = 40;
    img->tracks = calloc(40, sizeof(uft_unified_track_t));
    if (!img->tracks) FAIL("track alloc failed");
    
    // Access track
    uft_unified_track_t* track = uft_image_get_track(img, 10, 0);
    if (!track) FAIL("get_track returned NULL");
    
    uft_image_destroy(img);
    PASS();
    return 0;
}

static int test_layer_flags(void) {
    TEST(layer_flags);
    
    uft_unified_image_t* img = uft_image_create();
    if (!img) FAIL("create failed");
    
    // Initially no layers
    if (uft_image_has_layer(img, UFT_LAYER_FLUX)) FAIL("should not have flux");
    if (uft_image_has_layer(img, UFT_LAYER_SECTOR)) FAIL("should not have sector");
    
    uft_image_destroy(img);
    PASS();
    return 0;
}

int main(void) {
    printf("=== Unified Image Tests ===\n\n");
    
    int failures = 0;
    failures += test_image_create_destroy();
    failures += test_image_geometry();
    failures += test_track_access();
    failures += test_layer_flags();
    
    printf("\n%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}
