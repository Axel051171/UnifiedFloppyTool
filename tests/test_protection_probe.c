/**
 * @file test_protection_probe.c
 * @brief Content-based protection-anomaly scan (Phase-4 Klasse-3 detection).
 *
 * Links the real probe (src/analysis/uft_protection_probe.c), the SCP writer
 * and the SCP parser. Proves the probe reports what an image ACTUALLY carries
 * so a lossy-export warning is accurate in both directions:
 *   - an unprotected single-revolution SCP => no weak/multi-rev claim
 *   - a multi-revolution SCP with a diverging window => weak + multi-rev,
 *     with real counts
 *   - malformed input => error, never a crash or invented anomaly
 *
 * This is the detection half of the Klasse-3 work package: a sector-target
 * export can then warn "N weak-bit tracks will be lost" instead of the
 * generic count=0 placeholder that fired even for a plain disk.
 */

#include "uft/analysis/uft_protection_probe.h"
#include "uft/formats/uft_scp_writer.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_probe_%d.scp", dir, rand() % 100000);
}

/* Save a writer to a temp file, slurp the bytes back into *buf (caller frees).
   Returns byte count, 0 on failure. */
static size_t save_and_slurp(scp_writer_t *w, uint8_t **buf) {
    char path[256];
    get_temp_path(path, sizeof(path));
    if (scp_writer_save(w, path) != 0) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) { remove(path); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); remove(path); return 0; }
    *buf = (uint8_t *)malloc((size_t)n);
    size_t got = *buf ? fread(*buf, 1, (size_t)n, f) : 0;
    fclose(f);
    remove(path);
    if (got != (size_t)n) { free(*buf); *buf = NULL; return 0; }
    return got;
}

/* An unprotected disk: 3 tracks, one stable revolution each. */
TEST(unprotected_reports_no_anomaly) {
    const uint32_t flux[] = { 4000,4000,6000,4000,8000,4000,6000,4000 };
    const size_t n = sizeof(flux)/sizeof(flux[0]);
    uint32_t dur = 0; for (size_t i=0;i<n;i++) dur += flux[i];

    scp_writer_t *w = scp_writer_create(0x00, 1);   /* 1 revolution */
    ASSERT(w != NULL);
    for (int t = 0; t < 3; t++)
        ASSERT(scp_writer_add_track(w, t, 0, flux, n, dur, 0) == 0);

    uint8_t *bytes = NULL;
    size_t size = save_and_slurp(w, &bytes);
    scp_writer_free(w);
    ASSERT(size > 0 && bytes != NULL);

    uft_protection_summary_t s;
    ASSERT(uft_protection_probe_scp(bytes, size, &s) == UFT_OK);
    ASSERT(s.track_count == 3);
    ASSERT(s.max_revolutions == 1);
    ASSERT(s.has_multi_revolution == false);
    ASSERT(s.has_weak_regions == false);
    ASSERT(s.weak_track_count == 0);
    ASSERT(s.total_flux_transitions == (uint64_t)n * 3);
    free(bytes);
}

/* A protected disk: 3 revolutions, a middle window that diverges per rev. */
TEST(weakbit_multirev_reports_anomaly) {
    #define FLEN 12
    static const uint32_t rev[3][FLEN] = {
        { 4000,4000,6000,4000,  2000,4000,2000,4000,  8000,4000,6000,4000 },
        { 4000,4000,6000,4000,  4000,2000,6000,2000,  8000,4000,6000,4000 },
        { 4000,4000,6000,4000,  6000,6000,4000,8000,  8000,4000,6000,4000 },
    };
    scp_writer_t *w = scp_writer_create(0x00, 3);
    ASSERT(w != NULL);
    for (int r = 0; r < 3; r++) {
        uint32_t dur = 0; for (int i=0;i<FLEN;i++) dur += rev[r][i];
        ASSERT(scp_writer_add_track(w, 0, 0, rev[r], FLEN, dur, r) == 0);
    }

    uint8_t *bytes = NULL;
    size_t size = save_and_slurp(w, &bytes);
    scp_writer_free(w);
    ASSERT(size > 0 && bytes != NULL);

    uft_protection_summary_t s;
    ASSERT(uft_protection_probe_scp(bytes, size, &s) == UFT_OK);
    ASSERT(s.track_count == 1);
    ASSERT(s.max_revolutions == 3);
    ASSERT(s.has_multi_revolution == true);
    ASSERT(s.has_weak_regions == true);
    ASSERT(s.weak_track_count == 1);
    free(bytes);
    #undef FLEN
}

/* A stable multi-revolution disk (revolutions identical) must NOT be flagged
   weak — only genuine cross-rev divergence counts. */
TEST(stable_multirev_not_weak) {
    const uint32_t flux[] = { 4000,4000,6000,4000,8000,4000 };
    const size_t n = sizeof(flux)/sizeof(flux[0]);
    uint32_t dur = 0; for (size_t i=0;i<n;i++) dur += flux[i];

    scp_writer_t *w = scp_writer_create(0x00, 3);
    ASSERT(w != NULL);
    for (int r = 0; r < 3; r++)                    /* same flux each rev */
        ASSERT(scp_writer_add_track(w, 0, 0, flux, n, dur, r) == 0);

    uint8_t *bytes = NULL;
    size_t size = save_and_slurp(w, &bytes);
    scp_writer_free(w);
    ASSERT(size > 0 && bytes != NULL);

    uft_protection_summary_t s;
    ASSERT(uft_protection_probe_scp(bytes, size, &s) == UFT_OK);
    ASSERT(s.has_multi_revolution == true);        /* 3 revs present */
    ASSERT(s.has_weak_regions == false);           /* but they agree */
    ASSERT(s.weak_track_count == 0);
    free(bytes);
}

/* Motor jitter makes real revolutions differ in transition count. The probe
   deliberately OVER-reports (flags this weak) rather than risk hiding a real
   weak-bit loss — this pins that safe-direction contract so a future change
   can't silently turn it into under-reporting. */
TEST(jitter_overreports_weak_safely) {
    const uint32_t r0[] = { 4000,4000,6000,4000,8000,4000 };       /* 6 */
    const uint32_t r1[] = { 4000,4000,6000,4000,8000,4000,4000 };  /* 7 (one more) */
    uint32_t d0=0; for (size_t i=0;i<6;i++) d0+=r0[i];
    uint32_t d1=0; for (size_t i=0;i<7;i++) d1+=r1[i];

    scp_writer_t *w = scp_writer_create(0x00, 2);
    ASSERT(w != NULL);
    ASSERT(scp_writer_add_track(w, 0, 0, r0, 6, d0, 0) == 0);
    ASSERT(scp_writer_add_track(w, 0, 0, r1, 7, d1, 1) == 0);

    uint8_t *bytes = NULL;
    size_t size = save_and_slurp(w, &bytes);
    scp_writer_free(w);
    ASSERT(size > 0 && bytes != NULL);

    uft_protection_summary_t s;
    ASSERT(uft_protection_probe_scp(bytes, size, &s) == UFT_OK);
    ASSERT(s.has_multi_revolution == true);
    ASSERT(s.has_weak_regions == true);            /* over-report, safe direction */
    free(bytes);
}

TEST(rejects_bad_args) {
    uft_protection_summary_t s;
    uint8_t dummy[4] = {0};
    ASSERT(uft_protection_probe_scp(NULL, 10, &s) == UFT_ERR_NULL_POINTER);
    ASSERT(uft_protection_probe_scp(dummy, 4, NULL) == UFT_ERR_NULL_POINTER);
    /* non-SCP garbage: no readable track -> corrupted, not a crash */
    uint8_t junk[64];
    memset(junk, 0xAB, sizeof(junk));
    ASSERT(uft_protection_probe_scp(junk, sizeof(junk), &s) == UFT_ERR_CORRUPTED);
}

int main(void) {
    printf("=== protection-anomaly content probe (Klasse-3 detection) ===\n");
    RUN(unprotected_reports_no_anomaly);
    RUN(weakbit_multirev_reports_anomaly);
    RUN(stable_multirev_not_weak);
    RUN(jitter_overreports_weak_safely);
    RUN(rejects_bad_args);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
