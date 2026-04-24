/**
 * @file test_xfd_plugin.c
 * @brief XFD (Atari 8-bit raw sector image) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/xfd/uft_xfd.c::uft_xfd_plugin_probe.
 *
 * Format: no header, pure sector data. Probe is size-based with four
 * canonical Atari shapes; for unknown-but-plausible sizes (multiple
 * of 128 or 256, under 256K) a lower confidence is returned so XFD
 * can still match ambiguous images with other plugins.
 *
 *   92160  = 40 tracks × 18 sectors × 128 bytes (SD 88k boot disk)
 *   133120 = 40 tracks × 26 sectors × 128 bytes (ED 128k)
 *   184320 = 40 tracks × 18 sectors × 256 bytes (DD 180k)
 *   266240 = 80 tracks × 26 sectors × 128 bytes (ED 2-sided variant)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static int xfd_probe_replica(size_t file_size) {
    /* Canonical shapes: confidence 40. */
    if (file_size == 92160u  ||
        file_size == 184320u ||
        file_size == 133120u ||
        file_size == 266240u) {
        return 40;
    }
    /* Fallback: non-zero, multiple of 128 or 256, under 256K → 25. */
    if (file_size == 0) return 0;
    if ((file_size % 128u) != 0 && (file_size % 256u) != 0) return 0;
    if (file_size > 266240u) return 0;
    return 25;
}

TEST(probe_sd_88k) {
    ASSERT(xfd_probe_replica(92160u) == 40);
}

TEST(probe_ed_128k) {
    ASSERT(xfd_probe_replica(133120u) == 40);
}

TEST(probe_dd_180k) {
    ASSERT(xfd_probe_replica(184320u) == 40);
}

TEST(probe_ed_256k_variant) {
    ASSERT(xfd_probe_replica(266240u) == 40);
}

TEST(probe_fallback_plausible_size) {
    /* 30 tracks × 18 × 128 = 69120 — arbitrary-but-plausible Atari
     * image. Not canonical, but a multiple of 128 and below the cap. */
    ASSERT(xfd_probe_replica(69120u) == 25);
}

TEST(probe_rejects_zero) {
    ASSERT(xfd_probe_replica(0) == 0);
}

TEST(probe_rejects_too_large) {
    /* Well over 266240 — must fall through. */
    ASSERT(xfd_probe_replica(1000000u) == 0);
}

TEST(probe_rejects_odd_size) {
    /* Neither multiple of 128 nor 256 → rejected. */
    ASSERT(xfd_probe_replica(100001u) == 0);
}

TEST(probe_accepts_multiple_of_256_only) {
    /* 50000 bytes: not a mult of 128 or 256 → reject.
     * Actually 50000 / 128 = 390.625 → not mult of 128.
     *         50000 / 256 = 195.3125  → not mult of 256.
     * So probe rejects. */
    ASSERT(xfd_probe_replica(50000u) == 0);

    /* 51200 = 200 × 256 → mult of 256 → fallback 25. */
    ASSERT(xfd_probe_replica(51200u) == 25);
}

int main(void) {
    printf("=== XFD plugin probe tests ===\n");
    RUN(probe_sd_88k);
    RUN(probe_ed_128k);
    RUN(probe_dd_180k);
    RUN(probe_ed_256k_variant);
    RUN(probe_fallback_plausible_size);
    RUN(probe_rejects_zero);
    RUN(probe_rejects_too_large);
    RUN(probe_rejects_odd_size);
    RUN(probe_accepts_multiple_of_256_only);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
