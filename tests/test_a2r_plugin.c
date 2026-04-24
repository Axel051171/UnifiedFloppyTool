/**
 * @file test_a2r_plugin.c
 * @brief A2R (Applesauce Apple II flux) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/parsers/a2r/uft_a2r_parser.c magic check.
 *
 * Format: 8-byte prefix — 4 bytes magic "A2R2" or "A2R3" + 4 bytes
 * suffix FF 0A 0D 0A (Applesauce anti-text-conversion guard).
 * Same convention as MOOF/WOZ.
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

#define A2R_MAGIC_V2      "A2R2"
#define A2R_MAGIC_V3      "A2R3"
#define A2R_MAGIC_LEN     4
#define A2R_PREFIX_SIZE   8

static const uint8_t A2R_SUFFIX[4] = { 0xFF, 0x0A, 0x0D, 0x0A };

/* Returns 0=no, 2=v2 match, 3=v3 match */
static int a2r_probe_replica(const uint8_t *data, size_t size) {
    if (size < A2R_PREFIX_SIZE) return 0;
    if (memcmp(data + 4, A2R_SUFFIX, 4) != 0) return 0;
    if (memcmp(data, A2R_MAGIC_V2, 4) == 0) return 2;
    if (memcmp(data, A2R_MAGIC_V3, 4) == 0) return 3;
    return 0;
}

static void build_a2r_prefix(uint8_t *out, const char *magic) {
    memcpy(out, magic, A2R_MAGIC_LEN);
    memcpy(out + 4, A2R_SUFFIX, 4);
}

TEST(probe_magic_v2) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    build_a2r_prefix(buf, A2R_MAGIC_V2);
    ASSERT(a2r_probe_replica(buf, sizeof(buf)) == 2);
}

TEST(probe_magic_v3) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    build_a2r_prefix(buf, A2R_MAGIC_V3);
    ASSERT(a2r_probe_replica(buf, sizeof(buf)) == 3);
}

TEST(probe_unknown_version) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    build_a2r_prefix(buf, "A2R4");        /* hypothetical future version */
    /* Probe should reject unknown versions conservatively. */
    ASSERT(a2r_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_missing_suffix_guard) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, A2R_MAGIC_V2, 4);
    /* Suffix all zeros — file has been text-converted. */
    ASSERT(a2r_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[A2R_PREFIX_SIZE - 1];
    build_a2r_prefix(buf, A2R_MAGIC_V2);
    ASSERT(a2r_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_moof_is_not_a2r) {
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "MOOF", 4);
    memcpy(buf + 4, A2R_SUFFIX, 4);
    ASSERT(a2r_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== A2R plugin probe tests ===\n");
    RUN(probe_magic_v2);
    RUN(probe_magic_v3);
    RUN(probe_unknown_version);
    RUN(probe_missing_suffix_guard);
    RUN(probe_too_small);
    RUN(probe_moof_is_not_a2r);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
