/**
 * @file test_atx_plugin.c
 * @brief ATX (VAPI Atari 8-bit) Plugin — probe tests.
 *
 * Self-contained per template from test_atr_plugin.c.
 * Mirrors src/formats/atx/uft_atx.c::atx_plugin_probe AFTER the
 * byte-order fix for ATX_SIGNATURE (see KNOWN_ISSUES.md §M.-1).
 *
 * Format: 48-byte header. First 4 bytes are ASCII "AT8X" which a
 * little-endian read interprets as 0x58385441. ATX is the VAPI
 * (Vinculatum Atari Preservation Initiative) format with per-sector
 * timing and weak-bit annotations.
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

#define ATX_SIGNATURE     0x58385441u    /* "AT8X" as LE32 */
#define ATX_HEADER_SIZE   48

static uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int atx_probe_replica(const uint8_t *data, size_t size) {
    if (size < ATX_HEADER_SIZE) return 0;
    if (rd_le32(data) != ATX_SIGNATURE) return 0;
    return 98;
}

static void build_minimal_atx(uint8_t *out) {
    memset(out, 0, ATX_HEADER_SIZE);
    /* ASCII "AT8X" in file order (bytes 0..3) */
    out[0] = 'A';
    out[1] = 'T';
    out[2] = '8';
    out[3] = 'X';
}

TEST(probe_magic_valid) {
    uint8_t buf[ATX_HEADER_SIZE];
    build_minimal_atx(buf);
    ASSERT(atx_probe_replica(buf, sizeof(buf)) == 98);
}

TEST(probe_signature_constant_matches_le32_read) {
    /* Verify that the LE32 read of the bytes "AT8X" equals the
     * fixed signature constant — this is the load-bearing fix from
     * KNOWN_ISSUES §M.-1. */
    uint8_t buf[4] = { 'A', 'T', '8', 'X' };
    ASSERT(rd_le32(buf) == ATX_SIGNATURE);
}

TEST(probe_old_buggy_constant_no_longer_matches) {
    /* Before the fix, ATX_SIGNATURE was 0x41543858 (the BE
     * interpretation). A conformant "AT8X" file must NOT match that
     * value under an LE read. Regression check. */
    uint8_t buf[4] = { 'A', 'T', '8', 'X' };
    ASSERT(rd_le32(buf) != 0x41543858u);
}

TEST(probe_wrong_magic) {
    uint8_t buf[ATX_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "ATX2", 4);
    ASSERT(atx_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_zero_header) {
    uint8_t buf[ATX_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    ASSERT(atx_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[ATX_HEADER_SIZE - 1];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'A'; buf[1] = 'T'; buf[2] = '8'; buf[3] = 'X';
    ASSERT(atx_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_atr_is_not_atx) {
    /* ATR magic is 0x0296 LE16 — must not be confused with ATX. */
    uint8_t buf[ATX_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x96;
    buf[1] = 0x02;
    ASSERT(atx_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_byte_reversed_magic_rejected) {
    /* "X8TA" in file order — must NOT match even though it's a
     * byte-permutation of the correct magic. */
    uint8_t buf[ATX_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'X'; buf[1] = '8'; buf[2] = 'T'; buf[3] = 'A';
    ASSERT(atx_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== ATX plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_signature_constant_matches_le32_read);
    RUN(probe_old_buggy_constant_no_longer_matches);
    RUN(probe_wrong_magic);
    RUN(probe_zero_header);
    RUN(probe_too_small);
    RUN(probe_atr_is_not_atx);
    RUN(probe_byte_reversed_magic_rejected);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
