/**
 * @file test_dc42_plugin.c
 * @brief DC42 (Apple DiskCopy 4.2) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/dc42/uft_dc42.c::dc42_probe.
 *
 * Format: 84-byte header. Magic 0x0100 as BE16 at offset 82.
 * Name-length byte at offset 0 must be <= 63.
 * Data-size (BE32) at offset 0x40 must be non-zero.
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

#define DC42_HEADER_SIZE   84
#define DC42_MAGIC         0x0100
#define DC42_MAX_NAME      63
#define DC42_SECTOR_SIZE   512

static int dc42_probe_replica(const uint8_t *data, size_t size) {
    if (size < DC42_HEADER_SIZE) return 0;
    /* Magic at offset 82 (BE16) */
    uint16_t magic = ((uint16_t)data[82] << 8) | data[83];
    if (magic != DC42_MAGIC) return 0;
    /* Name length must be sane */
    if (data[0] > DC42_MAX_NAME) return 0;
    /* Data size (BE32) at offset 0x40 must be non-zero */
    uint32_t data_size = ((uint32_t)data[0x40] << 24) |
                         ((uint32_t)data[0x41] << 16) |
                         ((uint32_t)data[0x42] << 8) |
                          (uint32_t)data[0x43];
    if (data_size == 0) return 0;
    return 95;
}

static void build_minimal_dc42(uint8_t *out,
                                uint8_t name_len,
                                uint32_t data_size_be) {
    memset(out, 0, DC42_HEADER_SIZE);
    out[0] = name_len;
    /* Data size BE32 at offset 0x40 */
    out[0x40] = (uint8_t)(data_size_be >> 24);
    out[0x41] = (uint8_t)(data_size_be >> 16);
    out[0x42] = (uint8_t)(data_size_be >> 8);
    out[0x43] = (uint8_t)(data_size_be);
    /* Magic BE16 0x0100 at offset 82 */
    out[82] = 0x01;
    out[83] = 0x00;
}

TEST(probe_magic_valid_400k) {
    uint8_t buf[DC42_HEADER_SIZE];
    /* 400K Mac disk: 400 * 1024 = 409600 bytes data */
    build_minimal_dc42(buf, 8, 409600);
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_magic_valid_800k) {
    uint8_t buf[DC42_HEADER_SIZE];
    build_minimal_dc42(buf, 12, 819200);
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_magic) {
    uint8_t buf[DC42_HEADER_SIZE];
    build_minimal_dc42(buf, 8, 409600);
    buf[82] = 0x00;
    buf[83] = 0x01;       /* Swapped bytes — reads as 0x0001, not 0x0100 */
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_name_length_too_large) {
    uint8_t buf[DC42_HEADER_SIZE];
    build_minimal_dc42(buf, DC42_MAX_NAME + 1, 409600);
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_zero_data_size) {
    uint8_t buf[DC42_HEADER_SIZE];
    build_minimal_dc42(buf, 8, 0);
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[DC42_HEADER_SIZE - 1];
    memset(buf, 0, sizeof(buf));
    /* Can't fit all the required bytes for probe to succeed */
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_boundary_name_length_63) {
    uint8_t buf[DC42_HEADER_SIZE];
    build_minimal_dc42(buf, DC42_MAX_NAME, 409600);
    /* Exactly max allowed name length must still pass */
    ASSERT(dc42_probe_replica(buf, sizeof(buf)) == 95);
}

int main(void) {
    printf("=== DC42 plugin probe tests ===\n");
    RUN(probe_magic_valid_400k);
    RUN(probe_magic_valid_800k);
    RUN(probe_wrong_magic);
    RUN(probe_name_length_too_large);
    RUN(probe_zero_data_size);
    RUN(probe_too_small);
    RUN(probe_boundary_name_length_63);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
