/*
 * test_gw_encoder.c — Greaseweazle flux-encoder byte-exact regression test.
 *
 * GW-F2 (tests/external_audits/gw/REPORT.md): promote the encoder-vector
 * cross-check into the regular CI suite.
 *
 * Unlike the original audit harness (tests/external_audits/gw/
 * test_encode_match.c), this test links against the REAL
 * uft_gw_encode_flux_stream() from src/hal/uft_greaseweazle_full.c —
 * not a verbatim copy. A copy silently drifts: the audit copy still used
 * the pre-MF-179 4-argument signature; the real function gained a
 * `sample_freq_hz` parameter (GW-F3). Linking the real symbol makes this
 * a true regression gate — any change to the shipped encoder that breaks
 * Greaseweazle wire compatibility fails the build.
 *
 * The six reference vectors + expected byte streams were produced from
 * Greaseweazle's own reference Python `_encode_flux` (keirf/greaseweazle)
 * and frozen in the GW compatibility audit (2026-05-14). They cover every
 * encoder branch: <250 direct, 250-1524 two-byte, >=1525 three-form with
 * Space opcode, NFA (>150us) with Space+Astable, mixed, and zero-skipping.
 *
 * sample_freq_hz is passed as 0 → the function falls back to the 72 MHz
 * F7 default (UFT_GW_SAMPLE_FREQ_HZ), which is the frequency the frozen
 * reference vectors were generated at.
 *
 * Exit code: 0 = all vectors byte-identical, 1 = at least one mismatch.
 * (The audit harness always returned 0 — a real FAIL would not have
 *  failed ctest. This version returns 1 on any mismatch.)
 */
#include "uft/hal/uft_greaseweazle_full.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int run(const char *name, const uint32_t *vec, uint32_t n,
               const char *expected_hex)
{
    uint8_t buf[256];
    memset(buf, 0, sizeof buf);

    /* sample_freq_hz = 0 → 72 MHz F7 default, matching the frozen vectors. */
    size_t len = uft_gw_encode_flux_stream(vec, n, buf, sizeof buf, 0u);

    char actual_hex[2 * sizeof(buf) + 1];
    memset(actual_hex, 0, sizeof actual_hex);
    for (size_t i = 0; i < len && i * 2 + 1 < sizeof(actual_hex); i++)
        sprintf(actual_hex + i * 2, "%02x", buf[i]);

    int match = (strcmp(actual_hex, expected_hex) == 0);
    printf("[%s] %s\n", match ? "PASS" : "FAIL", name);
    if (!match) {
        printf("  expected: %s\n", expected_hex);
        printf("  actual:   %s\n", actual_hex);
    }
    return match ? 0 : 1;
}

int main(void)
{
    uint32_t v_small[]      = {100, 150, 200, 248, 249};
    uint32_t v_two_byte[]   = {250, 500, 1000, 1500, 1524};
    uint32_t v_three_form[] = {1525, 2000, 5000, 10000};
    uint32_t v_nfa[]        = {20000000};
    uint32_t v_mixed[]      = {100, 250, 1500, 2500, 100, 50, 300};
    uint32_t v_zeros[]      = {0, 100, 0, 200, 0};

    int failures = 0;
    failures += run("small",      v_small,      5,
                    "6496c8f8f9ff024f6d0101f900");
    failures += run("two_byte",   v_two_byte,   5,
                    "fa01fafbfcf1fee7feffff024f6d0101f900");
    failures += run("three_form", v_three_form, 4,
                    "ff02f9130101f9ff02af1b0101f9ff021f4b0101f9"
                    "ff022f990101f9ff024f6d0101f900");
    failures += run("nfa",        v_nfa,        1,
                    "ff0201b58913ff03b5010101ff024f6d0101f900");
    failures += run("mixed",      v_mixed,      7,
                    "64fa01fee7ff0297230101f96432fa33ff024f6d0101f900");
    failures += run("zeros",      v_zeros,      5,
                    "64c8ff024f6d0101f900");

    if (failures) {
        printf("\n%d/6 encoder vectors FAILED — Greaseweazle wire "
               "compatibility is broken.\n", failures);
        return 1;
    }
    printf("\nAll 6 encoder vectors byte-identical to the frozen "
           "Greaseweazle reference.\n");
    return 0;
}
