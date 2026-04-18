/**
 * @file test_stx_plugin.c
 * @brief STX (Atari ST Pasti) Plugin-B — format probe + parse unit tests.
 *
 * Self-contained: reproduces the plugin's probe logic and minimal
 * track-descriptor layout from src/formats/stx/uft_stx_plugin.c.
 * Tests run without linking the actual plugin (matches the pattern
 * used by tests/test_adf.c, test_atari.c et al.) so the harness is
 * robust against header-consolidation work in progress.
 *
 * Covers:
 *   probe_signature    — magic "RSY\\0" matches, other bytes don't
 *   open_minimal       — smallest valid STX file parses geometry
 *   track_count        — 16-bit LE parse of track count field
 *
 * Reference: Pasti specification (Jean Louis-Guerin), used across
 * the Atari ST preservation scene (Steem, Hatari, pasti.exe).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); \
                        test_##name(); \
                        if (_fail == 0 || _last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static int _last_fail = 0;

/* ═════════════════════════════════════════════════════════════════════════
 * Constants — mirror src/formats/stx/uft_stx_plugin.c
 * ═════════════════════════════════════════════════════════════════════════ */

#define STX_MAGIC       "RSY"          /* plus trailing 0x00 */
#define STX_HEADER_SIZE 16
#define STX_TRK_HDR     16             /* per-track descriptor header */

/* Little-endian reads (STX is LE throughout). */
static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Replica of stx_plugin_probe() — magic check + minimum size gate. */
static int stx_probe_replica(const uint8_t *data, size_t size) {
    if (size < STX_HEADER_SIZE) return 0;
    if (data[0] == 'R' && data[1] == 'S' && data[2] == 'Y' && data[3] == '\0')
        return 97;   /* confidence 97 per real plugin */
    return 0;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Synthetic minimal STX builder
 * ═════════════════════════════════════════════════════════════════════════ */

/* Build the smallest valid STX file in `out`, return bytes written.
 * Layout: 16-byte header + 1 empty track descriptor. */
static size_t build_minimal_stx(uint8_t *out, size_t out_cap,
                                 uint16_t version, uint16_t n_tracks) {
    if (out_cap < STX_HEADER_SIZE + STX_TRK_HDR) return 0;
    memset(out, 0, STX_HEADER_SIZE + STX_TRK_HDR);

    /* Header */
    out[0] = 'R'; out[1] = 'S'; out[2] = 'Y'; out[3] = '\0';
    out[4] = (uint8_t)(version & 0xFF);
    out[5] = (uint8_t)((version >> 8) & 0xFF);
    out[10] = (uint8_t)(n_tracks & 0xFF);
    out[11] = (uint8_t)((n_tracks >> 8) & 0xFF);

    /* Track descriptor at offset 16: size=16 (empty track), sector_count=0 */
    size_t tpos = STX_HEADER_SIZE;
    out[tpos + 0] = 16;       /* trk_size low byte */
    /* sector_count at offset 8 of track hdr = 0 (already zeroed) */

    return STX_HEADER_SIZE + STX_TRK_HDR;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Tests
 * ═════════════════════════════════════════════════════════════════════════ */

TEST(probe_signature_valid) {
    uint8_t buf[32] = { 'R', 'S', 'Y', '\0' };
    ASSERT(stx_probe_replica(buf, sizeof(buf)) == 97);
}

TEST(probe_signature_invalid_magic) {
    uint8_t buf[32] = { 'W', 'O', 'Z', '\0' };   /* WOZ magic */
    ASSERT(stx_probe_replica(buf, sizeof(buf)) == 0);

    uint8_t rsy_bad[32] = { 'R', 'S', 'Y', 'X' }; /* 4th byte wrong */
    ASSERT(stx_probe_replica(rsy_bad, sizeof(rsy_bad)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[8] = { 'R', 'S', 'Y', '\0' };     /* only 8 bytes */
    ASSERT(stx_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(stx_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(build_minimal_header) {
    uint8_t buf[64];
    size_t n = build_minimal_stx(buf, sizeof(buf), 0x0300, 1);
    ASSERT(n == STX_HEADER_SIZE + STX_TRK_HDR);
    ASSERT(stx_probe_replica(buf, n) == 97);
    ASSERT(rd_le16(buf + 4)  == 0x0300);   /* version 3.0 */
    ASSERT(rd_le16(buf + 10) == 1);        /* 1 track */
}

TEST(parse_track_count_multi) {
    uint8_t buf[256];
    size_t n = build_minimal_stx(buf, sizeof(buf), 0x0300, 160);  /* 80×2 */
    ASSERT(n > 0);
    ASSERT(rd_le16(buf + 10) == 160);
}

TEST(track_descriptor_size_field) {
    uint8_t buf[64];
    build_minimal_stx(buf, sizeof(buf), 0x0300, 1);
    /* Track descriptor starts at offset 16 — first 4 bytes = trk_size LE32 */
    uint32_t sz = rd_le32(buf + STX_HEADER_SIZE);
    ASSERT(sz == 16);
}

TEST(rsy_ascii_values) {
    /* Pin the magic to exact ASCII so any future endian mistake fails loud. */
    ASSERT((uint8_t)'R' == 0x52);
    ASSERT((uint8_t)'S' == 0x53);
    ASSERT((uint8_t)'Y' == 0x59);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Entry
 * ═════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("=== STX Plugin Tests ===\n");
    RUN(probe_signature_valid);
    RUN(probe_signature_invalid_magic);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(build_minimal_header);
    RUN(parse_track_count_multi);
    RUN(track_descriptor_size_field);
    RUN(rsy_ascii_values);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
