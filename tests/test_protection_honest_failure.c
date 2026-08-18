/**
 * @file test_protection_honest_failure.c
 * @brief Pins that unimplemented detectors FAIL instead of inventing a finding.
 *
 * MF-384. Six functions used to report success while never looking at their
 * input, so every caller received a fabricated forensic answer:
 *
 *   uft_c64_scan_all_protection()  -> "no protection", confidence 0.0
 *   uft_c64_scan_fat_tracks()      -> "no fat tracks"
 *   uft_rapidlok_scan_disk()       -> "no RapidLok"
 *   uft_ir_detect_weak_bits()      -> "no weak bits"
 *   uft_protection_detect_pirateslayer() -> "not detected"
 *   uft_speedlock_write()          -> a hand-invented 16 KB track
 *
 * None of them had callers, so nothing was visibly wrong — which is exactly
 * why a test is needed: the next caller would have been the first victim.
 * DESIGN_PRINCIPLES: "Keine erfundenen Daten." A missing implementation must
 * fail; it must never answer the question it cannot answer.
 *
 * These asserts are deliberately written so that IMPLEMENTING any of these
 * functions turns the test red — that is the signal to replace the assert with
 * a real behavioural test, not to delete it.
 */

#include "uft/protection/uft_c64_protection_enhanced.h"
#include "uft/protection/uft_speedlock.h"
#include "uft/uft_ir_format.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* uft_rapidlok.h lives under src/, declare what we need locally. */
int uft_protection_detect_pirateslayer(const uint8_t *track_data,
                                       size_t track_size, uint8_t track_num,
                                       void *result);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(scan_all_protection_fails_instead_of_claiming_clean) {
    uint8_t fake_image[512];
    memset(fake_image, 0xAA, sizeof(fake_image));
    uft_c64_protection_scan_t scan;
    memset(&scan, 0xEE, sizeof(scan));

    /* Must NOT return 0 — a zero here would mean "scanned, found nothing". */
    ASSERT(uft_c64_scan_all_protection(fake_image, &scan) != 0);
    /* And it must not leave a confident-looking verdict behind. */
    ASSERT(scan.has_protection == false);
    ASSERT(scan.overall_confidence == 0.0f);
}

TEST(scan_fat_tracks_fails_instead_of_claiming_none) {
    uint8_t fake_image[512];
    memset(fake_image, 0x55, sizeof(fake_image));
    uft_fat_track_result_t results[4];
    size_t found = 99;

    ASSERT(uft_c64_scan_fat_tracks(fake_image, results, 4, &found) != 0);
    ASSERT(found == 0);
}

TEST(ir_weak_bit_detector_fails_instead_of_claiming_none) {
    uft_ir_track_t track;
    memset(&track, 0, sizeof(track));
    track.revolution_count = 3;          /* enough revolutions to be asked */
    track.weak_region_count = 77;

    ASSERT(uft_ir_detect_weak_bits(&track) != 0);
    ASSERT(track.weak_region_count == 0);  /* no invented regions either */
}

TEST(speedlock_write_refuses_to_fabricate_a_track) {
    uft_speedlock_recon_params_t params;
    memset(&params, 0, sizeof(params));
    uint8_t out[16384];
    memset(out, 0x11, sizeof(out));
    uint32_t bits = 12345;

    ASSERT(uft_speedlock_write(&params, out, &bits, NULL) != 0);
    ASSERT(bits == 0);
    /* The buffer must be untouched — no 0x4E gap fill, no invented syncs. */
    for (size_t i = 0; i < sizeof(out); i++) {
        ASSERT(out[i] == 0x11);
    }
}

TEST(pirateslayer_fails_instead_of_reporting_absent) {
    uint8_t track[256];
    memset(track, 0x00, sizeof(track));
    uint8_t result[512];              /* opaque to this TU, size is generous */
    memset(result, 0, sizeof(result));

    /* 0 would mean "scheme not present" in this file's convention. */
    ASSERT(uft_protection_detect_pirateslayer(track, sizeof(track), 18, result) != 0);
}

TEST(null_arguments_are_still_rejected) {
    uft_c64_protection_scan_t scan;
    ASSERT(uft_c64_scan_all_protection(NULL, &scan) != 0);
    ASSERT(uft_c64_scan_all_protection(&scan, NULL) != 0);
    ASSERT(uft_ir_detect_weak_bits(NULL) != 0);
}

int main(void) {
    printf("=== Honest failure of unimplemented detectors (MF-384) ===\n");
    RUN(scan_all_protection_fails_instead_of_claiming_clean);
    RUN(scan_fat_tracks_fails_instead_of_claiming_none);
    RUN(ir_weak_bit_detector_fails_instead_of_claiming_none);
    RUN(speedlock_write_refuses_to_fabricate_a_track);
    RUN(pirateslayer_fails_instead_of_reporting_absent);
    RUN(null_arguments_are_still_rejected);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
