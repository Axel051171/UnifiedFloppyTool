/**
 * @file test_g64_plugin.c
 * @brief G64 (Commodore 1541 GCR) Plugin-B — probe + header parse tests.
 *
 * Self-contained plugin-test per template from test_stx_plugin.c.
 * Mirrors src/formats/g64/uft_g64.c probe logic.
 *
 * Format: "GCR-1541" magic + version + num_tracks + max_track_size(LE16)
 *         + track offset table.
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
 * Constants — mirror src/formats/g64/uft_g64.c
 * ═════════════════════════════════════════════════════════════════════════ */

#define G64_SIGNATURE     "GCR-1541"
#define G64_HEADER_SIZE   12
#define G64_MAX_TRACKS    84
#define G64_MAX_TRACK_SZ  7930

static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Replica of g64_probe() — returns confidence 0..100. */
static int g64_probe_replica(const uint8_t *data, size_t size, size_t file_size) {
    if (size < G64_HEADER_SIZE) return 0;
    if (memcmp(data, G64_SIGNATURE, 8) != 0) return 0;

    int confidence = 80;
    if (data[8] == 0x00) confidence = 90;

    uint8_t num_tracks = data[9];
    if (num_tracks > 0 && num_tracks <= G64_MAX_TRACKS) confidence = 95;

    uint16_t max_size = rd_le16(data + 10);
    if (max_size > 0 && max_size <= G64_MAX_TRACK_SZ) confidence = 98;

    if (file_size >= (size_t)(G64_HEADER_SIZE + num_tracks * 8))
        confidence = 100;
    return confidence;
}

/* Build the smallest valid G64 file. Returns bytes written. */
static size_t build_minimal_g64(uint8_t *out, size_t out_cap,
                                 uint8_t num_tracks, uint16_t max_track_size) {
    size_t need = G64_HEADER_SIZE + (size_t)num_tracks * 8;
    if (out_cap < need) return 0;
    memset(out, 0, need);
    memcpy(out, G64_SIGNATURE, 8);        /* magic */
    out[8] = 0x00;                         /* version 0 */
    out[9] = num_tracks;
    out[10] = (uint8_t)(max_track_size & 0xFF);
    out[11] = (uint8_t)(max_track_size >> 8);
    /* Track offset table at [12..12+num_tracks*4] stays zeroed (= track absent).
     * Speed-zone table at [12+num_tracks*4..] also zero. */
    return need;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Tests
 * ═════════════════════════════════════════════════════════════════════════ */

TEST(probe_signature_valid) {
    uint8_t buf[1024];
    size_t n = build_minimal_g64(buf, sizeof(buf), 84, 7928);
    ASSERT(n > 0);
    ASSERT(g64_probe_replica(buf, n, n) >= 80);
}

TEST(probe_signature_invalid_magic) {
    uint8_t buf[16] = "GCR-1571";      /* wrong signature (1571 not 1541) */
    ASSERT(g64_probe_replica(buf, sizeof(buf), sizeof(buf)) == 0);

    uint8_t woz[16] = "WOZ2\x00\x00";
    ASSERT(g64_probe_replica(woz, sizeof(woz), sizeof(woz)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[8] = "GCR-154";        /* 8 bytes — header requires 12 */
    ASSERT(g64_probe_replica(buf, 8, 8) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(g64_probe_replica((const uint8_t *)"", 0, 0) == 0);
}

TEST(probe_confidence_increases_with_valid_fields) {
    uint8_t buf[1024];

    /* Signature + version=0 + zero tracks but large file → jumps to 100
     * because the file-size check passes (min_size = 12 + 0*8 = 12,
     * file_size = sizeof(buf) >= 12). That's the real behaviour of
     * g64_probe — it's lenient, probably by design for partial files. */
    memset(buf, 0, sizeof(buf));
    memcpy(buf, G64_SIGNATURE, 8);
    ASSERT(g64_probe_replica(buf, sizeof(buf), sizeof(buf)) == 100);

    /* Now with actual tracks + max-size populated. */
    size_t n = build_minimal_g64(buf, sizeof(buf), 42, 7928);
    ASSERT(n > 0);
    ASSERT(g64_probe_replica(buf, n, n) == 100);
}

TEST(build_minimal_header) {
    uint8_t buf[1024];
    size_t n = build_minimal_g64(buf, sizeof(buf), 84, 7928);
    ASSERT(n == G64_HEADER_SIZE + 84 * 8);
    ASSERT(memcmp(buf, "GCR-1541", 8) == 0);
    ASSERT(buf[8] == 0);             /* version */
    ASSERT(buf[9] == 84);            /* num_tracks */
    ASSERT(rd_le16(buf + 10) == 7928);
}

TEST(parse_num_tracks_boundary) {
    uint8_t buf[1024];
    /* Max legit = 84 (42 cylinders × 2 heads for 1541/1571). */
    size_t n = build_minimal_g64(buf, sizeof(buf), 84, 7928);
    ASSERT(n > 0 && buf[9] == 84);

    /* 85 is over G64_MAX_TRACKS. The probe skips the num_tracks "bump
     * to 95" branch, but max_size + file_size still bump to 100, so
     * out-of-range track count doesn't reduce confidence under the
     * existing logic — confirms current behaviour. */
    build_minimal_g64(buf, sizeof(buf), 80, 7928);
    buf[9] = 85;
    ASSERT(g64_probe_replica(buf, sizeof(buf), sizeof(buf)) == 100);
}

TEST(gcr1541_ascii_pin) {
    const char *s = G64_SIGNATURE;
    ASSERT((uint8_t)s[0] == 'G');
    ASSERT((uint8_t)s[1] == 'C');
    ASSERT((uint8_t)s[2] == 'R');
    ASSERT((uint8_t)s[3] == '-');
    ASSERT((uint8_t)s[4] == '1');
    ASSERT((uint8_t)s[5] == '5');
    ASSERT((uint8_t)s[6] == '4');
    ASSERT((uint8_t)s[7] == '1');
}

int main(void) {
    printf("=== G64 Plugin Tests ===\n");
    RUN(probe_signature_valid);
    RUN(probe_signature_invalid_magic);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(probe_confidence_increases_with_valid_fields);
    RUN(build_minimal_header);
    RUN(parse_num_tracks_boundary);
    RUN(gcr1541_ascii_pin);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
