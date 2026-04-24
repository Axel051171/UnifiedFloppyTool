/**
 * @file test_nib_plugin.c
 * @brief NIB (Apple II Nibble) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/nib/uft_nib.c::nib_probe.
 *
 * Format: Raw nibble dump, 35 tracks × 6656 bytes = 232960 bytes.
 * No header magic — probe is size + heuristic scan for Apple II
 * GCR markers (FF sync runs, D5 AA 96 address fields).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define NIB_TRACKS        35
#define NIB_TRACK_SIZE    6656
#define NIB_FILE_SIZE     (NIB_TRACKS * NIB_TRACK_SIZE)     /* 232960 */

/* Returns confidence 0..100. Mirrors nib_probe in uft_nib.c. */
static int nib_probe_replica(const uint8_t *data, size_t size,
                               size_t file_size) {
    if (file_size != (size_t)NIB_FILE_SIZE) return 0;
    int confidence = 80;

    if (size >= NIB_TRACK_SIZE) {
        int ff_count = 0, d5_count = 0;
        for (size_t i = 0; i < (size_t)NIB_TRACK_SIZE; i++) {
            if (data[i] == 0xFF) ff_count++;
            if (i + 2 < (size_t)NIB_TRACK_SIZE &&
                data[i] == 0xD5 && data[i+1] == 0xAA && data[i+2] == 0x96) {
                d5_count++;
            }
        }
        if (d5_count >= 10 && ff_count > 500) confidence = 95;
        else if (ff_count > 200) confidence = 88;
    }
    return confidence;
}

TEST(probe_wrong_size) {
    uint8_t buf[256] = {0};
    ASSERT(nib_probe_replica(buf, sizeof(buf), 232959u) == 0);
    ASSERT(nib_probe_replica(buf, sizeof(buf), 232961u) == 0);
    ASSERT(nib_probe_replica(buf, sizeof(buf), 0) == 0);
}

TEST(probe_correct_size_empty_track_base_confidence) {
    /* Size matches but first track has no GCR markers → base confidence. */
    static uint8_t buf[NIB_TRACK_SIZE];
    memset(buf, 0x00, sizeof(buf));
    int c = nib_probe_replica(buf, sizeof(buf), NIB_FILE_SIZE);
    ASSERT(c == 80);
}

TEST(probe_rich_in_ff_sync) {
    /* File that's mostly FF sync → 88 confidence (no D5 AA 96). */
    static uint8_t buf[NIB_TRACK_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    int c = nib_probe_replica(buf, sizeof(buf), NIB_FILE_SIZE);
    ASSERT(c == 88);
}

TEST(probe_rich_ff_and_d5aa96) {
    /* Fill with 0xFF and inject 10+ D5 AA 96 address fields →
     * highest confidence (95). */
    static uint8_t buf[NIB_TRACK_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    /* Place 16 address fields spaced 400 bytes apart */
    for (int i = 0; i < 16; i++) {
        size_t off = (size_t)(i * 400);
        if (off + 3 <= (size_t)NIB_TRACK_SIZE) {
            buf[off]     = 0xD5;
            buf[off + 1] = 0xAA;
            buf[off + 2] = 0x96;
        }
    }
    int c = nib_probe_replica(buf, sizeof(buf), NIB_FILE_SIZE);
    ASSERT(c == 95);
}

TEST(probe_few_sync_bytes_stays_at_base) {
    static uint8_t buf[NIB_TRACK_SIZE];
    memset(buf, 0x00, sizeof(buf));
    /* Place only 50 FF bytes — below the 200 threshold. */
    for (int i = 0; i < 50; i++) buf[i * 10] = 0xFF;
    int c = nib_probe_replica(buf, sizeof(buf), NIB_FILE_SIZE);
    ASSERT(c == 80);
}

TEST(probe_many_address_but_no_sync) {
    /* Rare edge case: many D5 AA 96 markers but few FF sync bytes
     * (synthetic file). Source requires BOTH ff_count > 500 AND
     * d5_count >= 10 for the 95 tier, so this falls to 88 or 80. */
    static uint8_t buf[NIB_TRACK_SIZE];
    memset(buf, 0x00, sizeof(buf));
    /* 16 address fields — enough for d5_count >= 10 */
    for (int i = 0; i < 16; i++) {
        size_t off = (size_t)(i * 400);
        if (off + 3 <= (size_t)NIB_TRACK_SIZE) {
            buf[off]     = 0xD5;
            buf[off + 1] = 0xAA;
            buf[off + 2] = 0x96;
        }
    }
    /* FF_count will be 0 — below 200 → base confidence 80 */
    int c = nib_probe_replica(buf, sizeof(buf), NIB_FILE_SIZE);
    ASSERT(c == 80);
}

int main(void) {
    printf("=== NIB plugin probe tests ===\n");
    RUN(probe_wrong_size);
    RUN(probe_correct_size_empty_track_base_confidence);
    RUN(probe_rich_in_ff_sync);
    RUN(probe_rich_ff_and_d5aa96);
    RUN(probe_few_sync_bytes_stays_at_base);
    RUN(probe_many_address_but_no_sync);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
