/**
 * @file test_writer_backend.c
 * @brief Unit Tests for Writer Backend (Standalone)
 * 
 * P0-002: Verify Writer Backend functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
    tests_passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL: %s != %s\n", #a, #b); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(x) ASSERT_EQ(!!(x), 1)
#define ASSERT_FALSE(x) ASSERT_EQ(!!(x), 0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mock Writer Backend
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t track_size;
    int tracks;
    int heads;
    bool is_open;
    int tracks_written;
    int sectors_written;
    size_t bytes_written;
} mock_writer_t;

static mock_writer_t* mock_create(size_t size, int tracks, int heads) {
    mock_writer_t *w = calloc(1, sizeof(mock_writer_t));
    if (!w) return NULL;
    
    w->size = size;
    w->track_size = size / (tracks * heads);
    w->tracks = tracks;
    w->heads = heads;
    
    w->buffer = calloc(1, size);
    if (!w->buffer) {
        free(w);
        return NULL;
    }
    
    return w;
}

static void mock_destroy(mock_writer_t *w) {
    if (w) {
        free(w->buffer);
        free(w);
    }
}

static int mock_open(mock_writer_t *w) {
    if (!w) return -1;
    w->is_open = true;
    return 0;
}

static void mock_close(mock_writer_t *w) {
    if (w) w->is_open = false;
}

static size_t calc_offset(mock_writer_t *w, int cyl, int head) {
    return ((size_t)head * w->tracks + cyl) * w->track_size;
}

static int mock_write_track(mock_writer_t *w, int cyl, int head, 
                            const uint8_t *data, size_t size) {
    if (!w || !w->is_open || !data) return -1;
    
    size_t offset = calc_offset(w, cyl, head);
    if (offset + size > w->size) return -1;
    
    memcpy(w->buffer + offset, data, size);
    w->tracks_written++;
    w->bytes_written += size;
    return 0;
}

static int mock_write_sector(mock_writer_t *w, int cyl, int head, int sector,
                             const uint8_t *data, size_t size) {
    if (!w || !w->is_open || !data) return -1;
    
    size_t track_offset = calc_offset(w, cyl, head);
    size_t sector_offset = track_offset + (sector * size);
    
    if (sector_offset + size > w->size) return -1;
    
    memcpy(w->buffer + sector_offset, data, size);
    w->sectors_written++;
    w->bytes_written += size;
    return 0;
}

static int mock_verify_track(mock_writer_t *w, int cyl, int head,
                             const uint8_t *expected, size_t size) {
    if (!w || !w->is_open || !expected) return -1;
    
    size_t offset = calc_offset(w, cyl, head);
    if (offset + size > w->size) return -1;
    
    return memcmp(w->buffer + offset, expected, size) == 0 ? 0 : -1;
}

static int mock_read_track(mock_writer_t *w, int cyl, int head,
                           uint8_t *buffer, size_t size) {
    if (!w || !w->is_open || !buffer) return -1;
    
    size_t offset = calc_offset(w, cyl, head);
    if (offset + size > w->size) return -1;
    
    memcpy(buffer, w->buffer + offset, size);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(create_destroy) {
    mock_writer_t *w = mock_create(1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_TRUE(w->buffer != NULL);
    ASSERT_EQ(w->size, 1024 * 1024);
    ASSERT_EQ(w->tracks, 80);
    ASSERT_EQ(w->heads, 2);
    mock_destroy(w);
}

TEST(open_close) {
    mock_writer_t *w = mock_create(1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_FALSE(w->is_open);
    
    ASSERT_EQ(mock_open(w), 0);
    ASSERT_TRUE(w->is_open);
    
    mock_close(w);
    ASSERT_FALSE(w->is_open);
    
    mock_destroy(w);
}

TEST(write_single_track) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t track_data[512];
    for (int i = 0; i < 512; i++) {
        track_data[i] = i & 0xFF;
    }
    
    ASSERT_EQ(mock_write_track(w, 0, 0, track_data, 512), 0);
    ASSERT_EQ(w->tracks_written, 1);
    ASSERT_EQ(w->bytes_written, 512);
    
    /* Verify data in buffer */
    ASSERT_EQ(memcmp(w->buffer, track_data, 512), 0);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(write_multiple_tracks) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t track_data[512];
    
    for (int cyl = 0; cyl < 10; cyl++) {
        memset(track_data, cyl, 512);
        ASSERT_EQ(mock_write_track(w, cyl, 0, track_data, 512), 0);
    }
    
    ASSERT_EQ(w->tracks_written, 10);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(write_both_sides) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t data_h0[512], data_h1[512];
    memset(data_h0, 0xAA, 512);
    memset(data_h1, 0x55, 512);
    
    /* Write to both heads */
    ASSERT_EQ(mock_write_track(w, 0, 0, data_h0, 512), 0);
    ASSERT_EQ(mock_write_track(w, 0, 1, data_h1, 512), 0);
    
    /* Verify they're at different locations */
    uint8_t read_h0[512], read_h1[512];
    ASSERT_EQ(mock_read_track(w, 0, 0, read_h0, 512), 0);
    ASSERT_EQ(mock_read_track(w, 0, 1, read_h1, 512), 0);
    
    ASSERT_EQ(memcmp(read_h0, data_h0, 512), 0);
    ASSERT_EQ(memcmp(read_h1, data_h1, 512), 0);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(write_sector) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t sector_data[512];
    memset(sector_data, 0xBB, 512);
    
    ASSERT_EQ(mock_write_sector(w, 0, 0, 5, sector_data, 512), 0);
    ASSERT_EQ(w->sectors_written, 1);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(verify_track_success) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t track_data[512];
    memset(track_data, 0xCC, 512);
    
    ASSERT_EQ(mock_write_track(w, 5, 0, track_data, 512), 0);
    ASSERT_EQ(mock_verify_track(w, 5, 0, track_data, 512), 0);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(verify_track_failure) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t track_data[512], wrong_data[512];
    memset(track_data, 0xDD, 512);
    memset(wrong_data, 0xEE, 512);
    
    ASSERT_EQ(mock_write_track(w, 10, 0, track_data, 512), 0);
    ASSERT_EQ(mock_verify_track(w, 10, 0, wrong_data, 512), -1);  /* Should fail */
    
    mock_close(w);
    mock_destroy(w);
}

TEST(read_write_roundtrip) {
    mock_writer_t *w = mock_create(2 * 1024 * 1024, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t original[1024], readback[1024];
    for (int i = 0; i < 1024; i++) {
        original[i] = (i * 17) & 0xFF;
    }
    
    ASSERT_EQ(mock_write_track(w, 40, 1, original, 1024), 0);
    ASSERT_EQ(mock_read_track(w, 40, 1, readback, 1024), 0);
    ASSERT_EQ(memcmp(original, readback, 1024), 0);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(bounds_check) {
    mock_writer_t *w = mock_create(1024, 2, 1);  /* Tiny buffer */
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t data[2048];
    memset(data, 0, 2048);
    
    /* Should fail - exceeds buffer */
    ASSERT_EQ(mock_write_track(w, 0, 0, data, 2048), -1);
    
    mock_close(w);
    mock_destroy(w);
}

TEST(null_handling) {
    ASSERT_EQ(mock_open(NULL), -1);
    ASSERT_EQ(mock_write_track(NULL, 0, 0, NULL, 0), -1);
    ASSERT_EQ(mock_verify_track(NULL, 0, 0, NULL, 0), -1);
    ASSERT_EQ(mock_read_track(NULL, 0, 0, NULL, 0), -1);
    
    mock_writer_t *w = mock_create(1024, 10, 1);
    ASSERT_TRUE(w != NULL);
    /* Not opened - should fail */
    uint8_t data[512];
    ASSERT_EQ(mock_write_track(w, 0, 0, data, 512), -1);
    mock_destroy(w);
}

TEST(adf_format_size) {
    /* ADF: 80 tracks * 2 heads * 11 sectors * 512 bytes = 901120 */
    size_t adf_size = 80 * 2 * 11 * 512;
    ASSERT_EQ(adf_size, 901120);
}

TEST(d64_format_size) {
    /* D64: Variable sectors per track */
    int sectors = 0;
    for (int t = 1; t <= 17; t++) sectors += 21;
    for (int t = 18; t <= 24; t++) sectors += 19;
    for (int t = 25; t <= 30; t++) sectors += 18;
    for (int t = 31; t <= 35; t++) sectors += 17;
    size_t d64_size = sectors * 256;
    ASSERT_EQ(d64_size, 174848);
}

TEST(st_format_size) {
    /* Atari ST: 80 tracks * 2 heads * 9 sectors * 512 bytes = 737280 */
    size_t st_size = 80 * 2 * 9 * 512;
    ASSERT_EQ(st_size, 737280);
}

TEST(full_disk_write) {
    /* Simulate writing entire ADF disk */
    size_t adf_size = 901120;
    mock_writer_t *w = mock_create(adf_size, 80, 2);
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(mock_open(w), 0);
    
    uint8_t track_data[5632];  /* 11 sectors * 512 bytes */
    
    int total_tracks = 0;
    for (int cyl = 0; cyl < 80; cyl++) {
        for (int head = 0; head < 2; head++) {
            memset(track_data, (cyl << 4) | head, 5632);
            ASSERT_EQ(mock_write_track(w, cyl, head, track_data, 5632), 0);
            total_tracks++;
        }
    }
    
    ASSERT_EQ(total_tracks, 160);
    ASSERT_EQ(w->tracks_written, 160);
    ASSERT_EQ(w->bytes_written, adf_size);
    
    mock_close(w);
    mock_destroy(w);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("    Writer Backend Tests (P0-002)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN_TEST(create_destroy);
    RUN_TEST(open_close);
    RUN_TEST(write_single_track);
    RUN_TEST(write_multiple_tracks);
    RUN_TEST(write_both_sides);
    RUN_TEST(write_sector);
    RUN_TEST(verify_track_success);
    RUN_TEST(verify_track_failure);
    RUN_TEST(read_write_roundtrip);
    RUN_TEST(bounds_check);
    RUN_TEST(null_handling);
    RUN_TEST(adf_format_size);
    RUN_TEST(d64_format_size);
    RUN_TEST(st_format_size);
    RUN_TEST(full_disk_write);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("    Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return tests_failed > 0 ? 1 : 0;
}
