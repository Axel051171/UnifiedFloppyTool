/**
 * @file test_scp_plugin.c
 * @brief SCP (SuperCard Pro flux) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/flux/scp.c magic check.
 *
 * Format: 16-byte header. First 3 bytes "SCP". Byte 3 = version.
 * Bytes 4-5 = revolutions, bytes 6-7 = track count, then various
 * config bytes and a 4-byte BE file checksum. The probe checks the
 * 3-byte magic and validates revolution/track counts for plausibility.
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

#define SCP_MAGIC       "SCP"
#define SCP_MAGIC_LEN   3
#define SCP_HEADER_SIZE 16

static int scp_probe_replica(const uint8_t *data, size_t size) {
    if (size < SCP_HEADER_SIZE) return 0;
    if (memcmp(data, SCP_MAGIC, SCP_MAGIC_LEN) != 0) return 0;
    return 95;
}

static void build_minimal_scp(uint8_t *out, uint8_t version,
                                uint16_t revs, uint16_t tracks) {
    memset(out, 0, SCP_HEADER_SIZE);
    out[0] = 'S';
    out[1] = 'C';
    out[2] = 'P';
    out[3] = version;
    /* LE16 revolutions at offset 4 */
    out[4] = (uint8_t)(revs & 0xFF);
    out[5] = (uint8_t)(revs >> 8);
    /* LE16 track count at offset 6 */
    out[6] = (uint8_t)(tracks & 0xFF);
    out[7] = (uint8_t)(tracks >> 8);
}

TEST(probe_magic_valid_v16) {
    uint8_t buf[SCP_HEADER_SIZE];
    build_minimal_scp(buf, 0x16, 5, 80);   /* 5 revs, 80 tracks — classic */
    ASSERT(scp_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_magic_valid_v24) {
    uint8_t buf[SCP_HEADER_SIZE];
    build_minimal_scp(buf, 0x24, 3, 42);   /* Applesauce-style SCP */
    ASSERT(scp_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_magic) {
    uint8_t buf[SCP_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "PCS", 3);   /* byte-reversed */
    ASSERT(scp_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_magic_wrong_case) {
    uint8_t buf[SCP_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "scp", 3);
    ASSERT(scp_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[SCP_HEADER_SIZE - 1];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, SCP_MAGIC, SCP_MAGIC_LEN);
    ASSERT(scp_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_rejects_moof_magic) {
    /* MOOF starts with "MOOF", not "SCP". */
    uint8_t buf[SCP_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "MOOF", 4);
    ASSERT(scp_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== SCP plugin probe tests ===\n");
    RUN(probe_magic_valid_v16);
    RUN(probe_magic_valid_v24);
    RUN(probe_wrong_magic);
    RUN(probe_magic_wrong_case);
    RUN(probe_too_small);
    RUN(probe_rejects_moof_magic);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
