/**
 * @file test_td0_plugin.c
 * @brief TD0 (Sydex Teledisk) Plugin — probe tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/td0/uft_td0.c probe logic.
 *
 * Format: LE16 magic at offset 0.
 *   0x5444 = on-disk bytes {'D', 'T'} — normal (uncompressed)
 *   0x6474 = on-disk bytes {'d', 't'} — advanced (LZSS-compressed)
 *
 * Careful: the magic "looks like TD" when printed as a hex number
 * (0x5444), but little-endian reading stores it as bytes {'D','T'}
 * on disk. That's why hex dumps of TD0 files show "DT" at offset 0.
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

#define TD0_MAGIC_NORMAL    0x5444   /* 'T','D' */
#define TD0_MAGIC_ADVANCED  0x6474   /* 't','d' */
#define TD0_HEADER_SIZE     12

static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int td0_probe_replica(const uint8_t *data, size_t size) {
    if (size < 2) return 0;
    uint16_t magic = rd_le16(data);
    if (magic == TD0_MAGIC_NORMAL || magic == TD0_MAGIC_ADVANCED) return 95;
    return 0;
}

static size_t build_minimal_td0(uint8_t *out, size_t cap, uint16_t magic) {
    if (cap < TD0_HEADER_SIZE) return 0;
    memset(out, 0, TD0_HEADER_SIZE);
    out[0] = (uint8_t)(magic & 0xFF);
    out[1] = (uint8_t)(magic >> 8);
    out[2] = 0;         /* volume seq */
    out[3] = 0;         /* check sig */
    out[4] = 21;        /* Teledisk version (1.5 as 0x15 = 21) */
    out[5] = 0;         /* data source */
    out[6] = 2;         /* density (2=HD) */
    out[7] = 2;         /* drive type */
    out[8] = 0;         /* stepping */
    out[9] = 0;         /* dos flag */
    out[10] = 2;        /* sides */
    /* CRC at 11 — leave 0 for probe-only test */
    return TD0_HEADER_SIZE;
}

TEST(probe_normal_magic) {
    uint8_t buf[TD0_HEADER_SIZE];
    build_minimal_td0(buf, sizeof(buf), TD0_MAGIC_NORMAL);
    ASSERT(td0_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_advanced_magic) {
    uint8_t buf[TD0_HEADER_SIZE];
    build_minimal_td0(buf, sizeof(buf), TD0_MAGIC_ADVANCED);
    ASSERT(td0_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_magic) {
    /* "DX" — wrong second byte. */
    uint8_t buf[TD0_HEADER_SIZE] = { 'D', 'X' };
    ASSERT(td0_probe_replica(buf, sizeof(buf)) == 0);

    /* "TD" in on-disk order would be 0x4454 LE which is NEITHER the
     * normal nor advanced magic — the correct sequence is {'D','T'}. */
    uint8_t swapped[TD0_HEADER_SIZE] = { 'T', 'D' };
    ASSERT(td0_probe_replica(swapped, sizeof(swapped)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[1] = { 'T' };
    ASSERT(td0_probe_replica(buf, 1) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(td0_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(probe_first_two_bytes_only) {
    /* Probe reads only the first 2 bytes — rest can be anything.
     * On-disk order is {'D','T'} which reads as LE16 = 0x5444. */
    uint8_t buf[2] = { 'D', 'T' };
    ASSERT(td0_probe_replica(buf, 2) == 95);
}

TEST(build_minimal_header_fields) {
    uint8_t buf[TD0_HEADER_SIZE];
    build_minimal_td0(buf, sizeof(buf), TD0_MAGIC_NORMAL);
    /* Byte order on-disk for 0x5444 LE is D (0x44), T (0x54). */
    ASSERT(buf[0] == 'D' && buf[1] == 'T');
    ASSERT(buf[4] == 21);                /* version 1.5 */
    ASSERT(buf[6] == 2);                 /* HD density */
    ASSERT(buf[10] == 2);                /* double sided */
}

TEST(magic_ascii_case_pin) {
    /* Uppercase = normal, lowercase = LZSS-compressed.
     * The magic WORDS are 0x5444/0x6474; when written little-endian
     * to disk, byte[0] is the low half ('D'/'d'), byte[1] the high
     * half ('T'/'t'). */
    ASSERT((uint8_t)'T' == 0x54);
    ASSERT((uint8_t)'D' == 0x44);
    ASSERT((uint8_t)'t' == 0x74);
    ASSERT((uint8_t)'d' == 0x64);
    ASSERT(TD0_MAGIC_NORMAL   == ('D' | ('T' << 8)));  /* 0x44 | 0x5400 */
    ASSERT(TD0_MAGIC_ADVANCED == ('t' | ('d' << 8)));  /* 0x74 | 0x6400 */
}

int main(void) {
    printf("=== TD0 (Teledisk) Plugin Tests ===\n");
    RUN(probe_normal_magic);
    RUN(probe_advanced_magic);
    RUN(probe_wrong_magic);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(probe_first_two_bytes_only);
    RUN(build_minimal_header_fields);
    RUN(magic_ascii_case_pin);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
