/**
 * @file test_fdi_plugin.c
 * @brief FDI (Formatted Disk Image) Plugin — probe tests.
 *
 * Self-contained per template from test_crt_plugin.c.
 * Mirrors src/formats/fdi/uft_fdi_plugin.c::fdi_plugin_probe.
 *
 * Format: 14-byte header.
 *   offset 0..2  "FDI" magic
 *   offset 3     write-protect flag
 *   offset 4..5  cylinders (LE16)
 *   offset 6..7  heads (LE16)
 *   offset 8..9  desc offset (LE16)
 *   offset 10..11 data offset (LE16)
 *   offset 12..13 extra offset (LE16)
 *
 * Probe confidence: 92 if geometry is plausible (cyls 1..96, heads 1..2),
 * 75 if magic matches but geometry looks wrong.
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

#define FDI_SIG         "FDI"
#define FDI_SIG_LEN     3
#define FDI_HDR_SIZE    14

static uint16_t rd_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int fdi_probe_replica(const uint8_t *data, size_t size) {
    if (size < FDI_HDR_SIZE) return 0;
    if (memcmp(data, FDI_SIG, FDI_SIG_LEN) != 0) return 0;
    uint16_t cyls = rd_le16(data + 4);
    uint16_t heads = rd_le16(data + 6);
    if (cyls > 0 && cyls <= 96 && heads > 0 && heads <= 2) return 92;
    return 75;
}

static void build_minimal_fdi(uint8_t *out,
                               uint16_t cyls, uint16_t heads,
                               uint16_t data_off) {
    memset(out, 0, FDI_HDR_SIZE);
    memcpy(out, FDI_SIG, FDI_SIG_LEN);
    out[3] = 0;    /* WP flag */
    out[4] = (uint8_t)(cyls & 0xFF);
    out[5] = (uint8_t)(cyls >> 8);
    out[6] = (uint8_t)(heads & 0xFF);
    out[7] = (uint8_t)(heads >> 8);
    out[8] = 0;    /* desc_off lo */
    out[9] = 0;
    out[10] = (uint8_t)(data_off & 0xFF);
    out[11] = (uint8_t)(data_off >> 8);
    out[12] = 0;
    out[13] = 0;
}

TEST(probe_high_confidence_standard_geom) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 80, 2, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 92);
}

TEST(probe_high_confidence_single_head) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 40, 1, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 92);
}

TEST(probe_low_confidence_zero_cylinders) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 0, 2, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 75);
}

TEST(probe_low_confidence_too_many_cylinders) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 200, 2, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 75);
}

TEST(probe_low_confidence_three_heads) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 80, 3, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 75);
}

TEST(probe_wrong_magic) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 80, 2, 14);
    buf[0] = 'X';
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[FDI_HDR_SIZE - 1];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, FDI_SIG, FDI_SIG_LEN);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_boundary_96_cyls) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 96, 2, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 92);
}

TEST(probe_boundary_97_cyls_drops_to_low) {
    uint8_t buf[FDI_HDR_SIZE];
    build_minimal_fdi(buf, 97, 2, 14);
    ASSERT(fdi_probe_replica(buf, sizeof(buf)) == 75);
}

int main(void) {
    printf("=== FDI plugin probe tests ===\n");
    RUN(probe_high_confidence_standard_geom);
    RUN(probe_high_confidence_single_head);
    RUN(probe_low_confidence_zero_cylinders);
    RUN(probe_low_confidence_too_many_cylinders);
    RUN(probe_low_confidence_three_heads);
    RUN(probe_wrong_magic);
    RUN(probe_too_small);
    RUN(probe_boundary_96_cyls);
    RUN(probe_boundary_97_cyls_drops_to_low);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
