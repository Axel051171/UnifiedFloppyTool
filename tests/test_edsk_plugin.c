/**
 * @file test_edsk_plugin.c
 * @brief EDSK (Extended Amstrad CPC DSK) Plugin — probe tests.
 *
 * Self-contained per template from test_dsk_plugin.c.
 * Mirrors src/formats/amstrad/edsk_extdsk.c magic check.
 *
 * Format: 256-byte header. First 21 bytes "EXTENDED CPC DSK File"
 * — distinct from standard DSK ("MV - CPC"). Used for disks with
 * variable sector sizes, weak sectors, and copy protection.
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

#define EDSK_MAGIC        "EXTENDED CPC DSK File"
#define EDSK_MAGIC_LEN    21
#define EDSK_HEADER_SIZE  256

static int edsk_probe_replica(const uint8_t *data, size_t size) {
    if (size < EDSK_MAGIC_LEN) return 0;
    if (memcmp(data, EDSK_MAGIC, EDSK_MAGIC_LEN) != 0) return 0;
    return 92;
}

TEST(probe_magic_valid) {
    uint8_t buf[EDSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, EDSK_MAGIC, EDSK_MAGIC_LEN);
    ASSERT(edsk_probe_replica(buf, sizeof(buf)) == 92);
}

TEST(probe_standard_dsk_rejected) {
    /* Standard DSK ("MV - CPC") must not match EDSK probe. */
    uint8_t buf[EDSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "MV - CPC", 8);
    ASSERT(edsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_partial_magic) {
    uint8_t buf[EDSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    /* "EXTENDED CPC" — too short, missing " DSK File" suffix. */
    memcpy(buf, "EXTENDED CPC", 12);
    ASSERT(edsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_typo_in_magic) {
    uint8_t buf[EDSK_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "EXTENDED CPC DSK Filo", EDSK_MAGIC_LEN); /* 'Filo' */
    ASSERT(edsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[EDSK_MAGIC_LEN - 1];
    memcpy(buf, EDSK_MAGIC, sizeof(buf));
    ASSERT(edsk_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_full_magic_exactly_21) {
    /* Minimum buffer that could hold the magic. */
    uint8_t buf[EDSK_MAGIC_LEN];
    memcpy(buf, EDSK_MAGIC, EDSK_MAGIC_LEN);
    ASSERT(edsk_probe_replica(buf, sizeof(buf)) == 92);
}

int main(void) {
    printf("=== EDSK plugin probe tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_standard_dsk_rejected);
    RUN(probe_partial_magic);
    RUN(probe_typo_in_magic);
    RUN(probe_too_small);
    RUN(probe_full_magic_exactly_21);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
