/**
 * @file test_hfe_plugin.c
 * @brief HFE (HxC Floppy Emulator) Plugin-B — probe + header tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/hfe/uft_hfe.c probe logic.
 *
 * Magic: "HXCPICFE" (v1) or "HXCHFEV3" (v3).
 * Header: 512 bytes packed, struct hfe_header_t.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-32s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ═════════════════════════════════════════════════════════════════════════
 * Constants — mirror src/formats/hfe/uft_hfe.c
 * ═════════════════════════════════════════════════════════════════════════ */

#define HFE_SIGNATURE     "HXCPICFE"
#define HFE_SIGNATURE_V3  "HXCHFEV3"
#define HFE_HEADER_SIZE   512
#define HFE_BLOCK_SIZE    512
#define HFE_MAX_TRACKS    84

static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Replica of hfe_probe() — returns confidence 0..100. */
static int hfe_probe_replica(const uint8_t *data, size_t size) {
    if (size < HFE_HEADER_SIZE) return 0;
    int conf = 0;
    if (memcmp(data, HFE_SIGNATURE, 8) == 0)          conf = 95;
    else if (memcmp(data, HFE_SIGNATURE_V3, 8) == 0)  conf = 95;
    else return 0;

    uint8_t n_tracks = data[9];
    uint8_t n_sides  = data[10];
    uint16_t bitrate = rd_le16(data + 12);

    if (n_tracks > 0 && n_tracks <= HFE_MAX_TRACKS) conf += 2;
    if (n_sides >= 1 && n_sides <= 2)               conf += 2;
    if (bitrate >= 125 && bitrate <= 1000)          conf += 1;
    if (conf > 100) conf = 100;
    return conf;
}

/* Build minimal HFE file (512-byte header + empty track LUT block). */
static size_t build_minimal_hfe(uint8_t *out, size_t out_cap,
                                 const char *sig8, uint8_t n_tracks,
                                 uint8_t n_sides, uint16_t bitrate) {
    if (out_cap < HFE_HEADER_SIZE) return 0;
    memset(out, 0xFF, HFE_HEADER_SIZE);   /* 0xFF default for pad */
    memcpy(out, sig8, 8);
    out[8]  = 0;           /* format_revision */
    out[9]  = n_tracks;
    out[10] = n_sides;
    out[11] = 0;           /* track_encoding = ISOIBM_MFM */
    out[12] = (uint8_t)(bitrate & 0xFF);
    out[13] = (uint8_t)(bitrate >> 8);
    out[14] = 0; out[15] = 0;  /* rpm = 0 → default 300 */
    out[16] = 0;           /* interface = IBMPC_DD */
    out[17] = 0x01;        /* reserved */
    out[18] = 1; out[19] = 0;  /* track_list_offset = 1 block */
    return HFE_HEADER_SIZE;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Tests
 * ═════════════════════════════════════════════════════════════════════════ */

TEST(probe_signature_v1) {
    uint8_t buf[HFE_HEADER_SIZE];
    build_minimal_hfe(buf, sizeof(buf), HFE_SIGNATURE, 80, 2, 250);
    int conf = hfe_probe_replica(buf, sizeof(buf));
    ASSERT(conf >= 95);
}

TEST(probe_signature_v3) {
    uint8_t buf[HFE_HEADER_SIZE];
    build_minimal_hfe(buf, sizeof(buf), HFE_SIGNATURE_V3, 80, 2, 500);
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) >= 95);
}

TEST(probe_signature_invalid_magic) {
    uint8_t buf[HFE_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "HFE-1234", 8);
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) == 0);

    memcpy(buf, "GCR-1541", 8);
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[64] = "HXCPICFE";   /* valid magic but only 64 bytes */
    ASSERT(hfe_probe_replica(buf, 64) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(hfe_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(build_minimal_header) {
    uint8_t buf[HFE_HEADER_SIZE];
    size_t n = build_minimal_hfe(buf, sizeof(buf), HFE_SIGNATURE, 80, 2, 250);
    ASSERT(n == HFE_HEADER_SIZE);
    ASSERT(memcmp(buf, HFE_SIGNATURE, 8) == 0);
    ASSERT(buf[9]  == 80);
    ASSERT(buf[10] == 2);
    ASSERT(rd_le16(buf + 12) == 250);
}

TEST(probe_confidence_tiered) {
    uint8_t buf[HFE_HEADER_SIZE];

    /* All fields invalid → only signature match = 95 */
    memset(buf, 0, sizeof(buf));
    memcpy(buf, HFE_SIGNATURE, 8);
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) == 95);

    /* + valid tracks (80 ≤ 84) → +2 = 97 */
    buf[9] = 80;
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) == 97);

    /* + valid sides (2) → +2 = 99 */
    buf[10] = 2;
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) == 99);

    /* + valid bitrate (250) → +1 = 100 */
    buf[12] = 250; buf[13] = 0;
    ASSERT(hfe_probe_replica(buf, sizeof(buf)) == 100);
}

TEST(hxcpicfe_ascii_pin) {
    ASSERT(strlen(HFE_SIGNATURE) == 8);
    ASSERT(HFE_SIGNATURE[0] == 'H');
    ASSERT(HFE_SIGNATURE[1] == 'X');
    ASSERT(HFE_SIGNATURE[2] == 'C');
    ASSERT(HFE_SIGNATURE[7] == 'E');
    ASSERT(strlen(HFE_SIGNATURE_V3) == 8);
    ASSERT(HFE_SIGNATURE_V3[6] == 'V');
    ASSERT(HFE_SIGNATURE_V3[7] == '3');
}

int main(void) {
    printf("=== HFE Plugin Tests ===\n");
    RUN(probe_signature_v1);
    RUN(probe_signature_v3);
    RUN(probe_signature_invalid_magic);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(build_minimal_header);
    RUN(probe_confidence_tiered);
    RUN(hxcpicfe_ascii_pin);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
