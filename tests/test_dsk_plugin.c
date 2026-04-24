/**
 * @file test_dsk_plugin.c
 * @brief DSK (Amstrad CPC / ZX Spectrum) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/amstrad/dsk.c magic check.
 *
 * Format: 256-byte header. First 8 bytes "MV - CPC" for standard
 * DSK. EDSK (Extended DSK) uses "EXTENDED" — a separate plugin
 * handles that shape. This test validates the standard DSK probe only.
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

#define DSK_MAGIC        "MV - CPC"
#define DSK_MAGIC_LEN    8
#define DSK_HEADER_SIZE  256

static int dsk_probe_replica(const uint8_t *data, size_t size) {
    if (size < DSK_MAGIC_LEN) return 0;
    if (memcmp(data, DSK_MAGIC, DSK_MAGIC_LEN) != 0) return 0;
    return 90;
}

TEST(probe_magic_valid) {
    uint8_t buf[DSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, DSK_MAGIC, DSK_MAGIC_LEN);
    ASSERT(dsk_probe_replica(buf, sizeof(buf)) == 90);
}

TEST(probe_edsk_magic_rejected) {
    /* Extended DSK uses "EXTENDED" magic — standard DSK probe must
     * NOT claim it. (Separate plugin handles EDSK.) */
    uint8_t buf[DSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "EXTENDED", 8);
    ASSERT(dsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_partial_magic) {
    uint8_t buf[DSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "MV - CP", 7);      /* missing trailing 'C' */
    ASSERT(dsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_wrong_case) {
    uint8_t buf[DSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "mv - cpc", 8);
    ASSERT(dsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[DSK_MAGIC_LEN - 1];
    memcpy(buf, DSK_MAGIC, sizeof(buf));
    ASSERT(dsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_rejects_d64_anywhere) {
    /* "MV - CPC" must be at offset 0, not further in. */
    uint8_t buf[DSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf + 16, DSK_MAGIC, DSK_MAGIC_LEN);
    ASSERT(dsk_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== DSK plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_edsk_magic_rejected);
    RUN(probe_partial_magic);
    RUN(probe_wrong_case);
    RUN(probe_too_small);
    RUN(probe_rejects_d64_anywhere);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
