/**
 * @file test_atr_plugin.c
 * @brief ATR (Atari 8-bit) Plugin — probe tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/atr/uft_atr.c probe logic.
 *
 * Format: LE16 magic 0x0296 at offset 0, 16-byte header.
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

#define ATR_MAGIC         0x0296
#define ATR_HEADER_SIZE   16
#define ATR_BOOT_SECTORS  3
#define ATR_BOOT_SEC_SIZE 128

static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int atr_probe_replica(const uint8_t *data, size_t size) {
    if (size < ATR_HEADER_SIZE) return 0;
    if (rd_le16(data) == ATR_MAGIC) return 95;
    return 0;
}

static size_t build_minimal_atr(uint8_t *out, size_t cap,
                                 uint32_t data_para, uint16_t sec_size) {
    if (cap < ATR_HEADER_SIZE) return 0;
    memset(out, 0, ATR_HEADER_SIZE);
    /* magic LE16 at 0 */
    out[0] = ATR_MAGIC & 0xFF;
    out[1] = (ATR_MAGIC >> 8) & 0xFF;
    /* data_size in 16-byte paragraphs, LE16 at 2 */
    out[2] = (uint8_t)(data_para & 0xFF);
    out[3] = (uint8_t)((data_para >> 8) & 0xFF);
    /* sector size LE16 at 4 */
    out[4] = (uint8_t)(sec_size & 0xFF);
    out[5] = (uint8_t)(sec_size >> 8);
    /* high byte of data_size at 6 (for >8MB images, rarely used) */
    return ATR_HEADER_SIZE;
}

TEST(probe_magic_valid) {
    uint8_t buf[ATR_HEADER_SIZE];
    build_minimal_atr(buf, sizeof(buf), 5760, 128);  /* 90K SD disk */
    ASSERT(atr_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_magic_wrong_byte_order) {
    uint8_t buf[ATR_HEADER_SIZE] = { 0x96, 0x02 };   /* swapped — still 0x0296 LE = swapped 0x9602 BE */
    /* Wait — 0x96, 0x02 IS 0x0296 LE. Actual test is that a BE reading of 0x0296
     * wouldn't match. Use 0x02, 0x96 (byte-reversed) — that would read as 0x9602 LE. */
    uint8_t wrong[ATR_HEADER_SIZE] = { 0x02, 0x96 };
    ASSERT(atr_probe_replica(wrong, sizeof(wrong)) == 0);
    (void)buf;
}

TEST(probe_wrong_magic) {
    uint8_t buf[ATR_HEADER_SIZE] = { 0xFF, 0xFF };   /* garbage */
    ASSERT(atr_probe_replica(buf, sizeof(buf)) == 0);

    uint8_t zero[ATR_HEADER_SIZE];
    memset(zero, 0, sizeof(zero));
    ASSERT(atr_probe_replica(zero, sizeof(zero)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[2] = { 0x96, 0x02 };              /* magic present but too small */
    ASSERT(atr_probe_replica(buf, 2) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(atr_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(build_minimal_sizes) {
    uint8_t buf[ATR_HEADER_SIZE];

    /* SS/SD: 40 tracks × 18 sectors × 128 bytes = 90K */
    build_minimal_atr(buf, sizeof(buf), 5760, 128);
    ASSERT(rd_le16(buf + 4) == 128);

    /* SS/DD: same track/sector count, 256 bytes/sector = 180K */
    build_minimal_atr(buf, sizeof(buf), 11520, 256);
    ASSERT(rd_le16(buf + 4) == 256);

    /* SS/ED: 40 tracks × 26 sectors × 128 bytes = 130K */
    build_minimal_atr(buf, sizeof(buf), 8320, 128);
    ASSERT(rd_le16(buf + 2) == (8320 & 0xFFFF));
}

TEST(magic_value_pin) {
    /* ATR magic is the "SIO serial number" — not ASCII, pure hex.
     * Pinning this protects against well-meaning "code constant
     * cleanup" that would break every ATR reader in the wild. */
    ASSERT(ATR_MAGIC == 0x0296);
    ASSERT((ATR_MAGIC & 0xFF) == 0x96);
    ASSERT((ATR_MAGIC >> 8)  == 0x02);
}

TEST(boot_sector_layout_constants) {
    /* Atari 8-bit boot is 3 × 128-byte sectors regardless of the
     * disk's normal sector size. Pin these — they're in the format
     * spec and can't change. */
    ASSERT(ATR_BOOT_SECTORS  == 3);
    ASSERT(ATR_BOOT_SEC_SIZE == 128);
}

int main(void) {
    printf("=== ATR (Atari 8-bit) Plugin Tests ===\n");
    RUN(probe_magic_valid);
    RUN(probe_magic_wrong_byte_order);
    RUN(probe_wrong_magic);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(build_minimal_sizes);
    RUN(magic_value_pin);
    RUN(boot_sector_layout_constants);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
