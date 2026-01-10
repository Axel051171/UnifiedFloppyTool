/**
 * @file test_xdf_adapter.c
 * @brief XDF Format Adapter Tests
 * 
 * Tests for the adapter-based format plugin system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/core/uft_score.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mock Format Adapters for Testing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Mock ADF probe */
static uft_format_score_t mock_adf_probe(const uint8_t *data, size_t size, const char *filename) {
    uft_format_score_t score = uft_score_init();
    
    if (size >= 901120) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_MEDIUM, true, "DD size");
    }
    
    /* Check for "DOS" in bootblock */
    if (size >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
        uft_score_add_match(&score, "magic", UFT_SCORE_WEIGHT_MAGIC, true, "DOS bootblock");
    }
    
    /* Check extension */
    if (filename) {
        size_t len = strlen(filename);
        if (len > 4) {
            const char *ext = filename + len - 4;
            if (strcasecmp(ext, ".adf") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".adf");
            }
        }
    }
    
    uft_score_finalize(&score);
    return score;
}

/* Mock D64 probe */
static uft_format_score_t mock_d64_probe(const uint8_t *data, size_t size, const char *filename) {
    uft_format_score_t score = uft_score_init();
    
    if (size == 174848 || size == 175531) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "D64 size");
    }
    
    /* Check extension */
    if (filename) {
        size_t len = strlen(filename);
        if (len > 4) {
            const char *ext = filename + len - 4;
            if (strcasecmp(ext, ".d64") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".d64");
            }
        }
    }
    
    uft_score_finalize(&score);
    return score;
}

/* Mock adapters */
static const uft_format_adapter_t mock_adf_adapter = {
    .name = "ADF",
    .description = "Amiga Disk File",
    .extensions = "adf, adz",
    .format_id = UFT_FORMAT_ID_ADF,
    .can_read = true,
    .can_write = true,
    .can_create = true,
    .supports_errors = false,
    .supports_timing = false,
    .probe = mock_adf_probe,
    .open = NULL,
    .read_track = NULL,
    .get_geometry = NULL,
    .write_track = NULL,
    .export_native = NULL,
    .close = NULL,
};

static const uft_format_adapter_t mock_d64_adapter = {
    .name = "D64",
    .description = "C64 Disk Image",
    .extensions = "d64",
    .format_id = UFT_FORMAT_ID_D64,
    .can_read = true,
    .can_write = true,
    .can_create = true,
    .supports_errors = true,
    .supports_timing = false,
    .probe = mock_d64_probe,
    .open = NULL,
    .read_track = NULL,
    .get_geometry = NULL,
    .write_track = NULL,
    .export_native = NULL,
    .close = NULL,
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_score_init(void) {
    printf("  Score init... ");
    
    uft_format_score_t score = uft_score_init();
    assert(score.overall == 0.0f);
    assert(score.valid == false);
    assert(score.match_count == 0);
    
    printf("PASS\n");
}

static void test_score_add_match(void) {
    printf("  Score add match... ");
    
    uft_format_score_t score = uft_score_init();
    
    uft_score_add_match(&score, "magic", UFT_SCORE_WEIGHT_MAGIC, true, NULL);
    assert(score.overall >= 0.49f && score.overall <= 0.51f);
    assert(score.match_count == 1);
    
    uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_MEDIUM, true, NULL);
    assert(score.overall >= 0.69f && score.overall <= 0.71f);
    assert(score.match_count == 2);
    
    /* Non-match should not change score */
    uft_score_add_match(&score, "checksum", UFT_SCORE_WEIGHT_HIGH, false, NULL);
    assert(score.overall >= 0.69f && score.overall <= 0.71f);
    assert(score.match_count == 3);
    
    printf("PASS\n");
}

static void test_score_finalize(void) {
    printf("  Score finalize... ");
    
    uft_format_score_t score = uft_score_init();
    score.overall = 0.65f;
    uft_score_finalize(&score);
    
    assert(score.valid == true);
    assert(uft_score_is_valid(&score) == true);
    
    /* Low score */
    score = uft_score_init();
    score.overall = 0.15f;
    uft_score_finalize(&score);
    
    assert(score.valid == false);
    assert(uft_score_is_valid(&score) == false);
    
    printf("PASS\n");
}

static void test_score_confidence(void) {
    printf("  Score confidence... ");
    
    uft_format_score_t score = uft_score_init();
    score.overall = 0.75f;
    score.valid = true;
    
    uint16_t conf = uft_score_to_confidence(&score);
    assert(conf == 7500);
    
    assert(uft_score_is_confident(&score) == false); /* 75% < 80% threshold */
    
    score.overall = 0.85f;
    assert(uft_score_is_confident(&score) == true);
    
    printf("PASS\n");
}

static void test_score_compare(void) {
    printf("  Score compare... ");
    
    uft_format_score_t a = uft_score_init();
    uft_format_score_t b = uft_score_init();
    
    a.overall = 0.8f;
    b.overall = 0.6f;
    
    assert(uft_score_compare(&a, &b) > 0);
    assert(uft_score_compare(&b, &a) < 0);
    
    b.overall = 0.8f;
    assert(uft_score_compare(&a, &b) == 0);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_adapter_register(void) {
    printf("  Adapter register... ");
    
    uft_error_t err = uft_adapter_register(&mock_adf_adapter);
    assert(err == UFT_SUCCESS);
    
    err = uft_adapter_register(&mock_d64_adapter);
    assert(err == UFT_SUCCESS);
    
    /* Duplicate should fail */
    err = uft_adapter_register(&mock_adf_adapter);
    assert(err == UFT_ERR_ALREADY_EXISTS);
    
    printf("PASS\n");
}

static void test_adapter_find_by_id(void) {
    printf("  Adapter find by ID... ");
    
    const uft_format_adapter_t *adapter = uft_adapter_find_by_id(UFT_FORMAT_ID_ADF);
    assert(adapter != NULL);
    assert(strcmp(adapter->name, "ADF") == 0);
    
    adapter = uft_adapter_find_by_id(UFT_FORMAT_ID_D64);
    assert(adapter != NULL);
    assert(strcmp(adapter->name, "D64") == 0);
    
    adapter = uft_adapter_find_by_id(0x9999);
    assert(adapter == NULL);
    
    printf("PASS\n");
}

static void test_adapter_find_by_extension(void) {
    printf("  Adapter find by extension... ");
    
    const uft_format_adapter_t *adapter = uft_adapter_find_by_extension("adf");
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_ADF);
    
    adapter = uft_adapter_find_by_extension("ADF");
    assert(adapter != NULL);
    
    adapter = uft_adapter_find_by_extension("adz");
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_ADF);
    
    adapter = uft_adapter_find_by_extension("d64");
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_D64);
    
    adapter = uft_adapter_find_by_extension("xyz");
    assert(adapter == NULL);
    
    printf("PASS\n");
}

static void test_adapter_probe(void) {
    printf("  Adapter probe... ");
    
    /* Create fake ADF data */
    uint8_t adf_data[901120] = {0};
    adf_data[0] = 'D';
    adf_data[1] = 'O';
    adf_data[2] = 'S';
    
    uft_format_score_t scores[10];
    size_t found = uft_adapter_probe_all(adf_data, sizeof(adf_data), "test.adf", scores, 10);
    
    assert(found >= 1);
    assert(scores[0].format_id == UFT_FORMAT_ID_ADF);
    assert(scores[0].overall > 0.5f);
    
    printf("PASS (found %zu matches)\n", found);
}

static void test_adapter_detect(void) {
    printf("  Adapter detect... ");
    
    /* D64 data */
    uint8_t d64_data[174848] = {0};
    
    uft_format_score_t score;
    const uft_format_adapter_t *adapter = uft_adapter_detect(
        d64_data, sizeof(d64_data), "game.d64", &score);
    
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_D64);
    assert(score.overall > 0.0f);
    
    printf("PASS (detected %s)\n", adapter->name);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track/Sector Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_track_data(void) {
    printf("  Track data... ");
    
    uft_track_data_t track;
    uft_track_data_init(&track);
    
    assert(track.track_num == 0);
    assert(track.sectors == NULL);
    assert(track.sector_count == 0);
    
    uft_error_t err = uft_track_alloc_sectors(&track, 11);
    assert(err == UFT_SUCCESS);
    assert(track.sectors != NULL);
    assert(track.sector_count == 11);
    
    track.sectors[0].sector_id = 5;
    track.sectors[1].sector_id = 8;
    track.sectors[2].sector_id = 2;
    
    uft_sector_data_t *found = uft_track_find_sector(&track, 8);
    assert(found != NULL);
    assert(found->sector_id == 8);
    
    found = uft_track_find_sector(&track, 99);
    assert(found == NULL);
    
    uft_track_data_free(&track);
    assert(track.sectors == NULL);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" XDF Format Adapter Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("Score Tests:\n");
    test_score_init();
    test_score_add_match();
    test_score_finalize();
    test_score_confidence();
    test_score_compare();
    
    printf("\nAdapter Tests:\n");
    test_adapter_register();
    test_adapter_find_by_id();
    test_adapter_find_by_extension();
    test_adapter_probe();
    test_adapter_detect();
    
    printf("\nTrack/Sector Tests:\n");
    test_track_data();
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf(" ✓ All XDF Adapter tests passed! (11 tests)\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}
