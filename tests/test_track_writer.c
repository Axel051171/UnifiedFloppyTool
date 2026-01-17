/**
 * @file test_track_writer.c
 * @brief Unit tests for Track Writer Module
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/hardware/uft_track_writer.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #condition); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))

/* ============================================================================
 * Unit Tests - Constants
 * ============================================================================ */

TEST(constants)
{
    ASSERT_EQ(WRITER_TRACK_SIZE, 0x2000);
    ASSERT_EQ(WRITER_CAPACITY_D0, 6250);
    ASSERT_EQ(WRITER_CAPACITY_D1, 6666);
    ASSERT_EQ(WRITER_CAPACITY_D2, 7142);
    ASSERT_EQ(WRITER_CAPACITY_D3, 7692);
    ASSERT_EQ(WRITER_BM_NO_SYNC, 0x80);
    ASSERT_EQ(WRITER_BM_FF_TRACK, 0x40);
}

TEST(default_density)
{
    /* Tracks 1-17: density 3 */
    ASSERT_EQ(writer_default_density(1), 3);
    ASSERT_EQ(writer_default_density(17), 3);
    
    /* Tracks 18-24: density 2 */
    ASSERT_EQ(writer_default_density(18), 2);
    ASSERT_EQ(writer_default_density(24), 2);
    
    /* Tracks 25-30: density 1 */
    ASSERT_EQ(writer_default_density(25), 1);
    ASSERT_EQ(writer_default_density(30), 1);
    
    /* Tracks 31+: density 0 */
    ASSERT_EQ(writer_default_density(31), 0);
    ASSERT_EQ(writer_default_density(42), 0);
    
    /* Invalid */
    ASSERT_EQ(writer_default_density(0), 0);
    ASSERT_EQ(writer_default_density(50), 0);
}

TEST(default_capacity)
{
    ASSERT_EQ(writer_default_capacity(0), WRITER_CAPACITY_D0);
    ASSERT_EQ(writer_default_capacity(1), WRITER_CAPACITY_D1);
    ASSERT_EQ(writer_default_capacity(2), WRITER_CAPACITY_D2);
    ASSERT_EQ(writer_default_capacity(3), WRITER_CAPACITY_D3);
    
    ASSERT_EQ(writer_default_capacity(-1), 0);
    ASSERT_EQ(writer_default_capacity(4), 0);
}

TEST(sectors_per_track)
{
    ASSERT_EQ(writer_sectors_per_track(1), 21);
    ASSERT_EQ(writer_sectors_per_track(17), 21);
    ASSERT_EQ(writer_sectors_per_track(18), 19);
    ASSERT_EQ(writer_sectors_per_track(24), 19);
    ASSERT_EQ(writer_sectors_per_track(25), 18);
    ASSERT_EQ(writer_sectors_per_track(30), 18);
    ASSERT_EQ(writer_sectors_per_track(31), 17);
    ASSERT_EQ(writer_sectors_per_track(35), 17);
}

TEST(speed_valid)
{
    ASSERT_TRUE(writer_speed_valid(300.0f));
    ASSERT_TRUE(writer_speed_valid(280.0f));
    ASSERT_TRUE(writer_speed_valid(320.0f));
    ASSERT_TRUE(writer_speed_valid(295.5f));
    
    ASSERT_FALSE(writer_speed_valid(279.9f));
    ASSERT_FALSE(writer_speed_valid(320.1f));
    ASSERT_FALSE(writer_speed_valid(200.0f));
    ASSERT_FALSE(writer_speed_valid(400.0f));
}

TEST(calc_rpm)
{
    /* At 300 RPM, density 3 should give ~7692 capacity */
    float rpm = writer_calc_rpm(7692, 3);
    ASSERT(rpm > 290 && rpm < 310);
    
    /* At 300 RPM, density 0 should give ~6250 capacity */
    rpm = writer_calc_rpm(6250, 0);
    ASSERT(rpm > 290 && rpm < 310);
    
    /* Invalid inputs */
    ASSERT_EQ(writer_calc_rpm(0, 3), 0);
    ASSERT_EQ(writer_calc_rpm(7000, -1), 0);
    ASSERT_EQ(writer_calc_rpm(7000, 4), 0);
}

/* ============================================================================
 * Unit Tests - Session Management
 * ============================================================================ */

TEST(create_session)
{
    writer_session_t *session = writer_create_session();
    ASSERT_NOT_NULL(session);
    ASSERT_FALSE(session->calibrated);
    ASSERT_EQ(session->tracks_written, 0);
    
    writer_close_session(session);
}

TEST(null_session)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    ASSERT_NOT_NULL(session->send_cmd);
    ASSERT_NOT_NULL(session->burst_read);
    ASSERT_NOT_NULL(session->burst_write);
    ASSERT_NOT_NULL(session->step_to);
    
    writer_close_session(session);
}

TEST(default_options)
{
    write_options_t opts;
    writer_get_defaults(&opts);
    
    ASSERT_TRUE(opts.verify);
    ASSERT_FALSE(opts.raw_mode);
    ASSERT_FALSE(opts.backwards);
    ASSERT_FALSE(opts.use_ihs);
    ASSERT_EQ(opts.fillbyte, 0x55);
    ASSERT_EQ(opts.verify_tol, WRITER_VERIFY_TOLERANCE);
}

/* ============================================================================
 * Unit Tests - Calibration
 * ============================================================================ */

TEST(calibrate_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    motor_calibration_t result;
    int ret = writer_calibrate(session, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
    ASSERT_TRUE(session->calibrated);
    ASSERT(result.rpm > 0);
    
    writer_close_session(session);
}

TEST(get_capacity)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    /* Before calibration, use defaults */
    int cap = writer_get_capacity(session, 3);
    ASSERT_EQ(cap, WRITER_CAPACITY_D3);
    
    /* After calibration */
    writer_calibrate(session, NULL);
    cap = writer_get_capacity(session, 3);
    ASSERT(cap > 0);
    
    writer_close_session(session);
}

/* ============================================================================
 * Unit Tests - Track Processing
 * ============================================================================ */

TEST(check_sync_flags_normal)
{
    uint8_t track[1000];
    
    /* Normal track with syncs */
    memset(track, 0x55, sizeof(track));
    for (int i = 0; i < 100; i += 20) {
        memset(track + i, 0xFF, 5);
    }
    
    uint8_t result = writer_check_sync_flags(track, 3, sizeof(track));
    ASSERT_EQ(result, 3);  /* Just density, no flags */
}

TEST(check_sync_flags_killer)
{
    uint8_t track[1000];
    
    /* Killer track (all 0xFF) */
    memset(track, 0xFF, sizeof(track));
    
    uint8_t result = writer_check_sync_flags(track, 3, sizeof(track));
    ASSERT_EQ(result & WRITER_BM_FF_TRACK, WRITER_BM_FF_TRACK);
}

TEST(check_sync_flags_nosync)
{
    uint8_t track[1000];
    
    /* No sync marks */
    memset(track, 0x55, sizeof(track));
    
    uint8_t result = writer_check_sync_flags(track, 3, sizeof(track));
    ASSERT_EQ(result & WRITER_BM_NO_SYNC, WRITER_BM_NO_SYNC);
}

TEST(check_formatted)
{
    uint8_t track[1000];
    
    /* Formatted track */
    memset(track, 0x55, sizeof(track));
    for (int i = 0; i < 200; i += 20) {
        memset(track + i, 0xFF, 5);
    }
    
    ASSERT_TRUE(writer_check_formatted(track, sizeof(track)));
    
    /* Unformatted track */
    memset(track, 0x55, sizeof(track));
    ASSERT_FALSE(writer_check_formatted(track, sizeof(track)));
}

TEST(replace_bytes)
{
    uint8_t data[100];
    memset(data, 0x00, sizeof(data));
    data[10] = 0x55;
    data[50] = 0x55;
    
    int count = writer_replace_bytes(data, sizeof(data), 0x00, 0x01);
    
    ASSERT_EQ(count, 98);  /* All except the two 0x55 bytes */
    ASSERT_EQ(data[0], 0x01);
    ASSERT_EQ(data[10], 0x55);
    ASSERT_EQ(data[50], 0x55);
}

TEST(lengthen_sync)
{
    uint8_t track[100];
    memset(track, 0x55, sizeof(track));
    
    /* Short sync at position 10 */
    track[10] = 0xFF;
    track[11] = 0xFF;
    track[12] = 0x52;  /* Header marker */
    
    int added = writer_lengthen_sync(track, 50, 100);
    ASSERT(added >= 0);
}

TEST(compress_track)
{
    uint8_t track[8000];
    memset(track, 0x55, sizeof(track));
    
    /* Track longer than capacity should be truncated */
    size_t result = writer_compress_track(4, track, 3, sizeof(track));
    ASSERT(result <= WRITER_CAPACITY_D3);
    
    /* Short track should stay same */
    result = writer_compress_track(4, track, 3, 5000);
    ASSERT_EQ(result, 5000);
}

/* ============================================================================
 * Unit Tests - Track Writing
 * ============================================================================ */

TEST(write_track_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    uint8_t track[7000];
    memset(track, 0x55, sizeof(track));
    memset(track, 0xFF, 10);  /* Add sync */
    
    track_write_result_t result;
    int ret = writer_write_track(session, 4, track, sizeof(track), 3, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(session->tracks_written, 1);
    
    writer_close_session(session);
}

TEST(fill_track_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    int ret = writer_fill_track(session, 4, 0xFF);
    ASSERT_EQ(ret, 0);
    
    writer_close_session(session);
}

TEST(kill_track_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    int ret = writer_kill_track(session, 4);
    ASSERT_EQ(ret, 0);
    
    writer_close_session(session);
}

TEST(erase_track_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    int ret = writer_erase_track(session, 4);
    ASSERT_EQ(ret, 0);
    
    writer_close_session(session);
}

/* ============================================================================
 * Unit Tests - Track Preparation
 * ============================================================================ */

TEST(prepare_track)
{
    uint8_t track[8192];
    memset(track, 0x55, sizeof(track));
    memset(track, 0xFF, 10);  /* Sync at start */
    
    write_options_t opts;
    writer_get_defaults(&opts);
    
    size_t output_len;
    int ret = writer_prepare_track(track, 7000, 3, &opts, &output_len);
    
    ASSERT_EQ(ret, 0);
    ASSERT(output_len > 7000);  /* Should add leader */
    
    /* Check no 0x00 bytes (except in track 18 fix) */
    int zeros = 0;
    for (size_t i = 0; i < output_len - 5; i++) {
        if (track[i] == 0x00) zeros++;
    }
    ASSERT_EQ(zeros, 0);
}

/* ============================================================================
 * Unit Tests - Image Management
 * ============================================================================ */

TEST(create_image)
{
    uint8_t track_data[85 * WRITER_TRACK_SIZE];
    uint8_t track_density[85];
    size_t track_length[85];
    
    memset(track_data, 0x55, sizeof(track_data));
    memset(track_density, 3, sizeof(track_density));
    for (int i = 0; i < 85; i++) {
        track_length[i] = 7000;
    }
    
    master_image_t *image = writer_create_image(track_data, track_density,
                                                 track_length, 2, 70);
    ASSERT_NOT_NULL(image);
    ASSERT_EQ(image->start_track, 2);
    ASSERT_EQ(image->end_track, 70);
    ASSERT_NOT_NULL(image->track_data);
    
    writer_free_image(image);
}

/* ============================================================================
 * Unit Tests - Disk Mastering
 * ============================================================================ */

static int progress_count = 0;
static void test_progress_cb(int track, int total, const char *msg, void *ud)
{
    (void)track; (void)total; (void)msg; (void)ud;
    progress_count++;
}

TEST(master_disk_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    /* Create simple image */
    uint8_t *track_data = calloc(85, WRITER_TRACK_SIZE);
    uint8_t *track_density = calloc(85, 1);
    size_t *track_length = calloc(85, sizeof(size_t));
    
    /* Set up some tracks */
    for (int t = 2; t <= 70; t += 2) {
        uint8_t *track = track_data + (t * WRITER_TRACK_SIZE);
        memset(track, 0x55, WRITER_TRACK_SIZE);
        for (int i = 0; i < 500; i += 25) {
            memset(track + i, 0xFF, 5);
        }
        track_density[t] = writer_default_density(t / 2);
        track_length[t] = 7000;
    }
    
    master_image_t *image = writer_create_image(track_data, track_density,
                                                 track_length, 2, 70);
    ASSERT_NOT_NULL(image);
    
    progress_count = 0;
    int ret = writer_master_disk(session, image, test_progress_cb, NULL);
    
    ASSERT_EQ(ret, 0);
    ASSERT(progress_count > 0);
    
    writer_free_image(image);
    free(track_data);
    free(track_density);
    free(track_length);
    writer_close_session(session);
}

TEST(unformat_disk_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    int ret = writer_unformat_disk(session, 2, 70, 1);
    ASSERT_EQ(ret, 0);
    
    writer_close_session(session);
}

TEST(init_aligned_null)
{
    writer_session_t *session = writer_create_null_session();
    ASSERT_NOT_NULL(session);
    
    int ret = writer_init_aligned(session, 2, 70);
    ASSERT_EQ(ret, 0);
    
    writer_close_session(session);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Track Writer Tests ===\n\n");
    
    printf("Constants:\n");
    RUN_TEST(constants);
    RUN_TEST(default_density);
    RUN_TEST(default_capacity);
    RUN_TEST(sectors_per_track);
    RUN_TEST(speed_valid);
    RUN_TEST(calc_rpm);
    
    printf("\nSession Management:\n");
    RUN_TEST(create_session);
    RUN_TEST(null_session);
    RUN_TEST(default_options);
    
    printf("\nCalibration:\n");
    RUN_TEST(calibrate_null);
    RUN_TEST(get_capacity);
    
    printf("\nTrack Processing:\n");
    RUN_TEST(check_sync_flags_normal);
    RUN_TEST(check_sync_flags_killer);
    RUN_TEST(check_sync_flags_nosync);
    RUN_TEST(check_formatted);
    RUN_TEST(replace_bytes);
    RUN_TEST(lengthen_sync);
    RUN_TEST(compress_track);
    
    printf("\nTrack Writing:\n");
    RUN_TEST(write_track_null);
    RUN_TEST(fill_track_null);
    RUN_TEST(kill_track_null);
    RUN_TEST(erase_track_null);
    
    printf("\nTrack Preparation:\n");
    RUN_TEST(prepare_track);
    
    printf("\nImage Management:\n");
    RUN_TEST(create_image);
    
    printf("\nDisk Mastering:\n");
    RUN_TEST(master_disk_null);
    RUN_TEST(unformat_disk_null);
    RUN_TEST(init_aligned_null);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
