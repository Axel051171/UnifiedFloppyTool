/**
 * @file test_ipf_plugin.c
 * @brief IPF (CAPS/SPS) Plugin-B — probe + magic tests.
 *
 * Self-contained per template from test_stx_plugin.c.
 * Mirrors src/formats/ipf/uft_ipf_plugin.c probe logic.
 *
 * Format: "CAPS" magic at offset 0, chunk-based after that.
 * Notes: read_track currently returns NOT_IMPLEMENTED (needs libcapsimage
 * or the CAPS parser to be wired — plugin only does identification).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ═════════════════════════════════════════════════════════════════════════
 * Constants — mirror src/formats/ipf/uft_ipf_plugin.c
 * ═════════════════════════════════════════════════════════════════════════ */

#define IPF_MAGIC        "CAPS"
#define IPF_MIN_SIZE     12          /* minimum for probe */

/* Replica of ipf_plugin_probe() — simplified, doesn't chain through
 * uft_caps_is_ipf fallback (which checks secondary signatures). */
static int ipf_probe_replica(const uint8_t *data, size_t size) {
    if (size < IPF_MIN_SIZE) return 0;
    if (data[0] == 'C' && data[1] == 'A' && data[2] == 'P' && data[3] == 'S')
        return 95;
    return 0;
}

/* Build a minimal valid IPF stub — 4-byte magic + a CAPS info record header.
 * Real IPF files have CAPS→INFO→IMGE→… chunk chain; we only need the probe
 * to accept, not the full parse. */
static size_t build_minimal_ipf(uint8_t *out, size_t out_cap) {
    if (out_cap < 32) return 0;
    memset(out, 0, 32);
    memcpy(out, "CAPS", 4);
    /* Chunk layout: name(4) + length(4 BE) + CRC(4 BE) + data(...) */
    out[4] = 0; out[5] = 0; out[6] = 0; out[7] = 12;   /* length = 12 (chunk header only) */
    /* CRC at 8..11 omitted; not probe-relevant. */
    return 32;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Tests
 * ═════════════════════════════════════════════════════════════════════════ */

TEST(probe_signature_valid) {
    uint8_t buf[32];
    size_t n = build_minimal_ipf(buf, sizeof(buf));
    ASSERT(n > 0);
    ASSERT(ipf_probe_replica(buf, n) == 95);
}

TEST(probe_signature_invalid_magic) {
    uint8_t buf[32] = "IPF1";           /* wrong magic */
    ASSERT(ipf_probe_replica(buf, sizeof(buf)) == 0);

    uint8_t sps[32] = "SPS ";           /* SPS is a related but different container */
    ASSERT(ipf_probe_replica(sps, sizeof(sps)) == 0);

    /* Almost-CAPS (one byte off): */
    uint8_t capx[32] = "CAPX";
    ASSERT(ipf_probe_replica(capx, sizeof(capx)) == 0);
}

TEST(probe_too_small) {
    uint8_t buf[8] = "CAPS";
    /* 8 bytes < 12 minimum, even though magic is present */
    ASSERT(ipf_probe_replica(buf, 8) == 0);
}

TEST(probe_exact_min_size) {
    uint8_t buf[12] = "CAPS";
    /* 12 bytes is the minimum that passes */
    ASSERT(ipf_probe_replica(buf, 12) == 95);
}

TEST(probe_empty_buffer) {
    ASSERT(ipf_probe_replica((const uint8_t *)"", 0) == 0);
}

TEST(build_minimal_caps_chunk) {
    uint8_t buf[32];
    size_t n = build_minimal_ipf(buf, sizeof(buf));
    ASSERT(n == 32);
    ASSERT(memcmp(buf, "CAPS", 4) == 0);
    /* Chunk length field at offset 4-7 (big-endian per IPF spec). */
    ASSERT(buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] == 12);
}

TEST(caps_ascii_pin) {
    const char *s = IPF_MAGIC;
    ASSERT((uint8_t)s[0] == 0x43);      /* 'C' */
    ASSERT((uint8_t)s[1] == 0x41);      /* 'A' */
    ASSERT((uint8_t)s[2] == 0x50);      /* 'P' */
    ASSERT((uint8_t)s[3] == 0x53);      /* 'S' */
    ASSERT(strlen(IPF_MAGIC) == 4);
}

TEST(probe_case_sensitive) {
    /* Magic must be uppercase "CAPS" — lowercase variant must NOT match. */
    uint8_t lower[32] = "caps";
    ASSERT(ipf_probe_replica(lower, sizeof(lower)) == 0);

    uint8_t mixed[32] = "Caps";
    ASSERT(ipf_probe_replica(mixed, sizeof(mixed)) == 0);
}

int main(void) {
    printf("=== IPF Plugin Tests ===\n");
    RUN(probe_signature_valid);
    RUN(probe_signature_invalid_magic);
    RUN(probe_too_small);
    RUN(probe_exact_min_size);
    RUN(probe_empty_buffer);
    RUN(build_minimal_caps_chunk);
    RUN(caps_ascii_pin);
    RUN(probe_case_sensitive);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
