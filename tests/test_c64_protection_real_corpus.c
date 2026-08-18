/**
 * @file test_c64_protection_real_corpus.c
 * @brief POSITIVE control: C64 metrics vs really protected disks (MF-383).
 *
 * MF-381/382 verified the metric extractor against a freshly formatted disk:
 * the numbers were right and nothing false-fired. What that could NOT show is
 * whether the metrics actually discriminate — a detector that always reports
 * "clean" also passes a clean-disk test. This file supplies the missing half.
 *
 * Corpus (LOCAL-ONLY, tests/corpus/ is gitignored; game code is copyrighted):
 * two dumps from the C64 Preservation Project 10th Anniversary Collection,
 * which Pete Rittwage released for "no strings attached" download
 * (archive.org item C64_Preservation_Project_10th_Anniversary_Collection).
 * sha256 + exact path inside the collection are in the corpus manifest.
 *
 *   c64pp_bountybob.g64      Bounty Bob Strikes Back [Big Five, 1985]
 *                            PROTECTED: 71 populated half-track slots, of
 *                            which 35 are true half-tracks; 15 whole tracks
 *                            carry sync marks but no standard 1541 header.
 *   c64pp_aliensyndrome.g64  Alien Syndrome [Sega, 1987]
 *                            A real commercial disk with NO anomaly at all —
 *                            a stronger negative control than a self-formatted
 *                            image, because it is a genuine production disk.
 *
 * Ground truth pinned by independent python inspection BEFORE any UFT run.
 *
 * SKIPS (exit 77) when the local images are absent (e.g. CI).
 */

#include "uft/protection/ufm_c64_metrics.h"
#include "uft/protection/ufm_c64_scheme_detect.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_g64;

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img(const char *name) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_RESTRICTED_DIR, name);
    return p;
}

/** Collect whole-track metrics through the real plugin path. */
static int collect(const char *file, ufm_c64_track_metrics_t *out, int max_tracks) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    if (uft_format_plugin_g64.open(&disk, img(file), true) != UFT_OK) return -1;

    int n = 0;
    for (int cyl = 0; cyl < max_tracks; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        if (uft_format_plugin_g64.read_track(&disk, cyl, 0, &t) != UFT_OK) break;
        if (t.raw_data && t.raw_size > 0) {
            if (ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, cyl * 2,
                                         UFM_C64_SPEED_ZONE_AUTO, &out[n])) {
                n++;
            }
        }
        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);
    return n;
}

TEST(protected_disk_flags_exactly_the_pinned_tracks) {
    /* Pinned: whole tracks 1..10, 13..16 and 37 carry sync but no standard
     * header; every other populated whole track is ordinary. */
    static const int expect[] = {1,2,3,4,5,6,7,8,9,10,13,14,15,16,37};
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_bountybob.g64", m, 42);
    ASSERT(n > 30);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!m[i].has_custom_sync) continue;
        found++;
        bool expected = false;
        for (size_t k = 0; k < sizeof(expect) / sizeof(expect[0]); k++)
            if (m[i].track == expect[k]) { expected = true; break; }
        ASSERT(expected);                  /* no flag outside the pinned set */
    }
    ASSERT(found == (int)(sizeof(expect) / sizeof(expect[0])));
}

TEST(protected_disk_track1_is_short_and_headerless) {
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    ASSERT(collect("c64pp_bountybob.g64", m, 42) > 0);
    ASSERT(m[0].track == 1);
    ASSERT(m[0].sector_count == 0);         /* own format, no 1541 headers */
    ASSERT(m[0].sync_count == 4);           /* pinned */
    ASSERT(m[0].max_sync_run_bits == 30);   /* pinned */
    ASSERT(m[0].has_custom_sync);
    /* 6250 bytes where zone 3 nominally holds 7692 => clearly short */
    ASSERT(m[0].track_length_ratio > 0.81f && m[0].track_length_ratio < 0.82f);
}

TEST(protected_disk_produces_protection_hits) {
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_bountybob.g64", m, 42);
    ASSERT(n > 30);

    ufm_c64_prot_hit_t hits[128];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(m, n, hits, 128, &report));
    ASSERT(report.hits_written > 0);        /* the whole point of a positive control */
    ASSERT(report.confidence_0_100 > 0);

    int custom = 0;
    for (uint32_t i = 0; i < report.hits_written; i++)
        if (hits[i].type == UFM_PROT_CUSTOM_SYNC) custom++;
    ASSERT(custom == 15);                   /* one per pinned track */
}

TEST(real_protected_disk_is_described_not_named_PROT5) {
    /* Regression guard on real data for the PROT-5 fix (MF-402). With 15
     * custom-sync tracks the classifier used to take the `custom_syncs > 5`
     * branch and name the scheme "V-MAX!", derived purely from generic
     * structure with no V-MAX-specific evidence in the code path.
     *
     * It must now report the structure it measured. This says nothing about
     * which protection this disk actually uses — establishing that needs a
     * V-MAX-specific detector run against a disk of documented provenance. */
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_bountybob.g64", m, 42);
    ASSERT(n > 30);

    ufm_c64_prot_hit_t hits[128];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(m, n, hits, 128, &report));

    ASSERT(report.primary_scheme == UFM_PROT_CUSTOM_SYNC);
    ASSERT(report.hits_written > 0);
    for (uint32_t i = 0; i < report.hits_written; i++) {
        ASSERT(hits[i].type != UFM_PROT_VMAX);
        ASSERT(hits[i].type != UFM_PROT_RAPIDLOK);
    }
}

TEST(real_commercial_disk_without_protection_stays_silent) {
    /* Alien Syndrome: 40 tracks, all with standard 1541 headers. A production
     * disk that must not produce a single hit — the negative control that a
     * self-formatted image cannot provide. */
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_aliensyndrome.g64", m, 42);
    ASSERT(n == 40);

    for (int i = 0; i < n; i++) {
        ASSERT(!m[i].has_custom_sync);
        ASSERT(m[i].sector_count > 0);
        ASSERT(m[i].duplicate_ids == 0);
    }

    ufm_c64_prot_hit_t hits[128];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(m, n, hits, 128, &report));
    ASSERT(report.hits_written == 0);
    ASSERT(report.primary_scheme == UFM_PROT_NONE);
}

int main(void) {
    FILE *f = fopen(img("c64pp_bountybob.g64"), "rb");
    if (!f) {
        printf("SKIP: local C64 protection corpus absent (%s/c64pp_*.g64)\n"
               "      re-fetch per tests/corpus_manifest/manifest.json\n",
               UFT_CORPUS_RESTRICTED_DIR);
        return SKIP_EXIT;
    }
    fclose(f);
    printf("=== Positive control: C64 protection vs real disks (MF-383) ===\n");
    RUN(protected_disk_flags_exactly_the_pinned_tracks);
    RUN(protected_disk_track1_is_short_and_headerless);
    RUN(protected_disk_produces_protection_hits);
    RUN(real_protected_disk_is_described_not_named_PROT5);
    RUN(real_commercial_disk_without_protection_stays_silent);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
