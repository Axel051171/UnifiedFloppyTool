/**
 * @file test_cqm_plugin.c
 * @brief CQM (CopyQM) Plugin — probe tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/cqm/uft_cqm.c probe logic.
 *
 * Format: 3-byte magic 'C', 'Q', 0x14 at offset 0-2. No fixed header
 * size (CQM uses a variable-length header followed by RLE-compressed
 * sector data).
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

#define CQM_MAGIC_0  'C'
#define CQM_MAGIC_1  'Q'
#define CQM_MAGIC_2  0x14
#define CQM_HDR_MIN  18   /* uft_cqm.c reads 18 bytes for geometry fields */

static int cqm_probe_replica(const uint8_t *data, size_t size) {
    if (size < 3) return 0;
    if (data[0] == CQM_MAGIC_0 && data[1] == CQM_MAGIC_1 && data[2] == CQM_MAGIC_2)
        return 95;
    return 0;
}

static size_t build_minimal_cqm(uint8_t *out, size_t cap,
                                 uint8_t sz_code, uint8_t spt, uint8_t heads) {
    if (cap < CQM_HDR_MIN) return 0;
    memset(out, 0, CQM_HDR_MIN);
    out[0] = CQM_MAGIC_0;
    out[1] = CQM_MAGIC_1;
    out[2] = CQM_MAGIC_2;
    out[3] = sz_code;      /* sector size: (128 << code) */
    /* 4-7: reserved */
    out[8] = spt;          /* sectors per track */
    out[9] = heads;        /* 1 or 2 */
    return CQM_HDR_MIN;
}

TEST(probe_magic_valid) {
    uint8_t buf[CQM_HDR_MIN];
    build_minimal_cqm(buf, sizeof(buf), 2, 18, 2);   /* 1.44 MB HD layout */
    ASSERT(cqm_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_missing_c) {
    uint8_t buf[CQM_HDR_MIN];
    memset(buf, 0, sizeof(buf));
    /* 'C' wrong but 'Q' + 0x14 present */
    buf[0] = 'X';
    buf[1] = 'Q';
    buf[2] = 0x14;
    ASSERT(cqm_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_missing_q) {
    uint8_t buf[CQM_HDR_MIN];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'C';
    buf[1] = 'P';    /* wrong second byte */
    buf[2] = 0x14;
    ASSERT(cqm_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_missing_0x14) {
    uint8_t buf[CQM_HDR_MIN];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'C';
    buf[1] = 'Q';
    buf[2] = 0x13;    /* version byte wrong */
    ASSERT(cqm_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[2] = { 'C', 'Q' };
    ASSERT(cqm_probe_replica(buf, 2) == 0);
}

TEST(probe_exact_3_bytes_ok) {
    uint8_t buf[3] = { 'C', 'Q', 0x14 };
    ASSERT(cqm_probe_replica(buf, 3) == 95);
}

TEST(probe_empty_buffer) {
    ASSERT(cqm_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(geometry_fields_in_minimal) {
    uint8_t buf[CQM_HDR_MIN];
    /* 5.25" DD: 512-byte sectors (code 2), 9 SPT, 2 sides → 360KB */
    build_minimal_cqm(buf, sizeof(buf), 2, 9, 2);
    ASSERT(buf[3] == 2);
    ASSERT(buf[8] == 9);
    ASSERT(buf[9] == 2);
    uint16_t sec_size = (uint16_t)(128u << buf[3]);
    ASSERT(sec_size == 512);
}

TEST(magic_bytes_pin) {
    /* 0x14 is NOT a printable character — it's CopyQM's internal
     * version marker. Pin the non-obvious byte so nobody replaces it
     * with ' ' or similar "cleanup". */
    ASSERT(CQM_MAGIC_0 == 'C');
    ASSERT(CQM_MAGIC_1 == 'Q');
    ASSERT(CQM_MAGIC_2 == 0x14);
    ASSERT(CQM_MAGIC_2 != ' ');   /* definitely not a space */
    ASSERT(CQM_MAGIC_2 == 20);    /* also the decimal value */
}

int main(void) {
    printf("=== CQM (CopyQM) Plugin Tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_missing_c);
    RUN(probe_missing_q);
    RUN(probe_missing_0x14);
    RUN(probe_too_small);
    RUN(probe_exact_3_bytes_ok);
    RUN(probe_empty_buffer);
    RUN(geometry_fields_in_minimal);
    RUN(magic_bytes_pin);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
