/**
 * @file test_track_unified.c
 * @brief Test für P1-4: Zentrale uft_track_t Definition
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft/uft_track.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* ============================================================================
 * Test 1: Allocation & Initialization
 * ============================================================================ */
static void test_alloc(void)
{
    printf("\n=== Test 1: Allocation ===\n");
    
    /* Test basic allocation */
    uft_track_t *track = uft_track_alloc(UFT_LAYER_BITSTREAM, 100000);
    TEST(track != NULL, "Track allocation");
    TEST(UFT_TRACK_VALID(track), "Track magic valid");
    TEST(track->_version == UFT_TRACK_VERSION, "Track version");
    TEST(uft_track_has_layer(track, UFT_LAYER_BITSTREAM), "Bitstream layer present");
    TEST(track->bitstream != NULL, "Bitstream allocated");
    TEST(track->bitstream->capacity >= 100000/8, "Bitstream capacity");
    
    uft_track_free(track);
    printf("  ✓ Allocation tests passed\n");
    
    /* Test with multiple layers */
    track = uft_track_alloc(UFT_LAYER_BITSTREAM | UFT_LAYER_SECTORS | UFT_LAYER_FLUX, 50000);
    TEST(track != NULL, "Multi-layer allocation");
    TEST(uft_track_has_layer(track, UFT_LAYER_BITSTREAM), "Has bitstream");
    TEST(uft_track_has_layer(track, UFT_LAYER_SECTORS), "Has sectors");
    TEST(uft_track_has_layer(track, UFT_LAYER_FLUX), "Has flux");
    TEST(track->sector_layer != NULL, "Sector layer allocated");
    TEST(track->flux != NULL, "Flux layer allocated");
    
    uft_track_free(track);
    printf("  ✓ Multi-layer tests passed\n");
}

/* ============================================================================
 * Test 2: Bitstream Operations
 * ============================================================================ */
static void test_bitstream(void)
{
    printf("\n=== Test 2: Bitstream Operations ===\n");
    
    uft_track_t *track = uft_track_alloc(UFT_LAYER_BITSTREAM, 1000);
    TEST(track != NULL, "Track allocated");
    
    /* Create test bitstream */
    uint8_t test_bits[125];  /* 1000 bits */
    for (int i = 0; i < 125; i++) {
        test_bits[i] = (uint8_t)(i * 17);  /* Some pattern */
    }
    
    /* Set bits */
    int ret = uft_track_set_bits(track, test_bits, 1000);
    TEST(ret == 0, "Set bits success");
    TEST(track->bitstream->bit_count == 1000, "Bit count correct");
    TEST(track->bitstream->byte_count == 125, "Byte count correct");
    
    /* Get bits back */
    uint8_t out_bits[125];
    size_t out_count;
    ret = uft_track_get_bits(track, out_bits, &out_count);
    TEST(ret == 0, "Get bits success");
    TEST(out_count == 1000, "Output count correct");
    TEST(memcmp(test_bits, out_bits, 125) == 0, "Bits match");
    
    /* Test timing */
    uint16_t timing[1000];
    for (int i = 0; i < 1000; i++) timing[i] = 2000 + (i % 100);
    
    ret = uft_track_set_timing(track, timing, 1000);
    TEST(ret == 0, "Set timing success");
    TEST(uft_track_has_layer(track, UFT_LAYER_TIMING), "Timing layer flag");
    TEST(track->bitstream->timing_count == 1000, "Timing count");
    
    /* Test weak mask */
    uint8_t weak[125] = {0};
    weak[10] = 0xFF;  /* Some weak bits */
    
    ret = uft_track_set_weak_mask(track, weak, 125);
    TEST(ret == 0, "Set weak mask success");
    TEST(uft_track_has_layer(track, UFT_LAYER_WEAK), "Weak layer flag");
    
    uft_track_free(track);
    printf("  ✓ Bitstream tests passed\n");
}

/* ============================================================================
 * Test 3: Sector Operations
 * ============================================================================ */
static void test_sectors(void)
{
    printf("\n=== Test 3: Sector Operations ===\n");
    
    uft_track_t *track = uft_track_alloc(UFT_LAYER_SECTORS, 0);
    TEST(track != NULL, "Track allocated");
    
    /* Add sectors */
    for (int i = 1; i <= 18; i++) {
        uft_sector_t sector = {
            .cylinder = 0,
            .head = 0,
            .sector_id = (uint8_t)i,
            .size_code = 2,
            .logical_size = 512,
            .crc_ok = true,
            .data = (uint8_t*)malloc(512),
            .data_len = 512
        };
        memset(sector.data, i, 512);
        
        int ret = uft_track_add_sector(track, &sector);
        TEST(ret == 0, "Add sector success");
        free(sector.data);  /* track makes its own copy */
    }
    
    TEST(UFT_TRACK_SECTOR_COUNT(track) == 18, "18 sectors added");
    TEST(track->sector_layer->good == 18, "18 good sectors");
    
    /* Legacy array too */
    TEST(track->sector_count == 18, "Legacy array populated");
    
    /* Find sector */
    const uft_sector_t *found = uft_track_get_sector(track, 10);
    TEST(found != NULL, "Found sector 10");
    TEST(found->sector_id == 10, "Correct sector ID");
    TEST(found->data[0] == 10, "Correct data");
    
    /* Get all */
    size_t count;
    const uft_sector_t *all = uft_track_get_sectors(track, &count);
    TEST(all != NULL, "Get all sectors");
    TEST(count == 18, "All 18 returned");
    
    uft_track_free(track);
    printf("  ✓ Sector tests passed\n");
}

/* ============================================================================
 * Test 4: Clone
 * ============================================================================ */
static void test_clone(void)
{
    printf("\n=== Test 4: Clone ===\n");
    
    /* Create original */
    uft_track_t *orig = uft_track_alloc(UFT_LAYER_BITSTREAM | UFT_LAYER_SECTORS, 5000);
    orig->cylinder = 40;
    orig->head = 1;
    orig->encoding = UFT_ENC_MFM;
    orig->nominal_bit_rate_kbps = 250.0;
    
    uint8_t bits[625];
    for (int i = 0; i < 625; i++) bits[i] = (uint8_t)i;
    uft_track_set_bits(orig, bits, 5000);
    
    uft_sector_t sector = {
        .sector_id = 1, .size_code = 2, .crc_ok = true,
        .data = (uint8_t[]){1,2,3,4}, .data_len = 4
    };
    uft_track_add_sector(orig, &sector);
    
    /* Clone */
    uft_track_t *clone = uft_track_clone(orig);
    TEST(clone != NULL, "Clone created");
    TEST(clone != orig, "Clone is different pointer");
    TEST(clone->cylinder == 40, "Cylinder copied");
    TEST(clone->head == 1, "Head copied");
    TEST(clone->encoding == UFT_ENC_MFM, "Encoding copied");
    TEST(UFT_TRACK_BIT_COUNT(clone) == 5000, "Bits copied");
    TEST(UFT_TRACK_SECTOR_COUNT(clone) == 1, "Sector copied");
    
    /* Compare */
    TEST(uft_track_compare(orig, clone) == 0, "Tracks compare equal");
    
    /* Modify clone, original unchanged */
    clone->cylinder = 50;
    TEST(orig->cylinder == 40, "Original unchanged");
    
    uft_track_free(orig);
    uft_track_free(clone);
    printf("  ✓ Clone tests passed\n");
}

/* ============================================================================
 * Test 5: Layer Management
 * ============================================================================ */
static void test_layers(void)
{
    printf("\n=== Test 5: Layer Management ===\n");
    
    uft_track_t *track = uft_track_alloc(0, 0);  /* No initial layers */
    TEST(track != NULL, "Empty track created");
    TEST(track->available_layers == 0, "No layers initially");
    
    /* Add layers dynamically */
    int ret = uft_track_add_layer(track, UFT_LAYER_BITSTREAM, 10000);
    TEST(ret == 0, "Add bitstream layer");
    TEST(uft_track_has_layer(track, UFT_LAYER_BITSTREAM), "Bitstream flag set");
    
    ret = uft_track_add_layer(track, UFT_LAYER_FLUX, 50000);
    TEST(ret == 0, "Add flux layer");
    TEST(uft_track_has_layer(track, UFT_LAYER_FLUX), "Flux flag set");
    
    /* Remove layer */
    uft_track_remove_layer(track, UFT_LAYER_FLUX);
    TEST(!uft_track_has_layer(track, UFT_LAYER_FLUX), "Flux removed");
    TEST(track->flux == NULL, "Flux pointer NULL");
    TEST(uft_track_has_layer(track, UFT_LAYER_BITSTREAM), "Bitstream still there");
    
    uft_track_free(track);
    printf("  ✓ Layer management tests passed\n");
}

/* ============================================================================
 * Test 6: Validation
 * ============================================================================ */
static void test_validation(void)
{
    printf("\n=== Test 6: Validation ===\n");
    
    uft_track_t *track = uft_track_alloc(UFT_LAYER_BITSTREAM, 1000);
    
    TEST(uft_track_validate(track) == 0, "Valid track passes");
    TEST(uft_track_validate(NULL) == -1, "NULL fails");
    
    /* Corrupt magic */
    uint32_t saved_magic = track->_magic;
    track->_magic = 0xDEADBEEF;
    TEST(uft_track_validate(track) == -2, "Bad magic fails");
    track->_magic = saved_magic;
    
    /* Invalid cylinder */
    track->cylinder = 100;
    TEST(uft_track_validate(track) == -3, "Bad cylinder fails");
    track->cylinder = 40;
    
    /* Status string */
    char buf[128];
    uft_track_status_str(track, buf, sizeof(buf));
    TEST(strlen(buf) > 0, "Status string generated");
    printf("  Status: %s\n", buf);
    
    uft_track_free(track);
    printf("  ✓ Validation tests passed\n");
}

/* ============================================================================
 * Test 7: Legacy Compatibility
 * ============================================================================ */
static void test_legacy(void)
{
    printf("\n=== Test 7: Legacy Compatibility ===\n");
    
    uft_track_t *track = uft_track_alloc(UFT_LAYER_SECTORS, 0);
    
    /* Old-style fields still work */
    track->cylinder = 10;
    track->head = 0;
    track->bitrate = 250000;
    track->rpm = 300;
    track->decoded = true;
    track->errors = 2;
    track->quality = 0.95f;
    
    /* Add sector populates both old and new */
    uft_sector_t sector = {
        .sector_id = 1, .crc_ok = true,
        .data = NULL, .data_len = 0
    };
    uft_track_add_sector(track, &sector);
    
    TEST(track->sector_count == 1, "Legacy sector_count");
    TEST(track->sectors[0].sector_id == 1, "Legacy sectors array");
    TEST(UFT_TRACK_SECTOR_COUNT(track) == 1, "New macro works");
    
    uft_track_free(track);
    printf("  ✓ Legacy compatibility tests passed\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */
int main(void)
{
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║        P1-4: Unified Track API Tests                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    test_alloc();
    test_bitstream();
    test_sectors();
    test_clone();
    test_layers();
    test_validation();
    test_legacy();
    
    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("════════════════════════════════════════════════════════════════\n");
    
    if (tests_failed > 0) {
        printf("❌ SOME TESTS FAILED\n");
        return 1;
    }
    
    printf("✓ ALL TESTS PASSED\n");
    printf("\nP1-4 Status: uft_track_t Zentralisierung ERFOLGREICH\n");
    printf("  - Superset aller 7 Track-Definitionen\n");
    printf("  - Bitstream + Timing + Weak-Bits\n");
    printf("  - Sector Layer\n");
    printf("  - Flux Layer\n");
    printf("  - Legacy-Kompatibilität\n");
    
    return 0;
}
