/**
 * @file test_woz_plugin.c
 * @brief WOZ (Applesauce Apple II disk) Plugin — probe tests.
 *
 * Self-contained per template from test_moof_plugin.c.
 * Mirrors src/formats/apple/uft_woz.c magic check.
 *
 * Format: 12-byte header — 4 bytes magic "WOZ1" or "WOZ2" + 4 bytes
 * suffix FF 0A 0D 0A (Applesauce anti-text-conversion guard) +
 * 4 bytes CRC32 (not checked at probe time). Shared convention
 * with MOOF and A2R.
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

#define WOZ1_MAGIC      "WOZ1"
#define WOZ2_MAGIC      "WOZ2"
#define WOZ_MAGIC_LEN   4
#define WOZ_HEADER_SIZE 12

static const uint8_t WOZ_SUFFIX[4] = { 0xFF, 0x0A, 0x0D, 0x0A };

/* Returns 0=no, 1=WOZ1, 2=WOZ2 */
static int woz_probe_replica(const uint8_t *data, size_t size) {
    if (size < WOZ_HEADER_SIZE) return 0;
    if (memcmp(data + 4, WOZ_SUFFIX, 4) != 0) return 0;
    if (memcmp(data, WOZ1_MAGIC, 4) == 0) return 1;
    if (memcmp(data, WOZ2_MAGIC, 4) == 0) return 2;
    return 0;
}

static void build_woz_header(uint8_t *out, const char *magic) {
    memset(out, 0, WOZ_HEADER_SIZE);
    memcpy(out, magic, WOZ_MAGIC_LEN);
    memcpy(out + 4, WOZ_SUFFIX, 4);
}

TEST(probe_woz1) {
    uint8_t buf[WOZ_HEADER_SIZE];
    build_woz_header(buf, WOZ1_MAGIC);
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 1);
}

TEST(probe_woz2) {
    uint8_t buf[WOZ_HEADER_SIZE];
    build_woz_header(buf, WOZ2_MAGIC);
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 2);
}

TEST(probe_unknown_version) {
    uint8_t buf[WOZ_HEADER_SIZE];
    build_woz_header(buf, "WOZ9");
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_suffix_text_converted) {
    /* File that passed through a text-mode FTP: 0xFF stripped, \r\n
     * collapsed to \n. Applesauce's guard is specifically designed
     * to make this detectable. */
    uint8_t buf[WOZ_HEADER_SIZE];
    build_woz_header(buf, WOZ2_MAGIC);
    buf[4] = 0x00;
    buf[5] = 0x0A;
    buf[6] = 0x0A;
    buf[7] = 0x0A;
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[WOZ_HEADER_SIZE - 1];
    build_woz_header(buf, WOZ2_MAGIC);
    /* (partial build, missing last byte of CRC) */
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_moof_not_woz) {
    uint8_t buf[WOZ_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "MOOF", 4);
    memcpy(buf + 4, WOZ_SUFFIX, 4);
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_a2r_not_woz) {
    uint8_t buf[WOZ_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "A2R2", 4);
    memcpy(buf + 4, WOZ_SUFFIX, 4);
    ASSERT(woz_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== WOZ plugin probe tests ===\n");
    RUN(probe_woz1);
    RUN(probe_woz2);
    RUN(probe_unknown_version);
    RUN(probe_suffix_text_converted);
    RUN(probe_too_small);
    RUN(probe_moof_not_woz);
    RUN(probe_a2r_not_woz);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
