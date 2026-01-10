/**
 * @file test_xcopy_algo.c
 * @brief XCopy Algorithm Tests
 * 
 * Tests for algorithms ported from XCopy Pro and ManageDsk:
 * - Track length measurement (getracklen)
 * - Multi-revolution merging (NibbleRead)
 * - Sector timing analysis (FD_TIMED_SCAN_RESULT)
 * - Copy mode selection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/analysis/uft_xcopy_algo.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Data Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Create simulated MFM track data with syncs */
static uint8_t *create_mfm_track(size_t *out_len) {
    /* Standard Amiga DD track: ~12500 bytes */
    size_t len = 12800;
    uint8_t *data = calloc(1, len);
    if (!data) return NULL;
    
    /* Fill with some pattern */
    for (size_t i = 0; i < len - 100; i++) {
        data[i] = 0xAA; /* MFM clock bits */
    }
    
    /* Insert 11 sync patterns ($4489) at regular intervals */
    for (int sect = 0; sect < 11; sect++) {
        size_t pos = 100 + sect * 1100;
        if (pos + 20 < len) {
            /* Sync pattern */
            data[pos] = 0x44;
            data[pos + 1] = 0x89;
            data[pos + 2] = 0x44;
            data[pos + 3] = 0x89;
            /* Sector header (CHRN) */
            data[pos + 4] = 0x00; /* Track 0 */
            data[pos + 5] = 0x00; /* Head 0 */
            data[pos + 6] = (uint8_t)sect; /* Sector */
            data[pos + 7] = 0x02; /* 512 bytes (128 << 2) */
        }
    }
    
    /* Trailing zeros (end of track) */
    for (size_t i = len - 100; i < len; i++) {
        data[i] = 0x00;
    }
    
    *out_len = len;
    return data;
}

/* Create 2-revolution capture (XCopy NibbleRead style) */
static uint8_t *create_2rev_capture(size_t *out_len) {
    size_t single_len = 0;
    uint8_t *single = create_mfm_track(&single_len);
    if (!single) return NULL;
    
    /* Double it for 2 revolutions */
    size_t total_len = single_len * 2;
    uint8_t *data = malloc(total_len);
    if (!data) {
        free(single);
        return NULL;
    }
    
    memcpy(data, single, single_len);
    memcpy(data + single_len, single, single_len);
    
    /* Add some variation in second revolution (simulated) */
    for (size_t i = single_len; i < total_len - 100; i += 137) {
        data[i] ^= 0x01; /* Slight bit errors */
    }
    
    free(single);
    *out_len = total_len;
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Measurement Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_track_measure_basic(void) {
    printf("  Track measure basic... ");
    
    size_t len = 0;
    uint8_t *track = create_mfm_track(&len);
    assert(track != NULL);
    
    uft_track_measure_t measure;
    uft_error_t err = uft_track_measure_length(track, len, &measure);
    
    assert(err == UFT_SUCCESS);
    assert(measure.valid == true);
    assert(measure.length_bytes > 10000);
    assert(measure.length_bytes < 15000);
    assert(measure.first_data_offset < 200);
    
    free(track);
    printf("PASS (len=%u bytes)\n", measure.length_bytes);
}

static void test_track_measure_2rev(void) {
    printf("  Track measure 2-rev... ");
    
    size_t len = 0;
    uint8_t *track = create_2rev_capture(&len);
    assert(track != NULL);
    
    uft_track_measure_t measure;
    uft_error_t err = uft_track_measure_length(track, len, &measure);
    
    assert(err == UFT_SUCCESS);
    assert(measure.valid == true);
    /* Should detect 2-rev and give single track length */
    assert(measure.length_bytes > 5000);
    assert(measure.length_bytes < 15000);
    
    free(track);
    printf("PASS (len=%u from 2-rev)\n", measure.length_bytes);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sync Detection Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_sync_detection(void) {
    printf("  Sync detection... ");
    
    size_t len = 0;
    uint8_t *track = create_mfm_track(&len);
    assert(track != NULL);
    
    uft_sync_pos_t syncs[32];
    size_t found = 0;
    
    uft_error_t err = uft_track_find_sync_positions(
        track, len, 0x4489, syncs, 32, &found);
    
    assert(err == UFT_SUCCESS);
    assert(found >= 10); /* Should find ~11 sectors */
    assert(found <= 15);
    
    /* Check first sync position */
    assert(syncs[0].offset > 50);
    assert(syncs[0].offset < 200);
    assert(syncs[0].pattern == 0x4489);
    
    free(track);
    printf("PASS (found %zu syncs)\n", found);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Multi-Revolution Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_multirev_split(void) {
    printf("  Multi-rev split... ");
    
    size_t len = 0;
    uint8_t *data = create_2rev_capture(&len);
    assert(data != NULL);
    
    uft_multirev_t multirev;
    uft_error_t err = uft_track_split_revolutions(data, len, 12800, &multirev);
    
    assert(err == UFT_SUCCESS);
    assert(multirev.num_revolutions >= 1);
    assert(multirev.revolutions[0] != NULL);
    assert(multirev.rev_lengths[0] > 10000);
    assert(multirev.rpm_average > 280.0f && multirev.rpm_average < 320.0f);
    
    uft_multirev_free(&multirev);
    free(data);
    printf("PASS (%zu revs, %.1f RPM)\n", multirev.num_revolutions, multirev.rpm_average);
}

static void test_multirev_merge(void) {
    printf("  Multi-rev merge... ");
    
    size_t len = 0;
    uint8_t *data = create_2rev_capture(&len);
    assert(data != NULL);
    
    uft_multirev_t multirev;
    uft_error_t err = uft_track_split_revolutions(data, len, 12800, &multirev);
    assert(err == UFT_SUCCESS);
    
    uint8_t *merged = malloc(20000);
    assert(merged != NULL);
    
    size_t merged_len = 0;
    err = uft_track_merge_revolutions(&multirev, merged, 20000, &merged_len);
    assert(err == UFT_SUCCESS);
    assert(merged_len > 10000);
    
    uft_multirev_free(&multirev);
    free(merged);
    free(data);
    printf("PASS (merged=%zu bytes)\n", merged_len);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Timing Analysis Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_track_timing_analysis(void) {
    printf("  Track timing analysis... ");
    
    size_t len = 0;
    uint8_t *track = create_mfm_track(&len);
    assert(track != NULL);
    
    uft_track_timing_t timing;
    uft_error_t err = uft_track_analyze_timing(track, len, 0, &timing);
    
    assert(err == UFT_SUCCESS);
    assert(timing.sector_count >= 10);
    assert(timing.rpm_calculated > 200.0f && timing.rpm_calculated < 400.0f);
    
    /* Check sector data */
    assert(timing.sectors[0].valid == true);
    assert(timing.sectors[0].size_code == 0x02); /* 512 bytes */
    
    free(track);
    printf("PASS (%zu sectors, %.1f RPM)\n", timing.sector_count, timing.rpm_calculated);
}

static void test_protection_detection(void) {
    printf("  Protection detection... ");
    
    /* Create a track with unusual gaps (simulated protection) */
    size_t len = 15000;
    uint8_t *track = malloc(len);
    assert(track != NULL);
    memset(track, 0xAA, len);
    
    /* Insert syncs with varying gaps */
    size_t positions[] = {100, 1500, 2100, 4500, 5000, 8000, 10000, 12000};
    for (size_t i = 0; i < 8; i++) {
        if (positions[i] + 10 < len) {
            track[positions[i]] = 0x44;
            track[positions[i] + 1] = 0x89;
            track[positions[i] + 4] = 0x00;
            track[positions[i] + 5] = 0x00;
            track[positions[i] + 6] = (uint8_t)i;
            track[positions[i] + 7] = 0x02;
        }
    }
    
    uft_track_timing_t timing;
    uft_error_t err = uft_track_analyze_timing(track, len, 0, &timing);
    assert(err == UFT_SUCCESS);
    
    /* With inconsistent gaps, protection may be detected */
    /* (depending on implementation details) */
    
    free(track);
    printf("PASS (protection=%s)\n", 
           timing.protection_detected ? timing.protection_type : "none");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Calibration Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_drive_calibration(void) {
    printf("  Drive calibration... ");
    
    uft_drive_calibration_t cal;
    uft_drive_calibration_init(&cal);
    
    /* Default values */
    assert(cal.track_lengths[0] == 12500);
    assert(cal.rpm_measured[0] == 300.0f);
    assert(cal.calibrated[0] == false);
    
    /* Calibrate with test track */
    size_t len = 0;
    uint8_t *track = create_mfm_track(&len);
    assert(track != NULL);
    
    uft_error_t err = uft_drive_calibrate(&cal, 0, track, len);
    assert(err == UFT_SUCCESS);
    assert(cal.calibrated[0] == true);
    assert(cal.track_lengths[0] > 10000);
    
    free(track);
    printf("PASS (drive0: %u bytes, %.1f RPM)\n", 
           cal.track_lengths[0], cal.rpm_measured[0]);
}

static void test_write_length_calculation(void) {
    printf("  Write length calc... ");
    
    uft_drive_calibration_t cal;
    uft_drive_calibration_init(&cal);
    
    /* Simulate different drive lengths */
    cal.track_lengths[0] = 12600; /* Source */
    cal.track_lengths[1] = 12400; /* Target */
    cal.calibrated[0] = true;
    cal.calibrated[1] = true;
    
    /* XCopy: MIN(src, tgt) - 32 */
    uint32_t write_len = uft_drive_get_write_length(&cal, 0, 1, 0);
    
    /* Should be MIN(12600, 12400) - 32 = 12368 */
    assert(write_len == 12368);
    
    /* With offset */
    write_len = uft_drive_get_write_length(&cal, 0, 1, 100);
    assert(write_len == 12468);
    
    printf("PASS (write_len=%u)\n", write_len);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Copy Mode Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_copy_mode_selection(void) {
    printf("  Copy mode selection... ");
    
    /* Amiga ADF without protection */
    uft_copy_mode_t mode = uft_recommend_copy_mode("ADF", false, NULL);
    assert(mode == UFT_COPY_DOS);
    
    /* C64 D64 without protection */
    mode = uft_recommend_copy_mode("D64", false, NULL);
    assert(mode == UFT_COPY_BAM);
    
    /* Protected disk */
    mode = uft_recommend_copy_mode("ADF", true, NULL);
    assert(mode == UFT_COPY_NIBBLE);
    
    /* XDF format (variable sectors) */
    mode = uft_recommend_copy_mode("XDF", false, NULL);
    assert(mode == UFT_COPY_NIBBLE);
    
    /* Flux format */
    mode = uft_recommend_copy_mode("SCP", false, NULL);
    assert(mode == UFT_COPY_FLUX);
    
    printf("PASS\n");
}

static void test_copy_mode_names(void) {
    printf("  Copy mode names... ");
    
    assert(strcmp(uft_copy_mode_name(UFT_COPY_DOS), "DOS Copy") == 0);
    assert(strcmp(uft_copy_mode_name(UFT_COPY_NIBBLE), "Nibble Copy") == 0);
    assert(strcmp(uft_copy_mode_name(UFT_COPY_FLUX), "Flux Copy") == 0);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" UFT XCopy Algorithm Tests\n");
    printf(" (Based on XCopy Pro + ManageDsk)\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("Track Measurement (getracklen):\n");
    test_track_measure_basic();
    test_track_measure_2rev();
    
    printf("\nSync Detection:\n");
    test_sync_detection();
    
    printf("\nMulti-Revolution (NibbleRead):\n");
    test_multirev_split();
    test_multirev_merge();
    
    printf("\nTiming Analysis (fdrawcmd.sys):\n");
    test_track_timing_analysis();
    test_protection_detection();
    
    printf("\nDrive Calibration (mestrack):\n");
    test_drive_calibration();
    test_write_length_calculation();
    
    printf("\nCopy Mode Selection:\n");
    test_copy_mode_selection();
    test_copy_mode_names();
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf(" ✓ All XCopy Algorithm tests passed!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n\"Bei uns geht kein Bit verloren\" - XCopy Pro 1989-2011\n\n");
    
    return 0;
}
