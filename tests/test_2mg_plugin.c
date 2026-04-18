/**
 * @file test_2mg_plugin.c
 * @brief 2MG (Apple 2IMG container) Plugin — probe + header tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/apple/uft_2mg_parser.c probe logic.
 *
 * Format: "2IMG" magic at offset 0, 64-byte header.
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

#define IMG2_SIGNATURE   "2IMG"
#define IMG2_HEADER_SIZE 64

static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int img2_probe_replica(const uint8_t *data, size_t size) {
    if (size < IMG2_HEADER_SIZE) return 0;
    if (memcmp(data, IMG2_SIGNATURE, 4) == 0) return 95;
    return 0;
}

static size_t build_minimal_2mg(uint8_t *out, size_t cap,
                                 uint32_t n_blocks, uint16_t fmt) {
    if (cap < IMG2_HEADER_SIZE) return 0;
    memset(out, 0, IMG2_HEADER_SIZE);
    memcpy(out, IMG2_SIGNATURE, 4);
    memcpy(out + 4, "UFT ", 4);           /* creator (4 ASCII) */
    /* header_len at 8 (LE16) = 64 */
    out[8] = 64; out[9] = 0;
    /* version at 10 (LE16) = 1 */
    out[10] = 1; out[11] = 0;
    /* format at 12 (LE32): 0=DOS, 1=ProDOS, 2=NIB */
    out[12] = (uint8_t)fmt;
    /* n_blocks at 20 (LE32) for ProDOS */
    out[20] = (uint8_t)(n_blocks & 0xFF);
    out[21] = (uint8_t)((n_blocks >> 8) & 0xFF);
    out[22] = (uint8_t)((n_blocks >> 16) & 0xFF);
    out[23] = (uint8_t)((n_blocks >> 24) & 0xFF);
    return IMG2_HEADER_SIZE;
}

TEST(probe_signature_valid) {
    uint8_t buf[IMG2_HEADER_SIZE];
    build_minimal_2mg(buf, sizeof(buf), 280, 1);
    ASSERT(img2_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_signature_invalid) {
    uint8_t buf[IMG2_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "PDIM", 4);                /* wrong magic */
    ASSERT(img2_probe_replica(buf, sizeof(buf)) == 0);
    memcpy(buf, "2img", 4);                /* lowercase — must fail */
    ASSERT(img2_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[32] = "2IMG";
    ASSERT(img2_probe_replica(buf, 32) == 0);   /* < 64 byte header */
}

TEST(probe_empty_buffer) {
    ASSERT(img2_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(build_minimal_header_fields) {
    uint8_t buf[IMG2_HEADER_SIZE];
    size_t n = build_minimal_2mg(buf, sizeof(buf), 1600, 1);   /* 800K ProDOS */
    ASSERT(n == IMG2_HEADER_SIZE);
    ASSERT(memcmp(buf, "2IMG", 4) == 0);
    ASSERT(rd_le16(buf + 8)  == 64);     /* header_len */
    ASSERT(rd_le16(buf + 10) == 1);      /* version */
    ASSERT(buf[12] == 1);                 /* format=ProDOS */
    ASSERT(rd_le32(buf + 20) == 1600);   /* block count */
}

TEST(format_codes_dos_prodos_nib) {
    uint8_t buf[IMG2_HEADER_SIZE];
    for (uint16_t fmt = 0; fmt <= 2; fmt++) {
        build_minimal_2mg(buf, sizeof(buf), 280, fmt);
        ASSERT(buf[12] == fmt);
        ASSERT(img2_probe_replica(buf, sizeof(buf)) == 95);
    }
}

TEST(magic_ascii_pin) {
    ASSERT(IMG2_SIGNATURE[0] == '2');
    ASSERT(IMG2_SIGNATURE[1] == 'I');
    ASSERT(IMG2_SIGNATURE[2] == 'M');
    ASSERT(IMG2_SIGNATURE[3] == 'G');
    ASSERT(strlen(IMG2_SIGNATURE) == 4);
}

int main(void) {
    printf("=== 2MG (Apple 2IMG) Plugin Tests ===\n");
    RUN(probe_signature_valid);
    RUN(probe_signature_invalid);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(build_minimal_header_fields);
    RUN(format_codes_dos_prodos_nib);
    RUN(magic_ascii_pin);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
