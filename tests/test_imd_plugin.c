/**
 * @file test_imd_plugin.c
 * @brief IMD (Dave Dunfield ImageDisk) Plugin-B — probe tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/imd/uft_imd_plugin.c probe logic.
 *
 * Format: "IMD " (4 bytes incl. trailing space) + ASCII version +
 * comment terminated by 0x1A + track records.
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

#define IMD_SIG        "IMD "        /* note trailing space */
#define IMD_COMMENT_END 0x1A

static int imd_probe_replica(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    if (memcmp(data, IMD_SIG, 4) == 0) return 95;
    return 0;
}

/* Find comment-end byte (0x1A), return offset of the byte AFTER it.
 * Matches imd_skip_comment() in the real plugin. */
static size_t imd_skip_comment_replica(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        if (data[i] == IMD_COMMENT_END) return i + 1;
    return size;
}

static size_t build_minimal_imd(uint8_t *out, size_t cap) {
    if (cap < 32) return 0;
    memset(out, 0, 32);
    memcpy(out, "IMD 1.18: 01/01/2026 12:00:00", 29);
    out[29] = '\r';
    out[30] = '\n';
    out[31] = IMD_COMMENT_END;           /* comment ends immediately */
    return 32;
}

TEST(probe_signature_valid) {
    uint8_t buf[32];
    build_minimal_imd(buf, sizeof(buf));
    ASSERT(imd_probe_replica(buf, sizeof(buf)) == 95);
}

TEST(probe_signature_without_trailing_space) {
    uint8_t buf[32] = "IMDX";              /* missing the required space */
    ASSERT(imd_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_signature_lowercase) {
    uint8_t buf[32] = "imd ";
    ASSERT(imd_probe_replica(buf, sizeof(buf)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[3] = "IMD";
    ASSERT(imd_probe_replica(buf, 3) == 0);
}

TEST(probe_empty_buffer) {
    ASSERT(imd_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(comment_end_detection) {
    uint8_t buf[64];
    size_t n = build_minimal_imd(buf, sizeof(buf));
    size_t comment_end = imd_skip_comment_replica(buf, n);
    ASSERT(comment_end == 32);           /* one past the 0x1A at offset 31 */
}

TEST(comment_end_missing_returns_size) {
    uint8_t buf[16] = "IMD 1.18 no end";
    /* No 0x1A byte in the buffer. */
    size_t end = imd_skip_comment_replica(buf, 16);
    ASSERT(end == 16);                   /* entire size if 0x1A absent */
}

TEST(magic_ascii_pin) {
    ASSERT(IMD_SIG[0] == 'I');
    ASSERT(IMD_SIG[1] == 'M');
    ASSERT(IMD_SIG[2] == 'D');
    ASSERT(IMD_SIG[3] == ' ');           /* trailing space is REQUIRED */
    ASSERT(strlen(IMD_SIG) == 4);
    ASSERT(IMD_COMMENT_END == 0x1A);     /* Ctrl-Z (EOF under CP/M + DOS) */
}

int main(void) {
    printf("=== IMD Plugin Tests ===\n");
    RUN(probe_signature_valid);
    RUN(probe_signature_without_trailing_space);
    RUN(probe_signature_lowercase);
    RUN(probe_too_small);
    RUN(probe_empty_buffer);
    RUN(comment_end_detection);
    RUN(comment_end_missing_returns_size);
    RUN(magic_ascii_pin);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
