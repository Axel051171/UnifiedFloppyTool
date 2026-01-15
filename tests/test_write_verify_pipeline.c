/**
 * @file test_write_verify_pipeline.c
 * @brief Unit tests for Write-Verify Pipeline
 * 
 * Tests cover:
 * - Pipeline creation and destruction
 * - Track write with verification
 * - File verification
 * - Image comparison
 * - Statistics tracking
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)

/*============================================================================
 * EXTERNAL DECLARATIONS (from uft_write_verify_pipeline.c)
 *============================================================================*/

typedef enum {
    UFT_WVP_OK              = 0,
    UFT_WVP_ERR_PARAM       = -1,
    UFT_WVP_ERR_MEMORY      = -2,
    UFT_WVP_ERR_IO          = -3,
    UFT_WVP_ERR_VERIFY      = -4,
    UFT_WVP_ERR_ABORTED     = -5
} uft_wvp_error_t;

typedef enum {
    UFT_WVP_PHASE_IDLE      = 0,
    UFT_WVP_PHASE_HASHING   = 1,
    UFT_WVP_PHASE_WRITING   = 2,
    UFT_WVP_PHASE_READING   = 3,
    UFT_WVP_PHASE_VERIFYING = 4,
    UFT_WVP_PHASE_COMPLETE  = 5
} uft_wvp_phase_t;

typedef struct {
    int max_tracks;
    bool double_sided;
    bool verify_after_write;
    bool stop_on_error;
    int retry_count;
} uft_wvp_config_t;

typedef struct {
    int sector_id;
    int offset;
    int size;
    const uint8_t *data;
} uft_wvp_sector_info_t;

typedef struct {
    bool success;
    uft_wvp_error_t error_code;
    int track;
    int head;
    uint32_t expected_crc;
    uint32_t actual_crc;
    int bad_sector_count;
    int bad_sectors[16];
    char message[256];
} uft_wvp_result_t;

typedef struct {
    int tracks_written;
    int tracks_verified;
    int tracks_failed;
    int sectors_written;
    int sectors_failed;
    size_t bytes_written;
    size_t bytes_verified;
} uft_wvp_stats_t;

typedef struct uft_wvp_ctx uft_wvp_ctx_t;

/* External function declarations */
extern uft_wvp_ctx_t* uft_wvp_create(const uft_wvp_config_t *config);
extern void uft_wvp_destroy(uft_wvp_ctx_t *ctx);
extern void uft_wvp_reset(uft_wvp_ctx_t *ctx);
extern void uft_wvp_get_stats(const uft_wvp_ctx_t *ctx, uft_wvp_stats_t *stats);
extern uft_wvp_result_t uft_wvp_write_track(uft_wvp_ctx_t *ctx,
                                             int track, int head,
                                             const uint8_t *data, size_t len,
                                             const uft_wvp_sector_info_t *sectors,
                                             int sector_count);
extern int uft_wvp_verify_image_file(const char *path, uft_wvp_result_t *result);
extern int uft_wvp_compare_images(const char *path1, const char *path2,
                                   uft_wvp_result_t *result);

/*============================================================================
 * TEST HELPERS
 *============================================================================*/

static const char *test_dir = "/tmp/uft_wvp_test";

static void setup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", test_dir);
    system(cmd);
}

static void cleanup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
}

static char* create_test_file(const char *name, const uint8_t *data, size_t size) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/%s", test_dir, name);
    
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    
    if (data && size > 0) {
        fwrite(data, 1, size, f);
    }
    
    fclose(f);
    return path;
}

/*============================================================================
 * TESTS: CONTEXT MANAGEMENT
 *============================================================================*/

TEST(wvp_create_destroy) {
    uft_wvp_config_t config = {
        .max_tracks = 80,
        .double_sided = true,
        .verify_after_write = true,
        .stop_on_error = false,
        .retry_count = 3
    };
    
    uft_wvp_ctx_t *ctx = uft_wvp_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    uft_wvp_destroy(ctx);
}

TEST(wvp_create_null_config) {
    uft_wvp_ctx_t *ctx = uft_wvp_create(NULL);
    ASSERT_NULL(ctx);
}

TEST(wvp_reset) {
    uft_wvp_config_t config = {
        .max_tracks = 40,
        .double_sided = false,
        .verify_after_write = true,
        .stop_on_error = false,
        .retry_count = 1
    };
    
    uft_wvp_ctx_t *ctx = uft_wvp_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Write a track to update stats */
    uint8_t track_data[4608];
    memset(track_data, 0xE5, sizeof(track_data));
    
    uft_wvp_result_t result = uft_wvp_write_track(ctx, 0, 0, 
                                                   track_data, sizeof(track_data),
                                                   NULL, 0);
    ASSERT_TRUE(result.success);
    
    /* Check stats before reset */
    uft_wvp_stats_t stats;
    uft_wvp_get_stats(ctx, &stats);
    ASSERT_EQ(stats.tracks_written, 1);
    
    /* Reset and verify */
    uft_wvp_reset(ctx);
    uft_wvp_get_stats(ctx, &stats);
    ASSERT_EQ(stats.tracks_written, 0);
    
    uft_wvp_destroy(ctx);
}

/*============================================================================
 * TESTS: TRACK WRITE WITH VERIFICATION
 *============================================================================*/

TEST(wvp_write_track_simple) {
    uft_wvp_config_t config = {
        .max_tracks = 80,
        .double_sided = true,
        .verify_after_write = true,
        .stop_on_error = false,
        .retry_count = 3
    };
    
    uft_wvp_ctx_t *ctx = uft_wvp_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Create track data */
    uint8_t track_data[6250];
    for (int i = 0; i < (int)sizeof(track_data); i++) {
        track_data[i] = i & 0xFF;
    }
    
    uft_wvp_result_t result = uft_wvp_write_track(ctx, 0, 0,
                                                   track_data, sizeof(track_data),
                                                   NULL, 0);
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.error_code, UFT_WVP_OK);
    ASSERT_EQ(result.track, 0);
    ASSERT_EQ(result.head, 0);
    
    /* Check CRC was calculated */
    ASSERT_TRUE(result.expected_crc != 0);
    ASSERT_EQ(result.expected_crc, result.actual_crc);
    
    uft_wvp_destroy(ctx);
}

TEST(wvp_write_track_with_sectors) {
    uft_wvp_config_t config = {
        .max_tracks = 80,
        .double_sided = true,
        .verify_after_write = true,
        .stop_on_error = false,
        .retry_count = 1
    };
    
    uft_wvp_ctx_t *ctx = uft_wvp_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Create sector data */
    uint8_t sector1[512], sector2[512];
    memset(sector1, 0xAA, sizeof(sector1));
    memset(sector2, 0x55, sizeof(sector2));
    
    /* Create track with 2 sectors */
    uint8_t track_data[2048];
    memset(track_data, 0x4E, sizeof(track_data));  /* Gap fill */
    memcpy(&track_data[100], sector1, sizeof(sector1));
    memcpy(&track_data[700], sector2, sizeof(sector2));
    
    uft_wvp_sector_info_t sectors[2] = {
        { .sector_id = 1, .offset = 100, .size = 512, .data = sector1 },
        { .sector_id = 2, .offset = 700, .size = 512, .data = sector2 }
    };
    
    uft_wvp_result_t result = uft_wvp_write_track(ctx, 5, 1,
                                                   track_data, sizeof(track_data),
                                                   sectors, 2);
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.track, 5);
    ASSERT_EQ(result.head, 1);
    
    /* Check statistics */
    uft_wvp_stats_t stats;
    uft_wvp_get_stats(ctx, &stats);
    ASSERT_EQ(stats.tracks_written, 1);
    ASSERT_EQ(stats.sectors_written, 2);
    
    uft_wvp_destroy(ctx);
}

TEST(wvp_write_invalid_params) {
    uft_wvp_config_t config = {
        .max_tracks = 80,
        .double_sided = true,
        .verify_after_write = true,
        .stop_on_error = false,
        .retry_count = 1
    };
    
    uft_wvp_ctx_t *ctx = uft_wvp_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* NULL data */
    uft_wvp_result_t result = uft_wvp_write_track(ctx, 0, 0, NULL, 100, NULL, 0);
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.error_code, UFT_WVP_ERR_PARAM);
    
    /* Zero length */
    uint8_t data[10];
    result = uft_wvp_write_track(ctx, 0, 0, data, 0, NULL, 0);
    ASSERT_FALSE(result.success);
    
    /* NULL context */
    result = uft_wvp_write_track(NULL, 0, 0, data, sizeof(data), NULL, 0);
    ASSERT_FALSE(result.success);
    
    uft_wvp_destroy(ctx);
}

/*============================================================================
 * TESTS: FILE VERIFICATION
 *============================================================================*/

TEST(wvp_verify_file) {
    /* Create test file */
    uint8_t data[1024];
    for (int i = 0; i < (int)sizeof(data); i++) {
        data[i] = i & 0xFF;
    }
    
    char *path = create_test_file("test_verify.bin", data, sizeof(data));
    ASSERT_NOT_NULL(path);
    
    uft_wvp_result_t result;
    int ret = uft_wvp_verify_image_file(path, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.expected_crc != 0);
}

TEST(wvp_verify_nonexistent) {
    uft_wvp_result_t result;
    int ret = uft_wvp_verify_image_file("/nonexistent/file.bin", &result);
    
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.error_code, UFT_WVP_ERR_IO);
}

/*============================================================================
 * TESTS: IMAGE COMPARISON
 *============================================================================*/

TEST(wvp_compare_identical) {
    uint8_t data[512];
    memset(data, 0x42, sizeof(data));
    
    char *path1 = create_test_file("compare1.bin", data, sizeof(data));
    ASSERT_NOT_NULL(path1);
    
    /* Need separate path buffer for second file */
    char path2[512];
    snprintf(path2, sizeof(path2), "%s/compare2.bin", test_dir);
    FILE *f = fopen(path2, "wb");
    ASSERT(f != NULL);
    fwrite(data, 1, sizeof(data), f);
    fclose(f);
    
    uft_wvp_result_t result;
    int ret = uft_wvp_compare_images(path1, path2, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.expected_crc, result.actual_crc);
}

TEST(wvp_compare_different) {
    uint8_t data1[512], data2[512];
    memset(data1, 0xAA, sizeof(data1));
    memset(data2, 0x55, sizeof(data2));
    
    char *path1 = create_test_file("diff1.bin", data1, sizeof(data1));
    ASSERT_NOT_NULL(path1);
    
    char path2[512];
    snprintf(path2, sizeof(path2), "%s/diff2.bin", test_dir);
    FILE *f = fopen(path2, "wb");
    ASSERT(f != NULL);
    fwrite(data2, 1, sizeof(data2), f);
    fclose(f);
    
    uft_wvp_result_t result;
    int ret = uft_wvp_compare_images(path1, path2, &result);
    
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.error_code, UFT_WVP_ERR_VERIFY);
    ASSERT_TRUE(result.expected_crc != result.actual_crc);
}

/*============================================================================
 * TESTS: STATISTICS
 *============================================================================*/

TEST(wvp_statistics_tracking) {
    uft_wvp_config_t config = {
        .max_tracks = 80,
        .double_sided = true,
        .verify_after_write = true,
        .stop_on_error = false,
        .retry_count = 1
    };
    
    uft_wvp_ctx_t *ctx = uft_wvp_create(&config);
    ASSERT_NOT_NULL(ctx);
    
    /* Write multiple tracks */
    uint8_t track_data[4096];
    memset(track_data, 0xE5, sizeof(track_data));
    
    for (int t = 0; t < 5; t++) {
        uft_wvp_result_t result = uft_wvp_write_track(ctx, t, 0,
                                                       track_data, sizeof(track_data),
                                                       NULL, 0);
        ASSERT_TRUE(result.success);
    }
    
    uft_wvp_stats_t stats;
    uft_wvp_get_stats(ctx, &stats);
    
    ASSERT_EQ(stats.tracks_written, 5);
    ASSERT_EQ(stats.tracks_verified, 5);
    ASSERT_EQ(stats.tracks_failed, 0);
    ASSERT_EQ(stats.bytes_written, 5 * sizeof(track_data));
    ASSERT_EQ(stats.bytes_verified, 5 * sizeof(track_data));
    
    uft_wvp_destroy(ctx);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Write-Verify Pipeline Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    setup_test_dir();
    
    /* Context management */
    printf("[SUITE] Context Management\n");
    RUN_TEST(wvp_create_destroy);
    RUN_TEST(wvp_create_null_config);
    RUN_TEST(wvp_reset);
    
    /* Track write with verification */
    printf("\n[SUITE] Track Write + Verify\n");
    RUN_TEST(wvp_write_track_simple);
    RUN_TEST(wvp_write_track_with_sectors);
    RUN_TEST(wvp_write_invalid_params);
    
    /* File verification */
    printf("\n[SUITE] File Verification\n");
    RUN_TEST(wvp_verify_file);
    RUN_TEST(wvp_verify_nonexistent);
    
    /* Image comparison */
    printf("\n[SUITE] Image Comparison\n");
    RUN_TEST(wvp_compare_identical);
    RUN_TEST(wvp_compare_different);
    
    /* Statistics */
    printf("\n[SUITE] Statistics\n");
    RUN_TEST(wvp_statistics_tracking);
    
    cleanup_test_dir();
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
