/**
 * @file test_t64_plugin.c
 * @brief T64 (Commodore Tape Image) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/commodore/t64.c probe logic.
 *
 * Format: 19-byte ASCII magic "C64 tape image file" at offset 0 of a
 * 32-byte header. The reference loader is uft_cbm_t64_open in
 * src/formats/commodore/t64.c.
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

#define T64_MAGIC       "C64 tape image file"
#define T64_MAGIC_LEN   19
#define T64_HEADER_SIZE 32

static int t64_probe_replica(const uint8_t *data, size_t size) {
    if (size < T64_HEADER_SIZE) return 0;
    if (memcmp(data, T64_MAGIC, T64_MAGIC_LEN) != 0) return 0;
    return 95;
}

static size_t build_minimal_t64(uint8_t *out, size_t cap) {
    if (cap < T64_HEADER_SIZE) return 0;
    memset(out, 0, T64_HEADER_SIZE);
    memcpy(out, T64_MAGIC, T64_MAGIC_LEN);
    return T64_HEADER_SIZE;
}

TEST(probe_magic_valid) {
    uint8_t buf[T64_HEADER_SIZE];
    build_minimal_t64(buf, sizeof(buf));
    ASSERT(t64_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_magic) {
    uint8_t buf[T64_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    /* Plausible-looking but not the T64 magic */
    memcpy(buf, "C64 disk image file", 19);
    ASSERT(t64_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_empty_header) {
    uint8_t buf[T64_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    ASSERT(t64_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[T64_HEADER_SIZE - 1];
    memcpy(buf, T64_MAGIC, T64_MAGIC_LEN);
    ASSERT(t64_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_magic_with_trailing_garbage) {
    /* Header size is 32; probe accepts any bytes 19..31 as long as
     * the magic matches at offset 0. */
    uint8_t buf[T64_HEADER_SIZE];
    memcpy(buf, T64_MAGIC, T64_MAGIC_LEN);
    for (int i = T64_MAGIC_LEN; i < T64_HEADER_SIZE; i++) buf[i] = 0xAA;
    ASSERT(t64_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_magic_with_wrong_case) {
    uint8_t buf[T64_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "c64 tape image file", 19);   /* lowercase 'c' */
    ASSERT(t64_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== T64 plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_wrong_magic);
    RUN(probe_empty_header);
    RUN(probe_too_small);
    RUN(probe_magic_with_trailing_garbage);
    RUN(probe_magic_with_wrong_case);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
