/**
 * @file test_crt_plugin.c
 * @brief CRT (Commodore Cartridge) Plugin — probe tests.
 *
 * Self-contained per template from test_atr_plugin.c / test_stx_plugin.c.
 * Mirrors src/formats/commodore/crt.c probe logic.
 *
 * Format: 16-byte ASCII magic "C64 CARTRIDGE   " at offset 0 (note the
 * three trailing spaces to pad to 16 bytes). The reference loader is
 * uft_cbm_crt_open in src/formats/commodore/crt.c which reads the first
 * 16 bytes and does an exact memcmp against the magic string.
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

#define CRT_MAGIC       "C64 CARTRIDGE   "
#define CRT_MAGIC_LEN   16

/* Replica of the probe logic from uft_cbm_crt_open. */
static int crt_probe_replica(const uint8_t *data, size_t size) {
    if (size < CRT_MAGIC_LEN) return 0;
    if (memcmp(data, CRT_MAGIC, CRT_MAGIC_LEN) != 0) return 0;
    return 95;
}

static size_t build_minimal_crt(uint8_t *out, size_t cap) {
    if (cap < 64) return 0;
    memset(out, 0, 64);
    memcpy(out, CRT_MAGIC, CRT_MAGIC_LEN);
    /* Real CRT has a header of 0x40 bytes; fill plausible fields,
     * but probe only checks the magic. */
    return 64;
}

TEST(probe_magic_valid) {
    uint8_t buf[64];
    build_minimal_crt(buf, sizeof(buf));
    ASSERT(crt_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_magic) {
    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT(crt_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_truncated_magic) {
    /* "C64 CARTRIDGE" without the trailing spaces */
    uint8_t buf[64] = { 'C','6','4',' ','C','A','R','T','R','I','D','G','E',0,0,0 };
    /* Completed to 16 bytes but missing the 3 trailing spaces */
    ASSERT(crt_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[CRT_MAGIC_LEN - 1];
    memcpy(buf, CRT_MAGIC, sizeof(buf));
    ASSERT(crt_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_magic_midbuffer) {
    /* Magic must be at offset 0, not somewhere inside the file. */
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    memcpy(buf + 16, CRT_MAGIC, CRT_MAGIC_LEN);
    ASSERT(crt_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_empty) {
    ASSERT(crt_probe_replica((const uint8_t *)"", 0) == 0);
}

int main(void) {
    printf("=== CRT plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_wrong_magic);
    RUN(probe_truncated_magic);
    RUN(probe_too_small);
    RUN(probe_magic_midbuffer);
    RUN(probe_empty);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
