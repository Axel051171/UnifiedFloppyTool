/* Cross-validation: link against UFT's gw encoder and compare against
 * the byte vectors produced by Greaseweazle's reference Python. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* We extracted the encoder logic from UFT verbatim — same constants,
 * same control flow as src/hal/uft_greaseweazle_full.c lines 1108–1148.
 * This is the EXACT implementation the user ships. */
#define UFT_GW_SAMPLE_FREQ_HZ  72000000u
#define GW_NFA_THRESH_US       150u
#define GW_NFA_PERIOD_US_X100  125u  /* 1.25us */
#define GW_FLUXOP_SPACE        0x02
#define GW_FLUXOP_ASTABLE      0x03

static void gw_write_n28(uint8_t* p, uint32_t x) {
    p[0] = 1 | ((x << 1) & 0xFF);
    p[1] = 1 | ((x >> 6) & 0xFF);
    p[2] = 1 | ((x >> 13) & 0xFF);
    p[3] = 1 | ((x >> 20) & 0xFF);
}

size_t uft_gw_encode_flux_stream(const uint32_t* samples, uint32_t sample_count,
                                 uint8_t* raw, size_t max_raw) {
    if (!samples || !raw || max_raw < 16) return 0;
    size_t pos = 0;
    uint32_t sf = UFT_GW_SAMPLE_FREQ_HZ;
    uint32_t nfa_thresh = (uint32_t)((uint64_t)GW_NFA_THRESH_US * sf / 1000000);
    uint32_t nfa_period = (uint32_t)((uint64_t)GW_NFA_PERIOD_US_X100 * sf / 100000000);
    if (nfa_period == 0) nfa_period = 1;
    uint32_t dummy_flux = (uint32_t)((uint64_t)100 * sf / 1000000);

    for (uint32_t si = 0; si <= sample_count && pos < max_raw - 12; si++) {
        uint32_t val = (si < sample_count) ? samples[si] : dummy_flux;
        if (val == 0) continue;
        else if (val < 250) raw[pos++] = (uint8_t)val;
        else if (val > nfa_thresh) {
            raw[pos++] = 0xFF; raw[pos++] = GW_FLUXOP_SPACE;
            gw_write_n28(raw + pos, val); pos += 4;
            raw[pos++] = 0xFF; raw[pos++] = GW_FLUXOP_ASTABLE;
            gw_write_n28(raw + pos, nfa_period); pos += 4;
        } else {
            uint32_t high = (val - 250) / 255;
            if (high < 5) {
                raw[pos++] = (uint8_t)(250 + high);
                raw[pos++] = (uint8_t)(1 + (val - 250) % 255);
            } else {
                raw[pos++] = 0xFF; raw[pos++] = GW_FLUXOP_SPACE;
                gw_write_n28(raw + pos, val - 249); pos += 4;
                raw[pos++] = 249;
            }
        }
    }
    if (pos < max_raw) raw[pos++] = 0x00;
    return pos;
}

static void run(const char* name, const uint32_t* vec, size_t n, const char* expected_hex) {
    uint8_t buf[256] = {0};
    size_t len = uft_gw_encode_flux_stream(vec, (uint32_t)n, buf, sizeof buf);
    char actual_hex[513] = {0};
    for (size_t i = 0; i < len && i*2+1 < sizeof(actual_hex); i++)
        sprintf(actual_hex + i*2, "%02x", buf[i]);
    int match = strcmp(actual_hex, expected_hex) == 0;
    printf("[%s] %s\n  expected: %s\n  actual:   %s\n",
           match ? "PASS" : "FAIL", name, expected_hex, actual_hex);
}

int main(void) {
    uint32_t v_small[]      = {100, 150, 200, 248, 249};
    uint32_t v_two_byte[]   = {250, 500, 1000, 1500, 1524};
    uint32_t v_three_form[] = {1525, 2000, 5000, 10000};
    uint32_t v_nfa[]        = {20000000};
    uint32_t v_mixed[]      = {100, 250, 1500, 2500, 100, 50, 300};
    uint32_t v_zeros[]      = {0, 100, 0, 200, 0};

    run("small",      v_small,      5, "6496c8f8f9ff024f6d0101f900");
    run("two_byte",   v_two_byte,   5, "fa01fafbfcf1fee7feffff024f6d0101f900");
    run("three_form", v_three_form, 4, "ff02f9130101f9ff02af1b0101f9ff021f4b0101f9ff022f990101f9ff024f6d0101f900");
    run("nfa",        v_nfa,        1, "ff0201b58913ff03b5010101ff024f6d0101f900");
    run("mixed",      v_mixed,      7, "64fa01fee7ff0297230101f96432fa33ff024f6d0101f900");
    run("zeros",      v_zeros,      5, "64c8ff024f6d0101f900");
    return 0;
}
