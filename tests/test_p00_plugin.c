/**
 * @file test_p00_plugin.c
 * @brief P00 (Commodore PC64 single-file container) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/commodore/p00.c and src/formats/c64/uft_p00.c
 * probe logic.
 *
 * Format: 26-byte header — 7-byte magic "C64File" followed by a NUL
 * terminator at offset 7, then 16-byte PETSCII filename (offset 8),
 * 1-byte record count (offset 25).
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

#define P00_MAGIC          "C64File"
#define P00_MAGIC_LEN      7
#define P00_HEADER_SIZE    26

static int p00_probe_replica(const uint8_t *data, size_t size) {
    if (size < P00_HEADER_SIZE) return 0;
    if (memcmp(data, P00_MAGIC, P00_MAGIC_LEN) != 0) return 0;
    /* Byte 7 must be NUL separator per uft_p00.c comment. */
    if (data[7] != 0x00) return 0;
    return 95;
}

static void build_minimal_p00(uint8_t *out) {
    memset(out, 0, P00_HEADER_SIZE);
    memcpy(out, P00_MAGIC, P00_MAGIC_LEN);
    /* Byte 7 stays NUL (already zeroed). */
    /* Filename (PETSCII) at offset 8, 16 bytes — irrelevant for probe. */
    /* Record count at offset 25 — irrelevant for probe. */
}

TEST(probe_magic_valid) {
    uint8_t buf[P00_HEADER_SIZE];
    build_minimal_p00(buf);
    ASSERT(p00_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_missing_null_separator) {
    uint8_t buf[P00_HEADER_SIZE];
    build_minimal_p00(buf);
    buf[7] = 'X';     /* Not NUL — probe must reject */
    ASSERT(p00_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_wrong_magic) {
    uint8_t buf[P00_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "C128File", 8);     /* Similar but different container flavor */
    ASSERT(p00_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_magic_wrong_case) {
    uint8_t buf[P00_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "c64file", P00_MAGIC_LEN);    /* lowercase */
    ASSERT(p00_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[P00_HEADER_SIZE - 1];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, P00_MAGIC, P00_MAGIC_LEN);
    ASSERT(p00_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_accepts_trailing_filename) {
    /* A realistic P00 has a valid PETSCII filename at offset 8.
     * Probe should not care about it. */
    uint8_t buf[P00_HEADER_SIZE];
    build_minimal_p00(buf);
    memcpy(buf + 8, "GAME.PRG\0\0\0\0\0\0\0\0", 16);
    buf[25] = 0;  /* record count */
    ASSERT(p00_probe_replica(buf, sizeof(buf)) == 95);
}

int main(void) {
    printf("=== P00 plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_missing_null_separator);
    RUN(probe_wrong_magic);
    RUN(probe_magic_wrong_case);
    RUN(probe_too_small);
    RUN(probe_accepts_trailing_filename);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
