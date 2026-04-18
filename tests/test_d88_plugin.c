/**
 * @file test_d88_plugin.c
 * @brief D88 (NEC PC-88/98) Plugin-B — probe + track-offset table tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/d88/uft_d88.c probe logic.
 *
 * Format: no magic bytes — probe keys off the {disk_size@0x1C,
 * media_type@0x1B} fields. 0x2B0-byte header + 164-entry track offset
 * table at 0x20.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ═════════════════════════════════════════════════════════════════════════
 * Constants — mirror src/formats/d88/uft_d88.c
 * ═════════════════════════════════════════════════════════════════════════ */

#define D88_HEADER_SIZE  0x2B0   /* 688 bytes */
#define D88_MEDIA_2D     0x00    /* 2D: 5.25" DD, 40-tracks */
#define D88_MEDIA_2DD    0x10    /* 2DD: 3.5"/5.25" DD, 80-tracks */
#define D88_MEDIA_2HD    0x20    /* 2HD: 3.5"/5.25" HD, 77/80-tracks */

static uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Replica of d88_probe() — D88 has no magic bytes, so probe only
 * sanity-checks the media type byte + disk size field. */
static int d88_probe_replica(const uint8_t *data, size_t size, size_t file_size) {
    if (size < D88_HEADER_SIZE) return 0;
    uint32_t disk_size = rd_le32(data + 0x1C);
    uint8_t  media     = data[0x1B];
    if (disk_size <= file_size &&
        (media == D88_MEDIA_2D || media == D88_MEDIA_2DD || media == D88_MEDIA_2HD))
        return 90;
    return 0;
}

/* Build a minimal valid D88 file (header-only, no track data). */
static size_t build_minimal_d88(uint8_t *out, size_t out_cap, uint8_t media,
                                 uint32_t disk_size) {
    if (out_cap < D88_HEADER_SIZE) return 0;
    memset(out, 0, D88_HEADER_SIZE);
    /* bytes 0x00..0x16: disk name (ASCII). Leave empty. */
    /* 0x1A: write_protected flag (0=no, 0x10=yes) */
    out[0x1B] = media;
    wr_le32(out + 0x1C, disk_size);
    /* 0x20..0x2AF: 164 × 4-byte track offsets (all zero = no tracks). */
    return D88_HEADER_SIZE;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Tests
 * ═════════════════════════════════════════════════════════════════════════ */

TEST(probe_valid_2d) {
    uint8_t buf[D88_HEADER_SIZE];
    size_t n = build_minimal_d88(buf, sizeof(buf), D88_MEDIA_2D, D88_HEADER_SIZE);
    ASSERT(n == D88_HEADER_SIZE);
    ASSERT(d88_probe_replica(buf, n, n) == 90);
}

TEST(probe_valid_2dd) {
    uint8_t buf[D88_HEADER_SIZE];
    build_minimal_d88(buf, sizeof(buf), D88_MEDIA_2DD, D88_HEADER_SIZE);
    ASSERT(d88_probe_replica(buf, sizeof(buf), sizeof(buf)) == 90);
}

TEST(probe_valid_2hd) {
    uint8_t buf[D88_HEADER_SIZE];
    build_minimal_d88(buf, sizeof(buf), D88_MEDIA_2HD, D88_HEADER_SIZE);
    ASSERT(d88_probe_replica(buf, sizeof(buf), sizeof(buf)) == 90);
}

TEST(probe_invalid_media) {
    uint8_t buf[D88_HEADER_SIZE];
    build_minimal_d88(buf, sizeof(buf), 0x30, D88_HEADER_SIZE);  /* invalid media */
    ASSERT(d88_probe_replica(buf, sizeof(buf), sizeof(buf)) == 0);

    build_minimal_d88(buf, sizeof(buf), 0xFF, D88_HEADER_SIZE);
    ASSERT(d88_probe_replica(buf, sizeof(buf), sizeof(buf)) == 0);
}

TEST(probe_disk_size_exceeds_file) {
    uint8_t buf[D88_HEADER_SIZE];
    /* Claim disk_size = 1MB, but file only 688 bytes → reject. */
    build_minimal_d88(buf, sizeof(buf), D88_MEDIA_2D, 1024 * 1024);
    ASSERT(d88_probe_replica(buf, sizeof(buf), sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    ASSERT(d88_probe_replica(buf, sizeof(buf), sizeof(buf)) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(d88_probe_replica((const uint8_t *)"", 0, 0) == 0);
}

TEST(track_offset_table_parse) {
    uint8_t buf[D88_HEADER_SIZE];
    build_minimal_d88(buf, sizeof(buf), D88_MEDIA_2HD, D88_HEADER_SIZE + 4096);

    /* Populate track 0 offset at 0x20 and track 1 at 0x24 */
    wr_le32(buf + 0x20, 0x2B0);        /* track 0 starts right after header */
    wr_le32(buf + 0x24, 0x2B0 + 2048); /* track 1 at 2KB later */

    uint32_t off0 = rd_le32(buf + 0x20);
    uint32_t off1 = rd_le32(buf + 0x24);
    ASSERT(off0 == 0x2B0);
    ASSERT(off1 == 0x2B0 + 2048);
}

TEST(media_constants_fixed) {
    /* Pin the three canonical media bytes — future readers shouldn't be
     * tempted to "clean up" by renumbering. */
    ASSERT(D88_MEDIA_2D  == 0x00);
    ASSERT(D88_MEDIA_2DD == 0x10);
    ASSERT(D88_MEDIA_2HD == 0x20);
}

int main(void) {
    printf("=== D88 Plugin Tests ===\n");
    RUN(probe_valid_2d);
    RUN(probe_valid_2dd);
    RUN(probe_valid_2hd);
    RUN(probe_invalid_media);
    RUN(probe_disk_size_exceeds_file);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(track_offset_table_parse);
    RUN(media_constants_fixed);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
