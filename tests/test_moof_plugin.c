/**
 * @file test_moof_plugin.c
 * @brief MOOF (Applesauce Macintosh flux) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/apple/uft_moof_parser.c magic check.
 *
 * Format: 12-byte header — 4 bytes magic "MOOF" + 4 bytes suffix
 * FF 0A 0D 0A (the Applesauce anti-text-conversion guard) + 4 bytes
 * CRC32 (not checked at probe time). Reference:
 * https://applesaucefdc.com/moof-reference/
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

#define MOOF_MAGIC        "MOOF"
#define MOOF_MAGIC_LEN    4
#define MOOF_HEADER_SIZE  12

static const uint8_t MOOF_SUFFIX[4] = { 0xFF, 0x0A, 0x0D, 0x0A };

static int moof_probe_replica(const uint8_t *data, size_t size) {
    if (size < MOOF_HEADER_SIZE) return 0;
    if (memcmp(data, MOOF_MAGIC, MOOF_MAGIC_LEN) != 0) return 0;
    if (memcmp(data + 4, MOOF_SUFFIX, 4) != 0) return 0;
    return 95;
}

static size_t build_minimal_moof(uint8_t *out, size_t cap) {
    if (cap < MOOF_HEADER_SIZE) return 0;
    memset(out, 0, MOOF_HEADER_SIZE);
    memcpy(out, MOOF_MAGIC, MOOF_MAGIC_LEN);
    memcpy(out + 4, MOOF_SUFFIX, 4);
    /* CRC32 left zero — not validated at probe time */
    return MOOF_HEADER_SIZE;
}

TEST(probe_magic_valid) {
    uint8_t buf[MOOF_HEADER_SIZE];
    build_minimal_moof(buf, sizeof(buf));
    ASSERT(moof_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_magic) {
    uint8_t buf[MOOF_HEADER_SIZE];
    build_minimal_moof(buf, sizeof(buf));
    /* Corrupt the magic */
    buf[0] = 'W';
    ASSERT(moof_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_wrong_suffix) {
    uint8_t buf[MOOF_HEADER_SIZE];
    build_minimal_moof(buf, sizeof(buf));
    /* Simulate FTP text-conversion: \r\n → \n */
    buf[5] = 0x0A;
    buf[6] = 0x0A;
    buf[7] = 0x0A;
    ASSERT(moof_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_suffix_missing_guard) {
    uint8_t buf[MOOF_HEADER_SIZE];
    build_minimal_moof(buf, sizeof(buf));
    /* Drop the 0xFF guard byte that detects non-binary transfers */
    buf[4] = 0x00;
    ASSERT(moof_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[MOOF_HEADER_SIZE - 1];
    memcpy(buf, MOOF_MAGIC, MOOF_MAGIC_LEN);
    memcpy(buf + 4, MOOF_SUFFIX, 4);
    ASSERT(moof_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_woz_magic_is_not_moof) {
    /* WOZ (WOZ1/WOZ2) has the same suffix shape but different magic. */
    uint8_t buf[MOOF_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "WOZ2", 4);
    memcpy(buf + 4, MOOF_SUFFIX, 4);
    ASSERT(moof_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== MOOF plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_wrong_magic);
    RUN(probe_wrong_suffix);
    RUN(probe_suffix_missing_guard);
    RUN(probe_too_small);
    RUN(probe_woz_magic_is_not_moof);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
