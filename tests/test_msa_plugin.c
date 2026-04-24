/**
 * @file test_msa_plugin.c
 * @brief MSA (Atari ST Magic Shadow Archiver) Plugin — probe tests.
 *
 * Self-contained per template from test_dc42_plugin.c.
 * Mirrors src/formats/msa/uft_msa.c header parse + signature check.
 *
 * Format: 10-byte header, BE16 fields:
 *   offset 0: signature 0x0E0F
 *   offset 2: sectors_per_track
 *   offset 4: sides
 *   offset 6: start_track
 *   offset 8: end_track
 * Track data follows, either uncompressed (sectors * 512 bytes per
 * track) or RLE-compressed with a length prefix.
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

#define MSA_SIGNATURE       0x0E0F
#define MSA_HEADER_SIZE     10

static uint16_t rd_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* Returns confidence 0..100. */
static int msa_probe_replica(const uint8_t *data, size_t size) {
    if (size < MSA_HEADER_SIZE) return 0;
    if (rd_be16(data) != MSA_SIGNATURE) return 0;

    uint16_t spt    = rd_be16(data + 2);
    uint16_t sides  = rd_be16(data + 4);
    uint16_t start  = rd_be16(data + 6);
    uint16_t end    = rd_be16(data + 8);

    /* Plausibility: classic ST has 9..11 sectors, 1 or 2 sides,
     * 79-80 tracks ending at track 79. */
    if (spt < 8 || spt > 16) return 0;
    if (sides != 0 && sides != 1) return 0;       /* 0=single, 1=double */
    if (end < start) return 0;
    if (end >= 256) return 0;
    return 95;
}

static void build_minimal_msa(uint8_t *out,
                               uint16_t sig_be16,
                               uint16_t spt,
                               uint16_t sides,
                               uint16_t start,
                               uint16_t end) {
    out[0] = (uint8_t)(sig_be16 >> 8);
    out[1] = (uint8_t)(sig_be16 & 0xFF);
    out[2] = (uint8_t)(spt >> 8);
    out[3] = (uint8_t)(spt & 0xFF);
    out[4] = (uint8_t)(sides >> 8);
    out[5] = (uint8_t)(sides & 0xFF);
    out[6] = (uint8_t)(start >> 8);
    out[7] = (uint8_t)(start & 0xFF);
    out[8] = (uint8_t)(end >> 8);
    out[9] = (uint8_t)(end & 0xFF);
}

TEST(probe_magic_valid_standard_st) {
    /* Classic ST 720K: 9 sectors, 2 sides (sides field=1), tracks 0..79. */
    uint8_t buf[MSA_HEADER_SIZE];
    build_minimal_msa(buf, MSA_SIGNATURE, 9, 1, 0, 79);
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_magic_valid_single_sided) {
    uint8_t buf[MSA_HEADER_SIZE];
    build_minimal_msa(buf, MSA_SIGNATURE, 9, 0, 0, 79);
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_wrong_signature) {
    uint8_t buf[MSA_HEADER_SIZE];
    build_minimal_msa(buf, 0x0F0E, 9, 1, 0, 79);    /* byte-swapped */
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_rejects_impossible_sides) {
    uint8_t buf[MSA_HEADER_SIZE];
    build_minimal_msa(buf, MSA_SIGNATURE, 9, 5, 0, 79);   /* 5 sides? */
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_rejects_impossible_sectors_per_track) {
    uint8_t buf[MSA_HEADER_SIZE];
    build_minimal_msa(buf, MSA_SIGNATURE, 99, 1, 0, 79);
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_rejects_end_before_start) {
    uint8_t buf[MSA_HEADER_SIZE];
    build_minimal_msa(buf, MSA_SIGNATURE, 9, 1, 79, 0);
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[MSA_HEADER_SIZE - 1];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x0E;
    buf[1] = 0x0F;
    ASSERT(msa_probe_replica(buf, sizeof(buf)) == 0);
}

int main(void) {
    printf("=== MSA plugin probe tests ===\n");
    RUN(probe_magic_valid_standard_st);
    RUN(probe_magic_valid_single_sided);
    RUN(probe_wrong_signature);
    RUN(probe_rejects_impossible_sides);
    RUN(probe_rejects_impossible_sectors_per_track);
    RUN(probe_rejects_end_before_start);
    RUN(probe_too_small);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
